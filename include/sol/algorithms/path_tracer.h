#ifndef SOL_ALGORITHMS_PATH_TRACER_H
#define SOL_ALGORITHMS_PATH_TRACER_H

#include "sol/renderer.h"
#include "sol/color.h"

#if defined(SOL_ENABLE_TBB)
#include <par/tbb/executors.h>
#elif defined(SOL_ENABLE_OMP)
#include <par/omp/executors.h>
#else
#include <par/sequential_executor.h>
#endif

namespace sol {

class Sampler;

namespace detail {

struct PathTracerConfig {
    size_t max_path_len = 64;           ///< Maximum path length
    size_t min_rr_path_len = 3;         ///< Minimum path length to enable Russian Roulette
    float  min_survival_prob = 0.05f;   ///< Minimum Russian Roulette survival probability (must be in `[0, 1]`)
    float  max_survival_prob = 0.75f;   ///< Maximum Russian Roulette survival probability (must be in `[0, 1]`)
    float  ray_offset = 1.e-5f;         ///< Ray offset, in order to avoid self-intersections. Usually scene-dependent.
};

} // namespace detail

class PathTracer final : public Renderer {
public:
    using Config = detail::PathTracerConfig;

    PathTracer(const Scene& scene, const Config& config = {})
        : Renderer("PathTracer", scene), config_(config)
    {}

    void render(Image&, size_t, size_t) const override;

private:
    Color trace_path(Sampler&, proto::Rayf) const;

#if defined(SOL_ENABLE_TBB)
    par::tbb::Executor executor_;
#elif defined(SOL_ENABLE_OMP)
    par::omp::DynamicExecutor executor_;
#else
    par::SequentialExecutor executor_;
#endif
    Config config_;
};

} // namespace sol

#endif
