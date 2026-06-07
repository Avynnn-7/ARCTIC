#pragma once

#include <random>

namespace arctic {

struct RaceResult {
    bool agent_a_won;
    bool agent_b_won;
};

// figures out who wins when both pull the trigger
class RaceResolver {
public:
    // lowest latency wins the race
    RaceResult resolve_race(double delta_a, double delta_b) const {
        if (delta_a < delta_b) {
            return {true, false};
        } else if (delta_b < delta_a) {
            return {false, true};
        }

        // Measure-zero in continuous time but float quantisation can produce
        // exact equality. Resolve with a fair coin so PnL isn't silently dropped.
        thread_local std::mt19937 rng{std::random_device{}()};
        thread_local std::bernoulli_distribution coin{0.5};
        return coin(rng) ? RaceResult{true, false} : RaceResult{false, true};
    }
};

} // namespace arctic
