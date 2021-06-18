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

struct RenderJob {
    size_t sample_count = 0;            ///< Number of samples to render (0 = unlimited, until cancellation).
    size_t samples_per_frame = 1;       ///< Number of samples per frame (larger = higher throughput but higher latency)
    std::unique_ptr<Scene> scene;       ///< Scene to render.
    std::unique_ptr<Image> output;      ///< Output image, where samples are accumulated.
    std::unique_ptr<Renderer> renderer; ///< Renderer to use.

    RenderJob();
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
    static std::optional<RenderJob> load(const std::string& file_name, std::ostream* err_out = nullptr);

private:
    std::thread render_thread_;
    std::mutex mutex_;
    std::condition_variable done_cond_;
    bool must_stop_;
    bool is_done_;
};

} // namespace sol

#endif
