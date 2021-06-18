#include <string_view>
#include <optional>
#include <iostream>
#include <sstream>

#include <sol/scene.h>
#include <sol/render_job.h>

struct Options {
    std::string scene_file;
    std::string job_file;
    std::string out_file;
};

static void usage() {
    std::cout <<
        "Usage: driver [options] scene.toml job.toml\n"
        "Available options:\n"
        "  -h         --help  Shows this message\n"
        "  -o <image>         Sets the output image file name (default: \'render.exr\')\n";
}

static std::optional<Options> parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            using namespace std::literals::string_view_literals;
            if (argv[i] == "-h"sv || argv[i] == "--help"sv) {
                usage();
                return std::nullopt;
            } else {
                std::cerr << "Unknown option '" << argv[i] << "'" << std::endl;
                return std::nullopt;
            }
        } else if (options.scene_file.empty()) {
            options.scene_file = argv[i];
        } else if (options.job_file.empty()) {
            options.job_file = argv[i];
        } else {
            std::cerr << "Too many input files" << std::endl;
            return std::nullopt;
        }
    }
    if (options.scene_file.empty() || options.job_file.empty()) {
        std::cerr << "Missing scene and/or render job file" << std::endl;
        return std::nullopt;
    }
    return std::make_optional(options);
}

int main(int argc, char** argv) {
    auto options = parse_options(argc, argv);
    if (!options)
        return 1; 

    std::ostringstream err_stream;
    auto scene = sol::Scene::load(options->scene_file, {}, &err_stream);
    if (!scene) {
        std::cerr << err_stream.str() << std::endl;
        return 1;
    }

    std::cout
        << "Scene file '" << options->scene_file << "' loaded successfully\n"
        << "Summary:\n"
        << "    " << scene->bsdfs.size() << " BSDF(s)\n"
        << "    " << scene->lights.size() << " light(s)\n"
        << "    " << scene->textures.size() << " texture(s)\n"
        << "    " << scene->images.size() << " image(s)\n";

    auto render_job = sol::RenderJob::load(options->job_file, &err_stream);
    if (!render_job) {
        std::cerr << err_stream.str() << std::endl;
        return 1;
    }

    render_job->start();
    render_job->wait();

    if (!options->out_file.empty()) {
        render_job->output->scale(1.0f / static_cast<float>(render_job->sample_count));
        render_job->output->save(options->out_file);
    }
    return 0;
}
