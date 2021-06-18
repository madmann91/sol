#include <chrono>

#include "sol/render_job.h"
#include "sol/scene.h"
#include "sol/renderer.h"

namespace sol {

RenderJob::RenderJob() = default;
RenderJob::~RenderJob() = default;

RenderJob::RenderJob(RenderJob&& other)
    : sample_count(other.sample_count)
    , samples_per_frame(other.samples_per_frame)
    , scene(std::move(other.scene))
    , output(std::move(other.output))
    , renderer(std::move(other.renderer))
{}

void RenderJob::start(std::function<bool (const RenderJob&)>&& frame_end) {
    must_stop_ = false;
    render_thread_ = std::thread([&] {
        for (size_t i = 0; sample_count == 0 || i < sample_count; i += samples_per_frame) {
            size_t j = i + samples_per_frame;
            if (i + j > sample_count && sample_count != 0)
                j = sample_count - i;

            renderer->render(*output, i, j);
            if (!frame_end(*this) || must_stop_)
                break;
        }

        std::unique_lock<std::mutex> lock(mutex_);
        is_done_ = true;
        done_cond_.notify_one();
    });
}

bool RenderJob::wait(size_t timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (timeout_ms != 0) {
        using namespace std::chrono_literals;
        auto target = std::chrono::system_clock::now() + timeout_ms * 1ms;
        if (!done_cond_.wait_until(lock, target, [&] { return is_done_; }))
            return false;
    }
    render_thread_.join();
    return true;
}

void RenderJob::cancel() {
    must_stop_ = true;
}

std::optional<RenderJob> RenderJob::load(const std::string& file_name, std::ostream* err_out) {
    (*err_out) << "Cannot load render job file '" + file_name + "'";
    return {};
}

} // namespace sol
