#ifndef SOL_SAMPLERS_H
#define SOL_SAMPLERS_H

#include <cstdint>
#include <random>

#include <pcg_random.hpp>

namespace sol {

/// Random number source.
class Sampler {
public:
    /// Generates a new floating-point value in the range [0, 1] from this sampler.
    virtual float operator () () = 0;

protected:
    ~Sampler() {}
};

/// Template to create samplers from a generator compatible with the standard-library.
template <typename Rng>
class StdRandomSampler final : public Sampler {
public:
    StdRandomSampler(uint64_t seed)
        : distrib_(0, 1), rng_(seed)
    {}

    float operator () () override final { return distrib_(rng_); }

private:
    std::uniform_real_distribution<float> distrib_;
    Rng rng_;
};

using Mt19937Sampler = StdRandomSampler<std::mt19937>; ///< Mersenne-twister-based sampler.
using PcgSampler     = StdRandomSampler<pcg32>;        ///< PCG-based sampler.

} // namespace sol

#endif
