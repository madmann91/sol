#include <filesystem>
#include <system_error>
#include <chrono>

#include <toml++/toml.h>

#include "scene_loader.h"

#include "sol/render_job.h"
#include "sol/scene.h"
#include "sol/renderer.h"
#include "sol/algorithms/path_tracer.h"

namespace sol {

RenderJob::RenderJob(const Defaults& defaults)
    : sample_count(defaults.sample_count)
    , samples_per_frame(defaults.samples_per_frame)
    , is_done_(true)
{}

RenderJob::~RenderJob() = default;

RenderJob::RenderJob(RenderJob&& other)
    : sample_count(other.sample_count)
    , samples_per_frame(other.samples_per_frame)
    , is_done_(true)
{
    other.wait();
    scene = std::move(other.scene);
    output = std::move(other.output);
    renderer = std::move(other.renderer);
}

void RenderJob::start(std::function<bool (const RenderJob&)>&& frame_end) {
    is_done_ = false;
    render_thread_ = std::thread([&] {
        for (size_t i = 0; sample_count == 0 || i < sample_count; i += samples_per_frame) {
            size_t j = i + samples_per_frame;
            if (i + j > sample_count && sample_count != 0)
                j = sample_count - i;

            renderer->render(*output, i, j);
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

std::optional<RenderJob> RenderJob::load(const std::string& file_name, const Defaults& defaults, std::ostream* err_out) {
    try {
        return std::make_optional(load_and_throw_on_error(file_name, defaults));
    } catch (toml::parse_error& error) {
        if (err_out) (*err_out) << SourceError::from_toml(error).what();
    } catch (std::exception& e) {
        if (err_out) (*err_out) << e.what();
    }
    return std::nullopt;
}

static std::unique_ptr<Renderer> create_renderer(const toml::table& table, const Scene& scene) {
    auto type = table["type"].value_or<std::string>("");
    if (type == "path_tracer") {
        PathTracer::Config config;
        config.min_russian_roulette_path_len =
            table["min_russian_roulette_path_len"].value_or(config.min_russian_roulette_path_len);
        config.max_survival_prob = table["max_survival_prob"].value_or(config.max_survival_prob);
        config.min_survival_prob = table["min_survival_prob"].value_or(config.min_survival_prob);
        config.ray_offset        = table["ray_offset"].value_or(config.ray_offset);
        config.max_path_len      = table["max_path_len"].value_or(config.max_path_len);
        return std::make_unique<PathTracer>(scene, config);
    }
    throw SourceError::from_toml(table.source(), "Unknown renderer type '" + type + "'");
}

static std::unique_ptr<Scene> load_scene(const std::string& file_name, const Image& output) {
    Scene::Defaults defaults;
    defaults.aspect_ratio =
        static_cast<float>(output.width()) /
        static_cast<float>(output.height());

    auto scene = std::make_unique<Scene>();
    SceneLoader loader(*scene, defaults, nullptr);
    loader.load_and_throw_on_error(file_name);

    return scene;
}

RenderJob RenderJob::load_and_throw_on_error(const std::string& file_name, const Defaults& defaults) {
    std::ifstream is(file_name);
    if (!is)
        throw std::runtime_error("Cannot open job file '" + file_name + "'");

    std::error_code err_code;
    auto base_dir = std::filesystem::absolute(file_name, err_code).parent_path().string();

    auto table = toml::parse(is, file_name);

    RenderJob render_job(defaults);
    render_job.sample_count      = table["sample_count"     ].value_or(defaults.sample_count);
    render_job.samples_per_frame = table["samples_per_frame"].value_or(defaults.samples_per_frame);

    auto output_width  = table["output_width" ].value_or(defaults.output_width);
    auto output_height = table["output_height"].value_or(defaults.output_height);
    render_job.output = std::make_unique<Image>(output_width, output_height, 3);

    render_job.scene = load_scene(base_dir + "/" + table["scene"].value_or(""), *render_job.output);
    if (auto renderer = table["renderer"].as_table()) {
        render_job.renderer = create_renderer(*renderer, *render_job.scene);
        return render_job;
    }

    throw SourceError::from_toml(table.source(), "Missing 'renderer' element in render job");
}

} // namespace sol
