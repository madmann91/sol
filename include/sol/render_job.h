#ifndef SOL_RENDER_JOB_H
#define SOL_RENDER_JOB_H

#include <functional>
#include <thread>
#include <condition_variable>
#include <mutex>

namespace sol {

struct Image;
struct Scene;
class Renderer;

/// A rendering job, with accompanying scene data and renderer.
/// Rendering jobs should only be controlled from a single thread
/// (i.e. calling `wait/start/cancel` from different threads is undefined behavior).
struct RenderJob {
    size_t sample_count = 0;        ///< Number of samples to render (0 = unlimited, until cancellation).
    size_t samples_per_frame = 1;   ///< Number of samples per frame (larger = higher throughput but higher latency)
    const Renderer& renderer;       ///< Rendering algorithm to use.
    Image& output;                  ///< Output image, where samples are accumulated.

    RenderJob(const Renderer& renderer, Image& output);
    RenderJob(const RenderJob&) = delete;
    RenderJob(RenderJob&&);
    ~RenderJob();

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

private:
    std::thread render_thread_;
    std::mutex mutex_;
    std::condition_variable done_cond_;
    bool is_done_ = true;
};

} // namespace sol

#endif
