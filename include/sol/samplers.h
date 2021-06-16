#ifndef SOL_SAMPLERS_H
#define SOL_SAMPLERS_H

#include <cstdint>
#include <random>

namespace sol {

/// Random number source.
class Sampler {
public:
    /// Generates a new floating-point value in the range [0, 1] from this sampler.
    virtual float operator () () = 0;

protected:
    ~Sampler() {}
};

/// Random number generator compatible with the standard library.
struct PcgGenerator {
    using result_type = uint32_t;
    static constexpr uint32_t min() { return std::numeric_limits<uint32_t>::min(); }
    static constexpr uint32_t max() { return std::numeric_limits<uint32_t>::max(); }

    static constexpr uint32_t inc = 1;

    PcgGenerator(uint64_t seed)
        : state(seed)
    {}

    uint32_t operator () () {
        auto old_state = state;
        state = old_state * 6364136223846793005ULL + inc;
        uint32_t xorshifted = ((old_state >> 18u) ^ old_state) >> 27u;
        uint32_t rot = old_state >> 59u;
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    }

    uint64_t state;
};

/// Template to create samplers from a generator compatible with the standard-library.
template <typename Generator>
class StdRandomSampler final : public Sampler {
public:
    StdRandomSampler(uint64_t seed)
        : distrib_(0, 1), gen_(seed)
    {}

    float operator () () override final { return distrib_(gen_); }

private:
    std::uniform_real_distribution<float> distrib_;
    Generator gen_;
};

using Mt19937Sampler = StdRandomSampler<std::mt19937>; ///< Mersenne-twister-based sampler.
using PcgSampler     = StdRandomSampler<PcgGenerator>; ///< PCG-based sampler.

} // namespace sol

#endif
