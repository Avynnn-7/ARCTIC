#include "live_latency.hpp"
#include "tsc_clock.hpp"
#include <cmath>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <intrin.h>
#else
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <x86intrin.h>
#endif

namespace arctic {

LiveLatency::LiveLatency(const std::string& target_ip, int target_port, size_t buffer_capacity)
    : target_ip_(target_ip), target_port_(target_port), running_(false),
      rtt_buffer_(buffer_capacity), mu_(-4.0), sigma_(0.5), sample_count_(0) {
}

LiveLatency::~LiveLatency() {
    stop();
}

void LiveLatency::start() {
    if (running_.exchange(true)) return;
    
    udp_thread_ = std::thread(&LiveLatency::udp_measurement_loop, this);
    mle_thread_ = std::thread(&LiveLatency::mle_fitting_loop, this);
}

void LiveLatency::stop() {
    if (!running_.exchange(false)) return;
    
    if (udp_thread_.joinable()) udp_thread_.join();
    if (mle_thread_.joinable()) mle_thread_.join();
}

double LiveLatency::get_time_ns() const {
    // read rdtscp direct. caller needs to do the delta math
    unsigned int aux;
    return static_cast<double>(__rdtscp(&aux));
}

void LiveLatency::udp_measurement_loop() {
    // pinning to core 2 because core 0 handles all the OS interrupt noise
    // gotta isolate this properly in prod
#ifdef _WIN32
    SetThreadAffinityMask(GetCurrentThread(), (1ULL << 2)); // Core 2
#else
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(2, &cpuset); // Core 2 — avoid Core 0 (IRQ target)
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif

    // tsc calib
    TscClock tsc;
    double ticks_per_ns = tsc.get_ticks_per_ns();

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) return;
#else
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;
#endif

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<uint16_t>(target_port_));
    inet_pton(AF_INET, target_ip_.c_str(), &server_addr.sin_addr);

    char send_buf[64] = "ARCTIC_PING";
    char recv_buf[64];

#ifdef _WIN32
    // windows fallback. select() is gross but it works here
    while (running_.load(std::memory_order_relaxed)) {
        uint64_t tsc_send = TscClock::rdtscp();

        sendto(sock, send_buf, sizeof(send_buf), 0,
               (struct sockaddr*)&server_addr, sizeof(server_addr));

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        struct timeval tv = {0, 5000}; // 5ms
        int sel = select(0, &read_fds, nullptr, nullptr, &tv);

        if (sel > 0) {
            int bytes = recv(sock, recv_buf, sizeof(recv_buf), 0);
            if (bytes > 0) {
                uint64_t tsc_recv = TscClock::rdtscp();
                double rtt_seconds = static_cast<double>(tsc_recv - tsc_send) 
                                     / (ticks_per_ns * 1e9);
                rtt_buffer_.push(rtt_seconds);
            }
        }
    }
    closesocket(sock);
    WSACleanup();

#else
    // using epoll because it's better than select.
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        close(sock);
        return;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sock;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev);

    struct epoll_event events[1];

    while (running_.load(std::memory_order_relaxed)) {
        uint64_t tsc_send = TscClock::rdtscp();

        sendto(sock, send_buf, sizeof(send_buf), 0,
               (struct sockaddr*)&server_addr, sizeof(server_addr));

        // wait max 5ms so we don't cook the cpu
        int nfds = epoll_wait(epfd, events, 1, 5);

        if (nfds > 0) {
            int bytes = recv(sock, recv_buf, sizeof(recv_buf), 0);
            if (bytes > 0) {
                uint64_t tsc_recv = TscClock::rdtscp();
                double rtt_seconds = static_cast<double>(tsc_recv - tsc_send)
                                     / (ticks_per_ns * 1e9);
                rtt_buffer_.push(rtt_seconds);
            }
        }
        // epoll timeout handles the pacing
    }

    close(epfd);
    close(sock);
#endif
}

void LiveLatency::mle_fitting_loop() {
    // welford math for log-normal variance
    size_t count = 0;
    double mean_log = 0.0;
    double m2_log = 0.0;
    
    while (running_.load(std::memory_order_relaxed)) {
        double rtt_seconds = 0.0;
        
        while (rtt_buffer_.pop(rtt_seconds)) {
            if (rtt_seconds <= 0) continue;
            
            double log_rtt = std::log(rtt_seconds);
            count++;
            
            double delta = log_rtt - mean_log;
            mean_log += delta / static_cast<double>(count);
            double delta2 = log_rtt - mean_log;
            m2_log += delta * delta2;
            
            if (count > 1) {
                double var_log = m2_log / static_cast<double>(count - 1);
                double stddev_log = std::sqrt(var_log);
                
                mu_.store(mean_log, std::memory_order_release);
                sigma_.store(stddev_log, std::memory_order_release);
                sample_count_.store(count, std::memory_order_release);
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

} // namespace arctic
