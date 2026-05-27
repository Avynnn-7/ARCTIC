#include "ou_sampler.hpp"
#include "single_agent.hpp"
#include "race_resolver.hpp"
#include "math_utils.hpp"
#include "arena_allocator.hpp"
#include "tsc_clock.hpp"
#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>
#include <fstream>
#include <string>
#include <chrono>
#include <numeric>

struct SweepResult {
    double sigma_A;
    double sigma_B;
    double eq_b_A;
    double eq_b_B;
    double p_win_A;
    double win_rate_A;
    double win_rate_B;
    double avg_pnl_A;
    double avg_pnl_Solo;
    double competitive_cost;
    double sharpe_A;
    double mean_stop_time_A;
    double elapsed_ms;
};

SweepResult run_sweep_step(double sigma_A, double sigma_B) {
    double theta = 2.0;
    double mu = 0.0;
    double sigma_V = 1.0;
    double dt = 0.01;
    int steps = 1000;
    int num_paths = 50000;
    double cost_c = 0.5;
    double mu_delta = -4.0;
    
    arctic::OUSampler sampler(theta, mu, sigma_V, dt);
    arctic::SingleAgent agent_solo(mu_delta, sigma_A, dt);
    arctic::SingleAgent agent_a(mu_delta, sigma_A, dt);
    arctic::SingleAgent agent_b(mu_delta, sigma_B, dt);
    arctic::RaceResolver resolver;
    
    // bugfix: keep seeds separate or it breaks the PnL diff math
    std::mt19937_64 rng_ou(42);
    std::mt19937_64 rng_solo(101);
    std::mt19937_64 rng_a(201);
    std::mt19937_64 rng_b(301);
    
    double b_solo = arctic::compute_solo_boundary(sigma_A, theta, mu_delta, mu, cost_c);
    double b_a = arctic::compute_equilibrium_boundary(sigma_A, sigma_B, theta, mu_delta, mu, cost_c);
    double b_b = arctic::compute_equilibrium_boundary(sigma_B, sigma_A, theta, mu_delta, mu, cost_c);
    double p_win_a = arctic::compute_p_win(sigma_A, sigma_B);
    
    double pnl_solo = 0.0;
    double pnl_a = 0.0;
    double pnl_b = 0.0;
    int trades_solo = 0;
    int trades_a = 0;
    int trades_b = 0;
    
    // pnl tracker for sharpe
    std::vector<double> pnl_per_path_a(num_paths, 0.0);
    
    // track when we pull the trigger
    std::vector<int> stop_times_a;
    stop_times_a.reserve(num_paths);
    
    // arena alloc. keep new/delete out of the hot path.
    arctic::ArenaAllocator arena(steps * sizeof(double) + 64);
    
    auto t_start = std::chrono::steady_clock::now();
    
    for (int p = 0; p < num_paths; ++p) {
        arena.reset();
        double* v_history = arena.allocate<double>(steps);
        
        // exact OU step
        v_history[0] = mu;
        for (int i = 1; i < steps; ++i) {
            v_history[i] = sampler.step(v_history[i - 1], rng_ou);
        }
        
        bool solo_acted = false;
        bool game_resolved = false;
        
        for (int i = 1; i < steps; ++i) {
            if (!solo_acted) {
                auto decision_solo = agent_solo.evaluate_action(v_history, steps, i, b_solo, rng_solo);
                if (decision_solo.wants_to_act) {
                    int exec_step = std::min(i + static_cast<int>(decision_solo.latency_drawn / dt), steps - 1);
                    pnl_solo += (v_history[exec_step] - cost_c);
                    trades_solo++;
                    solo_acted = true;
                }
            }
            
            if (!game_resolved) {
                auto dec_a = agent_a.evaluate_action(v_history, steps, i, b_a, rng_a);
                auto dec_b = agent_b.evaluate_action(v_history, steps, i, b_b, rng_b);
                
                if (dec_a.wants_to_act && dec_b.wants_to_act) {
                    auto result = resolver.resolve_race(dec_a.latency_drawn, dec_b.latency_drawn);
                    int exec_a = std::min(i + static_cast<int>(dec_a.latency_drawn / dt), steps - 1);
                    int exec_b = std::min(i + static_cast<int>(dec_b.latency_drawn / dt), steps - 1);
                    if (result.agent_a_won) {
                        double path_pnl = v_history[exec_a] - cost_c;
                        pnl_a += path_pnl;
                        pnl_per_path_a[p] = path_pnl;
                        trades_a++;
                        stop_times_a.push_back(i);
                    } else if (result.agent_b_won) {
                        pnl_b += (v_history[exec_b] - cost_c);
                        trades_b++;
                    }
                    game_resolved = true;
                } else if (dec_a.wants_to_act) {
                    int exec_step = std::min(i + static_cast<int>(dec_a.latency_drawn / dt), steps - 1);
                    double path_pnl = v_history[exec_step] - cost_c;
                    pnl_a += path_pnl;
                    pnl_per_path_a[p] = path_pnl;
                    trades_a++;
                    stop_times_a.push_back(i);
                    game_resolved = true;
                } else if (dec_b.wants_to_act) {
                    int exec_step = std::min(i + static_cast<int>(dec_b.latency_drawn / dt), steps - 1);
                    pnl_b += (v_history[exec_step] - cost_c);
                    trades_b++;
                    game_resolved = true;
                }
            }
        }
    }
    
    auto t_end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    
    double win_rate_a = (trades_a + trades_b > 0) ? static_cast<double>(trades_a) / (trades_a + trades_b) : 0.0;
    double win_rate_b = (trades_a + trades_b > 0) ? static_cast<double>(trades_b) / (trades_a + trades_b) : 0.0;
    double avg_pnl_solo = trades_solo ? pnl_solo / num_paths : 0.0;
    double avg_pnl_a = trades_a ? pnl_a / num_paths : 0.0;
    double competitive_cost = (sigma_A >= sigma_B) ? (avg_pnl_solo - avg_pnl_a) : 0.0;
    
    double sharpe_a = arctic::compute_sharpe_ratio(pnl_per_path_a.data(), num_paths);
    
    double mean_stop_a = 0.0;
    if (!stop_times_a.empty()) {
        mean_stop_a = std::accumulate(stop_times_a.begin(), stop_times_a.end(), 0.0)
                      / stop_times_a.size();
    }
    
    return {sigma_A, sigma_B, b_a, b_b, p_win_a, win_rate_a, win_rate_b, 
            avg_pnl_a, avg_pnl_solo, competitive_cost, sharpe_a, mean_stop_a, elapsed};
}

