#pragma once

#include <vector>
#include <random>

namespace arctic {

// handles pulling the trigger based on signal and latency assumptions
struct AgentDecision {
    bool wants_to_act;
    double latency_drawn;
};

class SingleAgent {
public:
    SingleAgent(double mu_delta, double sigma_delta, double dt);

    // check if signal crossed our threshold. using raw pointers because std::vector is too slow.
    template<typename RNG>
    AgentDecision evaluate_action(const double* v_data, int v_size, int current_step, double stopping_boundary, RNG& rng) {
        double delta = lognormal_dist_(rng);
        int lag_steps = static_cast<int>(delta / dt_);
        int observation_index = current_step - lag_steps;
        
        if (observation_index < 0) {
            observation_index = 0;
        }
        if (observation_index >= v_size) {
            observation_index = v_size - 1;
        }
        
        double observed_y = v_data[observation_index];
        bool acts = observed_y >= stopping_boundary;
        return {acts, delta};
    }

    // vector wrapper just in case
    template<typename RNG>
    AgentDecision evaluate_action(const std::vector<double>& v_history, int current_step, double stopping_boundary, RNG& rng) {
        return evaluate_action(v_history.data(), static_cast<int>(v_history.size()), current_step, stopping_boundary, rng);
    }

private:
    double mu_delta_;
    double sigma_delta_;
    double dt_;
    std::lognormal_distribution<double> lognormal_dist_;
};

} // namespace arctic
