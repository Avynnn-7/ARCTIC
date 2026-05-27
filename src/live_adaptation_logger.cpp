// loopback UDP test. scheduler gives us horrible jitter here compared to real colo
// but good enough to test if our boundary adapts properly when the network freaks out

#include "live_latency.hpp"
#include "math_utils.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cmath>

int main() {
    std::cout << "Starting 60-Second Live Adaptation Test..." << std::endl;
    
    arctic::LiveLatency live_latency("127.0.0.1", 12345);
    live_latency.start();
    
    // don't just sleep. welford needs actual data points or it spits garbage
    constexpr size_t min_warmup_samples = 100;
    std::cout << "Waiting for " << min_warmup_samples << " latency samples..." << std::endl;
    int warmup_attempts = 0;
    while (live_latency.get_sample_count() < min_warmup_samples) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        warmup_attempts++;
        if (warmup_attempts > 100) {
            std::cout << "WARNING: Only " << live_latency.get_sample_count()
                      << " samples after 10s. Proceeding." << std::endl;
            break;
        }
    }
    std::cout << "Warmup complete: " << live_latency.get_sample_count() << " samples." << std::endl;
    
    std::ofstream out("data/live_adaptation.csv");
    out << "Time_s,Mu,Sigma,Eq_b_A,Eq_b_B,P_Win,Signal_Decay\n";
    
    // Fixed parameters
    double sigma_B = 0.2;  
    double theta = 2.0;
    double mu_delta = -4.0;
    double mu = 0.0;
    double cost_c = 0.5;

    for (int t = 0; t <= 60; ++t) {
        double current_mu = live_latency.get_mu();
        double current_sigma = live_latency.get_sigma();
        
        if (current_sigma < 0.01) current_sigma = 0.01;
        
        double b_A = arctic::compute_equilibrium_boundary(current_sigma, sigma_B, theta, mu_delta, mu, cost_c);
        double b_B = arctic::compute_equilibrium_boundary(sigma_B, current_sigma, theta, mu_delta, mu, cost_c);
        double p_win = arctic::compute_p_win(current_sigma, sigma_B);
        double expected_latency = std::exp(mu_delta + current_sigma * current_sigma / 2.0);
        double signal_decay = std::exp(-theta * expected_latency);
        
        out << t << "," << current_mu << "," << current_sigma 
            << "," << b_A << "," << b_B << "," << p_win 
            << "," << signal_decay << "\n";
        
        if (t % 10 == 0) {
            std::cout << "t=" << t << "s | Sigma: " << current_sigma 
                      << " | b_A*: " << b_A << " | P(win): " << p_win 
                      << " | Decay: " << signal_decay 
                      << " | n=" << live_latency.get_sample_count() << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    live_latency.stop();
    out.close();
    std::cout << "Test Complete. Data written to data/live_adaptation.csv" << std::endl;
    return 0;
}
