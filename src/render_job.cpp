#include <chrono>

#include "sol/render_job.h"
#include "sol/scene.h"
#include "sol/image.h"
#include "sol/renderer.h"

namespace sol {

RenderJob::RenderJob(const Renderer& renderer, Image& output)
    : renderer(renderer), output(output)
{}

RenderJob::RenderJob(RenderJob&& other)
    : sample_count(other.sample_count)
    , samples_per_frame(other.samples_per_frame)
    , renderer(other.renderer)
    , output(other.output)
{}

RenderJob::~RenderJob() = default;

void RenderJob::start(std::function<bool (const RenderJob&)>&& frame_end) {
    is_done_ = false;
    render_thread_ = std::thread([&] {
        for (size_t i = 0; sample_count == 0 || i < sample_count; i += samples_per_frame) {
            size_t j = i + samples_per_frame;
            if (i + j > sample_count && sample_count != 0)
                j = sample_count - i;

            renderer.render(output, i, j);
            if ((frame_end && !frame_end(*this)) || is_done_)
                break;
        }

        std::unique_lock<std::mutex> lock(mutex_);
        is_done_ = true;
        done_cond_.notify_one();
    });
}

bool RenderJob::wait(size_t timeout_ms) {
    if (is_done_ || !render_thread_.joinable())
        return true;
    std::unique_lock<std::mutex> lock(mutex_);
    if (timeout_ms != 0) {
        using namespace std::chrono_literals;
        auto target = std::chrono::system_clock::now() + timeout_ms * 1ms;
        if (!done_cond_.wait_until(lock, target, [&] { return is_done_; }))
            return false;
    } else
        done_cond_.wait(lock, [&] { return is_done_; });
    render_thread_.join();
    return true;
}

void RenderJob::cancel() {
    is_done_ = true;
}

} // namespace sol
