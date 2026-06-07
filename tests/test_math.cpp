#include <gtest/gtest.h>
#include "ou_sampler.hpp"
#include "math_utils.hpp"
#include <random>
#include <cmath>

TEST(OUSamplerTest, StationaryMoments) {
    double theta = 2.0;
    double mu = 100.0;
    double sigma_V = 5.0;
    double dt = 0.01;
    int num_steps = 100000;
    
    arctic::OUSampler sampler(theta, mu, sigma_V, dt);
    std::mt19937_64 rng(42); 
    
    // Burn-in
    double v_t = mu;
    for (int i = 0; i < 10000; ++i) {
        v_t = sampler.step(v_t, rng);
    }
    
    double sum = 0.0;
    double sum_sq = 0.0;
    for (int i = 0; i < num_steps; ++i) {
        v_t = sampler.step(v_t, rng);
        sum += v_t;
        sum_sq += v_t * v_t;
    }
    
    double empirical_mean = sum / num_steps;
    double empirical_var = (sum_sq / num_steps) - (empirical_mean * empirical_mean);
    
    double theoretical_mean = sampler.get_stationary_mean();
    double theoretical_var = sampler.get_stationary_variance();
    
    EXPECT_NEAR(empirical_mean, theoretical_mean, 0.1);
    EXPECT_NEAR(empirical_var, theoretical_var, theoretical_var * 0.05);
}

TEST(MathUtilsTest, ComputePWin) {
    // If sigmas are equal, P(win) should be exactly 0.5
    EXPECT_DOUBLE_EQ(arctic::compute_p_win(0.5, 0.5), 0.5);
    
    // If self has lower variance, win probability should be > 0.5
    EXPECT_GT(arctic::compute_p_win(0.1, 0.5), 0.5);
    
    // If self has higher variance, win probability should be < 0.5
    EXPECT_LT(arctic::compute_p_win(0.5, 0.1), 0.5);
}

TEST(MathUtilsTest, ComputeEquilibriumBoundary) {
    double sig_A = 0.1;
    double sig_B = 0.2;
    double theta = 2.0;
    double mu_delta = -4.0;
    double mu = 0.0;
    double cost_c = 0.5;
    
    double b_A = arctic::compute_equilibrium_boundary(sig_A, sig_B, theta, mu_delta, mu, cost_c);
    double b_B = arctic::compute_equilibrium_boundary(sig_B, sig_A, theta, mu_delta, mu, cost_c);
    
    // b_A should be lower than b_B because A has lower latency variance and therefore higher P(win)
    EXPECT_LT(b_A, b_B);
    EXPECT_GT(b_A, cost_c);
}
