#pragma once

#include "spsc_buffer.hpp"
#include <atomic>
#include <thread>
#include <string>

namespace arctic {

// runs a background thread to ping loopback and fit the variance
// jitter is gonna look high because it's local but good for testing the math
class LiveLatency {
public:
    LiveLatency(const std::string& target_ip, int target_port, size_t buffer_capacity = 1024);
    ~LiveLatency();

    // boot threads
    void start();
    
    // kill threads
    void stop();

    // get mu safely
    double get_mu() const { return mu_.load(std::memory_order_acquire); }
    
    // get sigma safely
    double get_sigma() const { return sigma_.load(std::memory_order_acquire); }

    // valid samples
    size_t get_sample_count() const { return sample_count_.load(std::memory_order_acquire); }

private:
    void udp_measurement_loop();
    void mle_fitting_loop();
    
    // nano timer
    double get_time_ns() const;

    std::string target_ip_;
    int target_port_;
    
    std::atomic<bool> running_;
    
    // lock-free pipe for rtt data
    SPSCBuffer<double> rtt_buffer_;
    
    std::thread udp_thread_;
    std::thread mle_thread_;
    
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)
#endif

    // padded to avoid cache thrashing
    alignas(64) std::atomic<double> mu_;
    alignas(64) std::atomic<double> sigma_;
    alignas(64) std::atomic<size_t> sample_count_;

#ifdef _MSC_VER
#pragma warning(pop)
#endif
};

} // namespace arctic
