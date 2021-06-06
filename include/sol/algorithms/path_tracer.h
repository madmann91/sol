#ifndef SOL_ALGORITHMS_PATH_TRACER_H
#define SOL_ALGORITHMS_PATH_TRACER_H

#include "sol/renderer.h"
#include "sol/color.h"

namespace sol {

class Sampler;

class PathTracer final : public Renderer {
public:
    struct Config {
        size_t min_russian_roulette_path_len = 3; ///< Minimum path length to enable Russian Roulette
        float  max_survival_prob = 0.75f;         ///< Maximum Russian Roulette survival probability (must be in `[0, 1]`)
        float  min_survival_prob = 0.05f;         ///< Minimum Russian Roulette survival probability (must be in `[0, 1]`)
        float  ray_offset = 1.e-5f;                ///< Ray offset, in order to avoid self-intersections. Usually scene-dependent.
        size_t max_path_len = 64;                 ///< Maximum path length
    };

    PathTracer(const Scene& scene, Config&& config)
        : Renderer("PathTracer", scene), config_(std::move(config))
    {}

    void render(Image&, size_t) const override;

private:
    Color trace_path(Sampler&, proto::Rayf) const;

    Config config_;
};

} // namespace sol

#endif
