#ifndef SOL_RENDER_JOB_H
#define SOL_RENDER_JOB_H

#include <memory>
#include <string>
#include <optional>
#include <ostream>
#include <functional>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "sol/image.h"

namespace sol {

struct Scene;
class Renderer;

struct Viewport {
    size_t width, height;

    float aspect_ratio() const {
        return static_cast<float>(width) / static_cast<float>(height);
    }
};

namespace detail {

struct RenderJobDefaults {
    size_t output_width = 1080;
    size_t output_height = 720;
    size_t sample_count = 100;
    size_t samples_per_frame = 2;
};

} // namespace detail

/// A rendering job, with accompanying scene data and renderer.
/// Rendering jobs should only be controlled from a single thread
/// (i.e. calling `wait/start/cancel` from different threads is undefined behavior).
struct RenderJob {
    using Defaults = detail::RenderJobDefaults;

    size_t sample_count;                ///< Number of samples to render (0 = unlimited, until cancellation).
    size_t samples_per_frame;           ///< Number of samples per frame (larger = higher throughput but higher latency)
    std::unique_ptr<Scene> scene;       ///< Scene to render.
    std::unique_ptr<Image> output;      ///< Output image, where samples are accumulated.
    std::unique_ptr<Renderer> renderer; ///< Renderer to use.

    /// Creates an empty rendering job.
    /// The renderer, scene, and output are left uninitialized.
    RenderJob(const Defaults& = {});
    RenderJob(const RenderJob&) = delete;
    RenderJob(RenderJob&&);
    ~RenderJob();

    Viewport viewport() const { return Viewport { output->width(), output->height() }; }

    /// Starts the rendering job, producing samples into the output image.
    /// This function takes a callback that is called after a frame has been rendered.
    /// The next frame will only start after that callback returns, and if the returned value is `true`.
    /// If the returned value is false, the job is cancelled.
    void start(std::function<bool (const RenderJob&)>&& frame_end = {});

    /// Waits for this rendering job to finish, or until the given amount of milliseconds has passed.
    /// If the function is given 0 milliseconds as timeout, it will wait indefinitely without any timeout.
    /// Returns true if the rendering job is over, otherwise false.
    bool wait(size_t timeout_ms = 0);

    /// Explicitly cancels the rendering job.
    /// This might require waiting for some frames to finish rendering.
    void cancel();

    /// Loads a rendering job from the given configuration file.
    static std::optional<RenderJob> load(
        const std::string& file_name,
        const Defaults& defaults = {},
        std::ostream* err_out = nullptr);

private:
    static RenderJob load_and_throw_on_error(const std::string&, const Defaults&);

    std::thread render_thread_;
    std::mutex mutex_;
    std::condition_variable done_cond_;
    bool is_done_;
};

} // namespace sol

#endif
