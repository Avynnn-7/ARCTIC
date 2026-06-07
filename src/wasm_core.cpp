#include <emscripten/bind.h>
#include <emscripten/emscripten.h>
#include <atomic>
#include <vector>
#include <cmath>
#include <random>

using namespace emscripten;

class ArcticWasmEngine {
private:
    int num_points;
    double dt;
    
    // OU process parameters
    double theta = 2.0;
    double mu = 0.0;
    double sigma_V = 1.0;
    double cost_c = 0.5;
    double mu_delta = -4.0;  // LogNormal location parameter for latency
    
    // cached OU math
    double ou_decay;          // exp(-theta * dt)
    double ou_conditional_std; // sigma_V * sqrt((1 - exp(-2*theta*dt)) / (2*theta))
    
    // circular buffers for the ui
    std::vector<float> ou_signal;
    std::vector<float> boundary_a;
    std::vector<float> boundary_b;
    
    int head = 0;
    double current_v = 0.0;
    std::mt19937_64 rng;
    std::normal_distribution<double> norm_dist;
    
    // welford running stats
    size_t welford_n = 0;
    double welford_mean = 0.0;
    double welford_m2 = 0.0;
    
    // cache for js
    float cached_boundary_a_val = 1.0f;
    float cached_boundary_b_val = 1.0f;
    float cached_p_win = 0.5f;
    float cached_signal_decay_a = 1.0f;
    
    // lock-free read from webrtc
    std::atomic<float>* live_sigma_ptr = nullptr;

    // normal cdf
    static double normal_cdf(double x) {
        return 0.5 * std::erfc(-x * 0.70710678118654752440);
    }

    // math for the trigger boundary
    // dominant strategy so we don't care what they do
    double compute_equilibrium_boundary(double sig_self, double sig_competitor) const {
        // expected latency delay
        double expected_latency = std::exp(mu_delta + sig_self * sig_self / 2.0);
        
        // how much the signal drops while we wait
        double decay = std::exp(-theta * expected_latency);
        
        if (decay < 1e-10) {
            return 1.0; // Latency so high the signal decays entirely; fallback
        }
        
        // chance we win the race based on variance diff
        double var_self = sig_self * sig_self;
        double var_comp = sig_competitor * sig_competitor;
        double combined_std = std::sqrt(var_self + var_comp);
        
        double p_win = 0.5;
        if (combined_std > 1e-10) {
            p_win = normal_cdf((var_comp - var_self) / (2.0 * combined_std));
        }
        
        // where payoff hits zero. ignores opponent's boundary.
        // => b* = mu + (c - mu) / (p_win * decay)
        double effective_decay = p_win * decay;
        if (effective_decay < 1e-10) {
            return 1.0; // Can't win; use fallback
        }
        
        double b_star = mu + (cost_c - mu) / effective_decay;
        return b_star;
    }

public:
    ArcticWasmEngine(int points, double delta_t) 
        : num_points(points), dt(delta_t), rng(42), norm_dist(0.0, 1.0) 
    {
        // Precompute exact OU transition kernel constants
        ou_decay = std::exp(-theta * dt);
        double var_term = (1.0 - std::exp(-2.0 * theta * dt)) / (2.0 * theta);
        ou_conditional_std = sigma_V * std::sqrt(var_term);
        
        ou_signal.resize(num_points, 0.0f);
        boundary_a.resize(num_points, 0.0f);
        boundary_b.resize(num_points, 0.0f);
    }
    
    // hook up shared memory from js
    void bind_latency_buffer(uintptr_t ptr) {
        live_sigma_ptr = reinterpret_cast<std::atomic<float>*>(ptr);
    }
    
