#ifndef SOL_RENDERER_H
#define SOL_RENDERER_H

#include <string_view>
#include <string>
#include <memory>
#include <ranges>
#include <algorithm>
#include <execution>

#include <proto/hash.h>
#include <proto/utils.h>

#include "sol/samplers.h"
#include "sol/scene.h"

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

    /// Renders the frame with the given index into the given image.
    /// Since the behavior is entirely deterministic, this `frame_index`
    /// variable can be used to retrace a particular frame.
    virtual void render(Image& image, size_t frame_index) const = 0;

    /// Processes each tile of the given range `[0,w]x[0,h]` in parallel.
    /// The size of each tile is given 
    template <typename F>
    static inline void for_each_tile(
        size_t w, size_t h, F&& f,
        size_t tile_w = default_tile_size,
        size_t tile_h = default_tile_size)
    {
        // TODO: Define a custom 2D range for this to work properly
        auto range_y = std::views::iota(size_t{0}, proto::round_up(h, tile_h) / tile_h);
        auto range_x = std::views::iota(size_t{0}, proto::round_up(w, tile_w) / tile_w);
        std::for_each(
            std::execution::par_unseq,
            range_y.begin(), range_y.end(),
            [&] (size_t tile_y) {
                size_t y = tile_y * tile_h;
                std::for_each(
                    std::execution::par_unseq,
                    range_x.begin(), range_x.end(),
                    [&] (size_t tile_x) {
                        size_t x = tile_x * tile_w;
                        f(x, y, std::min(x + tile_w, w), std::min(y + tile_h, h));
                    });
            });
    }

    /// Generates a seed suitable to initialize a sampler, given a frame index, and a pixel position (2D).
    static inline uint32_t pixel_seed(size_t frame_index, size_t x, size_t y) {
        return proto::fnv::Hasher()
            .combine(frame_index)
            .combine(x)
            .combine(y);
    }

    /// Generates a seed suitable to initialize a sampler, given a frame index, and a pixel position (1D).
    static inline uint32_t pixel_seed(size_t frame_index, size_t x) {
        return proto::fnv::Hasher().combine(frame_index).combine(x);
    }

    /// Samples the area within a pixel, using the given sampler.
    /// Returns the coordinates of the pixel in camera space (i.e. `[-1, 1]^2`).
    static inline proto::Vec2f sample_pixel(Sampler& sampler, size_t x, size_t y, size_t w, size_t h) {
        return proto::Vec2f(
            (x + sampler()) * (2.0f / static_cast<float>(w)) - 1.0f,
            (y + sampler()) * (2.0f / static_cast<float>(h)) - 1.0f);
    }

    /// Computes a Russian Roulette survival probability from the luminance of the current path throughput.
    static inline float survival_prob(float luminance, float min_prob, float max_prob) {
        return proto::robust_min(proto::robust_max(luminance, min_prob), max_prob);
    }

    /// Computes the balance heuristic given the probability density values for two techniques.
    static inline float balance_heuristic(float x, float y) {
        return x / (x + y);
    }

protected:
    std::string name_;
    const Scene& scene_;
};

} // namespace sol

#endif
