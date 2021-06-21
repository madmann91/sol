#include <string_view>
#include <optional>
#include <iostream>
#include <sstream>
#include <chrono>

#include <sol/scene.h>
#include <sol/render_job.h>

struct Options {
    std::string job_file;
    std::string out_file = "render.exr";
    sol::Image::Format out_format = sol::Image::Format::Auto;
};

static void usage() {
    Options default_options;
    std::cout <<
        "Usage: driver [options] job.toml\n"
        "Available options:\n"
        "  -h          --help  Shows this message\n"
        "  -o <image>          Sets the output image file name (default: \'"
        << default_options.out_file << "\')\n"
        "  -f <format>         Sets the output image format (default: auto)\n"
        "\nValid image formats:\n"
        "  auto, png, jpeg, exr, tiff"
        << std::endl;
}

bool must_have_arg(int i, int argc, char** argv) {
    if (i == argc - 1) {
        std::cerr << "Missing argument for option '" << argv[i] << "'" << std::endl;
        return false;
    }
    return true;
}

static std::optional<Options> parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            using namespace std::literals::string_view_literals;
            if (argv[i] == "-h"sv || argv[i] == "--help"sv) {
                usage();
                return std::nullopt;
            } else if (argv[i] == "-o"sv) {
                if (!must_have_arg(i++, argc, argv))
                    return std::nullopt;
                options.out_file = argv[i];
            } else if (argv[i] == "-f"sv) {
                if (!must_have_arg(i++, argc, argv))
                    return std::nullopt;
                     if (argv[i] == "auto"sv) options.out_format = sol::Image::Format::Auto;
                else if (argv[i] == "png"sv)  options.out_format = sol::Image::Format::Png;
                else if (argv[i] == "jpeg"sv) options.out_format = sol::Image::Format::Jpeg;
                else if (argv[i] == "tiff"sv) options.out_format = sol::Image::Format::Tiff;
                else if (argv[i] == "exr"sv)  options.out_format = sol::Image::Format::Exr;
                else {
                    std::cerr << "Unknown image format '" << argv[i] << "'" << std::endl;
                    return std::nullopt;
                }
            } else {
                std::cerr << "Unknown option '" << argv[i] << "'" << std::endl;
                return std::nullopt;
            }
        } else if (options.job_file.empty()) {
            options.job_file = argv[i];
        } else {
            std::cerr << "Too many input files" << std::endl;
            return std::nullopt;
        }
    }
    if (options.job_file.empty()) {
        std::cerr <<
            "Missing render job file\n"
            "Type 'driver -h' to show usage" << std::endl;
        return std::nullopt;
    }
    return std::make_optional(options);
}

static bool save_image(const sol::Image& image, const Options& options) {
    if (!image.save(options.out_file, options.out_format)) {
        if (options.out_format != sol::Image::Format::Auto &&
            image.save(options.out_file, sol::Image::Format::Auto)) {
            std::cout << "Image could not be saved in the given format, so the default format was used instead" << std::endl;
            return true;
        }
        std::cout << "Could not save image to '" << options.out_file << "'" << std::endl;
        return false;
    }
    std::cout << "Image was saved to '" << options.out_file << "'" << std::endl;
    return true;
}

int main(int argc, char** argv) {
    auto options = parse_options(argc, argv);
    if (!options)
        return 1; 

    std::ostringstream err_stream;
    auto render_job = sol::RenderJob::load(options->job_file, {}, &err_stream);
    if (!render_job) {
        std::cerr << err_stream.str() << std::endl;
        return 1;
    }

    std::cout
        << "Scene summary:\n"
        << "    " << render_job->scene->bsdfs.size() << " BSDF(s)\n"
        << "    " << render_job->scene->lights.size() << " light(s)\n"
        << "    " << render_job->scene->textures.size() << " texture(s)\n"
        << "    " << render_job->scene->images.size() << " image(s)\n";

    auto render_start = std::chrono::system_clock::now();
    render_job->start();
    std::cout << "Rendering started..." << std::endl;
    render_job->wait();
    auto render_end = std::chrono::system_clock::now();
    auto rendering_ms = std::chrono::duration_cast<std::chrono::milliseconds>(render_end - render_start).count();
    std::cout << "Rendering finished in " << rendering_ms << "ms" << std::endl;

    if (!options->out_file.empty()) {
        render_job->output->scale(1.0f / static_cast<float>(render_job->sample_count));
        if (!save_image(*render_job->output, *options))
            return 1;
    }
    return 0;
}