    // called on raf. runs the exact OU math
    void step_frame(int steps_per_frame) {
        float sig_a = 0.1f; // fallback
        if (live_sigma_ptr != nullptr) {
            // lock-free read
            sig_a = live_sigma_ptr->load(std::memory_order_acquire);
        }
        
        // Clamp to reasonable range
        if (sig_a < 0.01f) sig_a = 0.01f;
        if (sig_a > 2.0f) sig_a = 2.0f;
        
        // Competitor is fixed at 0.2
        float sig_b = 0.2f;
        
        // run the math for both
        double b_A_val = compute_equilibrium_boundary(static_cast<double>(sig_a), static_cast<double>(sig_b));
        double b_B_val = compute_equilibrium_boundary(static_cast<double>(sig_b), static_cast<double>(sig_a));
        
        // Cache for JS readback
        cached_boundary_a_val = static_cast<float>(b_A_val);
        cached_boundary_b_val = static_cast<float>(b_B_val);
        
        // Cache auxiliary diagnostics
        double var_self = sig_a * sig_a;
        double var_comp = sig_b * sig_b;
        double combined_std = std::sqrt(var_self + var_comp);
        if (combined_std > 1e-10) {
            cached_p_win = static_cast<float>(normal_cdf((var_comp - var_self) / (2.0 * combined_std)));
        }
        double expected_latency_a = std::exp(mu_delta + var_self / 2.0);
        cached_signal_decay_a = static_cast<float>(std::exp(-theta * expected_latency_a));
        
        for (int i = 0; i < steps_per_frame; ++i) {
            // exact OU step
            double Z = norm_dist(rng);
            current_v = mu + (current_v - mu) * ou_decay + ou_conditional_std * Z;
            
            // update stats
            welford_n++;
            double delta = current_v - welford_mean;
            welford_mean += delta / static_cast<double>(welford_n);
            double delta2 = current_v - welford_mean;
            welford_m2 += delta * delta2;
            
            ou_signal[head] = static_cast<float>(current_v);
            boundary_a[head] = cached_boundary_a_val;
            boundary_b[head] = cached_boundary_b_val;
            
            head = (head + 1) % num_points;
        }
    }
    
    // give js pointers so it can copy the arrays fast
    uintptr_t get_ou_signal_ptr() const { return reinterpret_cast<uintptr_t>(ou_signal.data()); }
    uintptr_t get_boundary_a_ptr() const { return reinterpret_cast<uintptr_t>(boundary_a.data()); }
    uintptr_t get_boundary_b_ptr() const { return reinterpret_cast<uintptr_t>(boundary_b.data()); }
    
    int get_head() const { return head; }
    
    // getters
    float get_current_v() const { return static_cast<float>(current_v); }
    float get_boundary_a_val() const { return cached_boundary_a_val; }
    float get_boundary_b_val() const { return cached_boundary_b_val; }
    float get_p_win() const { return cached_p_win; }
    float get_signal_decay() const { return cached_signal_decay_a; }
    
    // welford stats
    float get_signal_mean() const { return static_cast<float>(welford_mean); }
    float get_signal_variance() const { 
        if (welford_n < 2) return 0.0f;
        return static_cast<float>(welford_m2 / (welford_n - 1)); 
    }
    float get_theoretical_variance() const {
        return static_cast<float>((sigma_V * sigma_V) / (2.0 * theta));
    }
};

EMSCRIPTEN_BINDINGS(arctic_wasm) {
    class_<ArcticWasmEngine>("ArcticWasmEngine")
        .constructor<int, double>()
        .function("bind_latency_buffer", &ArcticWasmEngine::bind_latency_buffer)
        .function("step_frame", &ArcticWasmEngine::step_frame)
        .function("get_ou_signal_ptr", &ArcticWasmEngine::get_ou_signal_ptr)
        .function("get_boundary_a_ptr", &ArcticWasmEngine::get_boundary_a_ptr)
        .function("get_boundary_b_ptr", &ArcticWasmEngine::get_boundary_b_ptr)
        .function("get_head", &ArcticWasmEngine::get_head)
        .function("get_current_v", &ArcticWasmEngine::get_current_v)
        .function("get_boundary_a_val", &ArcticWasmEngine::get_boundary_a_val)
        .function("get_boundary_b_val", &ArcticWasmEngine::get_boundary_b_val)
        .function("get_p_win", &ArcticWasmEngine::get_p_win)
        .function("get_signal_decay", &ArcticWasmEngine::get_signal_decay)
        .function("get_signal_mean", &ArcticWasmEngine::get_signal_mean)
        .function("get_signal_variance", &ArcticWasmEngine::get_signal_variance)
        .function("get_theoretical_variance", &ArcticWasmEngine::get_theoretical_variance);
}
