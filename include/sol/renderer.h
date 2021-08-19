#ifndef SOL_RENDERER_H
#define SOL_RENDERER_H

#include <string_view>
#include <string>
#include <memory>
#include <algorithm>

#include <proto/hash.h>
#include <proto/utils.h>

#include "sol/samplers.h"
#include "sol/scene.h"

#if defined(SOL_ENABLE_TBB)
#define TBB_SUPPRESS_DEPRECATED_MESSAGES 1
#include <tbb/tbb.h>
#endif

namespace sol {

struct Image;

/// Base class for all rendering algorithms.
class Renderer {
public:
    /// Default value for the size of each tile when calling `for_each_tile()`.
    static constexpr size_t default_tile_size = 32;

    Renderer(const std::string_view& name, const Scene& scene)
        : name_(name), scene_(scene)
    {}

    virtual ~Renderer() {}

    const std::string& name() const { return name_; }

    /// Renders the samples starting at the given index into the given image.
    /// Since the behavior is entirely deterministic, this `sample_index`
    /// variable can be used to retrace a particular set of samples.
    virtual void render(Image& image, size_t sample_index, size_t sample_count = 1) const = 0;

protected:
    /// Processes each tile of the given range `[0,w]x[0,h]` in parallel.
    /// The size of each tile is given 
    template <typename F>
    static inline void for_each_tile(
        size_t w, size_t h, F&& f,
        size_t tile_w = default_tile_size,
        size_t tile_h = default_tile_size)
    {
#if defined(SOL_ENABLE_TBB)
        tbb::parallel_for(
            tbb::blocked_range2d(
                size_t{0}, proto::round_up(h, tile_h) / tile_h,
                size_t{0}, proto::round_up(w, tile_w) / tile_w),
            [&] (auto& range) {
                for (auto tile_y = range.rows().begin(); tile_y < range.rows().end(); ++tile_y) {
                    for (auto tile_x = range.cols().begin(); tile_x < range.cols().end(); ++tile_x) {
                        size_t y = tile_y * tile_h;
                        size_t x = tile_x * tile_w;
                        f(x, y, std::min(x + tile_w, w), std::min(y + tile_h, h));
                    }
                }
            });
#else
    #if defined(SOL_ENABLE_OMP)
        #pragma omp parallel for collapse(2) schedule(dynamic)
    #endif
        for (size_t y = 0; y < h; y += tile_h) {
            for (size_t x = 0; x < w; x += tile_w)
                f(x, y, std::min(x + tile_w, w), std::min(y + tile_h, h));
        }
#endif
    }

    /// Generates a seed suitable to initialize a sampler, given a frame index, and a pixel position (2D).
    static inline uint32_t pixel_seed(size_t frame_index, size_t x, size_t y) {
        return proto::fnv::Hasher().combine(x).combine(y).combine(frame_index);
    }

    /// Samples the area within a pixel, using the given sampler.
    /// Returns the coordinates of the pixel in camera space (i.e. `[-1, 1]^2`).
    static inline proto::Vec2f sample_pixel(Sampler& sampler, size_t x, size_t y, size_t w, size_t h) {
        return proto::Vec2f(
            (x + sampler()) * (2.0f / static_cast<float>(w)) - 1.0f,
            1.0f - (y + sampler()) * (2.0f / static_cast<float>(h)));
    }

    /// Computes the balance heuristic given the probability density values for two techniques.
    static inline float balance_heuristic(float x, float y) {
        // More robust than x / (x + y), for when x, y = +-inf
        return 1.0f / (1.0f + y / x);
    }

    std::string name_;
    const Scene& scene_;
};

} // namespace sol

#endif
