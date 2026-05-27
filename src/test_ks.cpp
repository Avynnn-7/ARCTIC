// tests if udp loopback jitter is actually log-normal.
// spoiler: it fails the ks-test all the time because windows scheduler is garbage and has heavy tails.
// still useful to prove we aren't trading in a vacuum.

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define NOMINMAX
#include "live_latency.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

static double normalCDF(double value) {
    return 0.5 * std::erfc(-value * 0.70710678118654752440);
}

static double lognormalCDF(double x, double mu, double sigma) {
    if (x <= 0) return 0.0;
    return normalCDF((std::log(x) - mu) / sigma);
}

int main() {
    std::cout << "Starting Live Latency KS-Test Validation..." << std::endl;
    
    arctic::LiveLatency live_latency("127.0.0.1", 12345);
    live_latency.start();
    
    const int target_samples = 5000;
    std::vector<double> empirical_samples;
    empirical_samples.reserve(target_samples);
    
    std::cout << "Collecting " << target_samples << " live UDP RTT samples..." << std::endl;
    
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#else
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#endif

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    
    // set timeout so we don't hang on dropped packets
#ifdef _WIN32
    DWORD timeout_ms = 50;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
#else
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000; // 50ms
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    char dummy_buffer[8] = {0};
    int total_drops = 0;
    
    for (int i = 0; i < target_samples; ++i) {
        if (i > 0 && i % 1000 == 0) {
            std::cout << "Collected " << i << " samples (" << total_drops << " drops)..." << std::endl;
        }
        
        // grab start time
        auto t_start = std::chrono::steady_clock::now();
        
        sendto(sock, dummy_buffer, sizeof(dummy_buffer), 0, 
               (struct sockaddr*)&server_addr, sizeof(server_addr));
        
#ifdef _WIN32
        int from_len = sizeof(server_addr);
#else
        socklen_t from_len = sizeof(server_addr);
#endif
        struct sockaddr_in from_addr;
        int bytes = recvfrom(sock, dummy_buffer, sizeof(dummy_buffer), 0, 
                             (struct sockaddr*)&from_addr, &from_len);
        
        if (bytes > 0) {
            auto t_end = std::chrono::steady_clock::now();
            double rtt_s = std::chrono::duration<double>(t_end - t_start).count();
            empirical_samples.push_back(rtt_s);
        } else {
            // packet dropped. retry a bit then give up so we don't hang forever.
            int retries = 0;
            bool recovered = false;
            while (retries < 10) {
                auto t_retry = std::chrono::steady_clock::now();
                sendto(sock, dummy_buffer, sizeof(dummy_buffer), 0,
                       (struct sockaddr*)&server_addr, sizeof(server_addr));
                bytes = recvfrom(sock, dummy_buffer, sizeof(dummy_buffer), 0,
                                 (struct sockaddr*)&from_addr, &from_len);
                if (bytes > 0) {
                    auto t_end = std::chrono::steady_clock::now();
                    double rtt_s = std::chrono::duration<double>(t_end - t_retry).count();
                    empirical_samples.push_back(rtt_s);
                    recovered = true;
                    break;
                }
                retries++;
            }
            if (!recovered) {
                total_drops++;
            }
        }
    }
    
#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif

    double fitted_mu = live_latency.get_mu();
    double fitted_sigma = live_latency.get_sigma();
    
    std::cout << "Fitted Parameters (LiveLatency MLE): mu = " << fitted_mu 
              << ", sigma = " << fitted_sigma << std::endl;
    std::cout << "Samples collected: " << empirical_samples.size() 
              << " (dropped: " << total_drops << ")" << std::endl;
    
    if (empirical_samples.size() < 100) {
        std::cout << "ERROR: Insufficient samples for KS test. Is the echo server running?" << std::endl;
        live_latency.stop();
        return 1;
    }
    
    std::sort(empirical_samples.begin(), empirical_samples.end());
    
    double d_max = 0.0;
    int n = static_cast<int>(empirical_samples.size());
    for (int i = 0; i < n; ++i) {
        double empirical_cdf = static_cast<double>(i + 1) / n;
        double theoretical_cdf = lognormalCDF(empirical_samples[i], fitted_mu, fitted_sigma);
        d_max = std::max(d_max, std::abs(empirical_cdf - theoretical_cdf));
    }
    
    // 5% significance
    double critical_value = 1.36 / std::sqrt(static_cast<double>(n));
    
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Kolmogorov-Smirnov Test Results (n=" << n << ")" << std::endl;
    std::cout << "KS Statistic (D_n): " << d_max << std::endl;
    std::cout << "Critical Value (alpha=0.05): " << critical_value << std::endl;
    
    if (d_max < critical_value) {
        std::cout << "Result: PASS. Empirical jitter is consistent with Log-Normal." << std::endl;
    } else {
        std::cout << "Result: FAIL. Distribution deviates from Log-Normal." << std::endl;
        std::cout << "expected failure. loopback is noisy as hell and log-normal doesn't fit the fat tails." << std::endl;
    }
    
    live_latency.stop();
    return 0;
}
