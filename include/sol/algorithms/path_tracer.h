#ifndef SOL_ALGORITHMS_PATH_TRACER_H
#define SOL_ALGORITHMS_PATH_TRACER_H

#include "sol/renderer.h"
#include "sol/color.h"

namespace sol {

class Sampler;

// FIXME: Workaround gcc/clang bug (this should be part of PathTracer).
// See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88165
// and https://bugs.llvm.org/show_bug.cgi?id=36684
namespace detail {
    struct PathTracerConfig {
        size_t min_russian_roulette_path_len = 3; ///< Minimum path length to enable Russian Roulette
        float  max_survival_prob = 0.75f;         ///< Maximum Russian Roulette survival probability (must be in `[0, 1]`)
        float  min_survival_prob = 0.05f;         ///< Minimum Russian Roulette survival probability (must be in `[0, 1]`)
        float  ray_offset = 1.e-5f;               ///< Ray offset, in order to avoid self-intersections. Usually scene-dependent.
        size_t max_path_len = 64;                 ///< Maximum path length
    };
}

class PathTracer final : public Renderer {
public:
    using Config = detail::PathTracerConfig;

    PathTracer(const Scene& scene, const Config& config = Config {})
        : Renderer("PathTracer", scene), config_(config)
    {}

    void render(Image&, size_t) const override;

private:
    Color trace_path(Sampler&, proto::Rayf) const;

    Config config_;
};

} // namespace sol

#endif