int main() {
    std::ofstream out("data/sweep_results.csv");
    out << "Sigma_A,Sigma_B,Eq_B_A,Eq_B_B,P_Win_A,Win_Rate_A,Win_Rate_B,"
        << "Avg_PnL_A,Avg_PnL_Solo,Competitive_Cost,Sharpe_A,Mean_StopTime_A,Elapsed_ms\n";
    
    double sigma_B = 0.1;
    
    std::cout << "Running Variance Gap Parameter Sweep (50k paths/step)...\n";
    for (double sigma_A = 0.1; sigma_A <= 1.51; sigma_A += 0.1) {
        std::cout << "Evaluating Sigma_A = " << sigma_A << "..." << std::flush;
        auto result = run_sweep_step(sigma_A, sigma_B);
        
        out << result.sigma_A << ","
            << result.sigma_B << ","
            << result.eq_b_A << ","
            << result.eq_b_B << ","
            << result.p_win_A << ","
            << result.win_rate_A << ","
            << result.win_rate_B << ","
            << result.avg_pnl_A << ","
            << result.avg_pnl_Solo << ","
            << result.competitive_cost << ","
            << result.sharpe_A << ","
            << result.mean_stop_time_A << ","
            << result.elapsed_ms << "\n";
            
        std::cout << " Done. (P_win=" << result.p_win_A 
                  << ", Cost=" << result.competitive_cost 
                  << ", Sharpe=" << result.sharpe_A
                  << ", " << result.elapsed_ms << "ms)\n";
    }
    
    out.close();
    std::cout << "Sweep complete! Data written to data/sweep_results.csv\n";
    return 0;
}
