#pragma once

#include <random>

namespace arctic {

// exact OU sampler. euler-maruyama is for noobs.
class OUSampler {
public:
    OUSampler(double theta, double mu, double sigma_V, double dt);

    // long run mean
    double get_stationary_mean() const;

    // long run variance
    double get_stationary_variance() const;

    // step forward
    template<typename RNG>
    double step(double v_t, RNG& rng) {
        return mean_term_1_ + (v_t - mu_) * mean_term_2_ + std_dev_ * norm_dist_(rng);
    }

private:
    double theta_;
    double mu_;
    double sigma_V_;
    double dt_;

    // cached math so we don't call exp() in the loop
    double mean_term_1_;
    double mean_term_2_;
    double std_dev_;

    std::normal_distribution<double> norm_dist_;
};

} // namespace arctic
