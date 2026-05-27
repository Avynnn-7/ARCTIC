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
            return {true, false}; // Agent A is faster
        } else if (delta_b < delta_a) {
            return {false, true}; // Agent B is faster
        }
        
        // exact tie. prob zero in continuous time but just in case, nobody gets it.
        return {false, false}; 
    }
};

} // namespace arctic
