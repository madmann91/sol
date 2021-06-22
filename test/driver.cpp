#include <optional>
#include <iostream>
#include <sstream>
#include <chrono>
#include <string>
#include <string_view>
#include <unordered_set>

#include <sol/scene.h>
#include <sol/image.h>
#include <sol/render_job.h>
#include <sol/algorithms/path_tracer.h>

static const std::unordered_set<std::string> valid_algorithms = { "path_tracer" };

struct Options {
    std::string scene_file;
    std::string out_file = "render.exr";
    sol::Image::Format out_format = sol::Image::Format::Auto;

    std::string algorithm = "path_tracer";

    size_t output_width = 1080;
    size_t output_height = 720;
    size_t samples_per_pixel = 16;

    size_t max_path_len = 64;
    size_t min_rr_path_len = 3;
    float min_survival_prob = 0.05f;
    float max_survival_prob = 0.75f;
    float ray_offset  = 1e-5f;
};

static void usage() {
    Options default_options;
    std::cout <<
        "Usage: driver [options] scene.toml\n"
        "Available options:\n"
        "  -h         --help                      Shows this message\n"
        "  -o <file>  --output <file>             Sets the output image file name (default: '"
        << default_options.out_file << "')\n"
        "  -f <fmt>   --format <fmt>              Sets the output image format (default: auto)\n"
        "  -a <algo>  --algorithm <algo>          Sets the rendering algorithm to use (default: '"
        << default_options.algorithm << "')\n"
        "  -s <n>     --samples <n>               Sets the number of samples per pixel (default: "
        << default_options.samples_per_pixel << ")\n"
        "  -w <n>     --width <n>                 Sets the output width, in pixels (default: "
        << default_options.output_width << ")\n"
        "  -h <n>     --height <n>                Sets the output height, in pixels (default: "
        << default_options.output_height << ")\n"
        "             --max-path-len <len>        Sets the maximum path length (default: "
        << default_options.max_path_len << ")\n"
        "             --min-survival-prob <prob>  Sets the minimum Russian Roulette survival probability (default: "
        << default_options.min_survival_prob << ")\n"
        "             --max-survival-prob <prob>  Sets the maximum Russian Roulette survival probability (default: "
        << default_options.max_survival_prob << ")\n"
        "             --min-rr-path-len <len>     Sets the minimum path length after which Russian Roulette starts (default: "
        << default_options.min_rr_path_len << ")\n"
        "             --ray-offset <off>          Sets the ray offset used to avoid self intersections (default: "
        << default_options.ray_offset << ")\n"
        "\nValid image formats:\n"
        "  auto, png, jpeg, exr, tiff\n"
        "\nValid algorithms:\n  ";
    for (auto it = valid_algorithms.begin(); it != valid_algorithms.end();) {
        std::cout << *it;
        auto next = std::next(it);
        if (next != valid_algorithms.end())
            std::cout << ", ";
        it = next;
    }
    std::cout << "\n" << std::endl;
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
            } else if (argv[i] == "-a"sv || argv[i] == "--algorithm"sv) {
                if (!must_have_arg(i++, argc, argv))
                    return std::nullopt;
                options.algorithm = argv[i];
                if (!valid_algorithms.contains(options.algorithm)) {
                    std::cerr << "Unknown rendering algorithm '" << argv[i] << "'" << std::endl;
                    return std::nullopt;
                }
            } else if (argv[i] == "-w"sv || argv[i] == "--width"sv) {
                if (!must_have_arg(i++, argc, argv))
                    return std::nullopt;
                options.output_width = std::strtoul(argv[i], NULL, 10);
            } else if (argv[i] == "-h"sv || argv[i] == "--height"sv) {
                if (!must_have_arg(i++, argc, argv))
                    return std::nullopt;
                options.output_height = std::strtoul(argv[i], NULL, 10);
            } else if (argv[i] == "-spp"sv || argv[i] == "--samples-per-pixel"sv) {
                if (!must_have_arg(i++, argc, argv))
                    return std::nullopt;
                options.samples_per_pixel = std::strtoul(argv[i], NULL, 10);
            } else if (argv[i] == "--max-path-len"sv) {
                if (!must_have_arg(i++, argc, argv))
                    return std::nullopt;
                options.max_path_len = std::strtoul(argv[i], NULL, 10);
            } else if (argv[i] == "--min-survival-prob"sv) {
                if (!must_have_arg(i++, argc, argv))
                    return std::nullopt;
                options.min_survival_prob = std::strtof(argv[i], NULL);
            } else if (argv[i] == "--max-survival-prob"sv) {
                if (!must_have_arg(i++, argc, argv))
                    return std::nullopt;
                options.max_survival_prob = std::strtof(argv[i], NULL);
            } else if (argv[i] == "--min-rr-path-len"sv) {
                if (!must_have_arg(i++, argc, argv))
                    return std::nullopt;
                options.min_rr_path_len = std::strtoul(argv[i], NULL, 10);
            } else if (argv[i] == "--ray-offset"sv) {
                if (!must_have_arg(i++, argc, argv))
                    return std::nullopt;
                options.ray_offset = std::strtof(argv[i], NULL);
            } else {
                std::cerr << "Unknown option '" << argv[i] << "'" << std::endl;
                return std::nullopt;
            }
        } else if (options.scene_file.empty()) {
            options.scene_file = argv[i];
        } else {
            std::cerr << "Too many input files" << std::endl;
            return std::nullopt;
        }
    }
    if (options.scene_file.empty()) {
        std::cerr <<
            "Missing scene file\n"
            "Type 'driver -h' to show usage" << std::endl;
        return std::nullopt;
    }
    return std::make_optional(options);
}

static bool save_image(const sol::Image& image, const Options& options) {
    auto format = options.out_format;
    if (format == sol::Image::Format::Auto) {
        // Try to guess the format from the file extension
        if (options.out_file.ends_with(".png"))
            format = sol::Image::Format::Png;
        if (options.out_file.ends_with(".jpg") || options.out_file.ends_with(".jpeg"))
            format = sol::Image::Format::Jpeg;
        if (options.out_file.ends_with(".tiff"))
            format = sol::Image::Format::Tiff;
        if (options.out_file.ends_with(".exr"))
            format = sol::Image::Format::Exr;
    }
    if (!image.save(options.out_file, format)) {
        if (format != sol::Image::Format::Auto &&
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

    sol::Scene::Defaults scene_defaults;
    scene_defaults.aspect_ratio =
        static_cast<float>(options->output_width) /
        static_cast<float>(options->output_height);

    std::ostringstream err_stream;
    auto scene = sol::Scene::load(options->scene_file, scene_defaults, &err_stream);
    if (!scene) {
        std::cerr << err_stream.str() << std::endl;
        return 1;
    }

    std::cout
        << "Scene summary:\n"
        << "    " << scene->bsdfs.size() << " BSDF(s)\n"
        << "    " << scene->lights.size() << " light(s)\n"
        << "    " << scene->textures.size() << " texture(s)\n"
        << "    " << scene->images.size() << " image(s)\n";

    std::unique_ptr<sol::Renderer> renderer;
    assert(options->algorithm == "path_tracer");
    renderer = std::make_unique<sol::PathTracer>(*scene, sol::PathTracer::Config {
        .max_path_len      = options->max_path_len,
        .min_rr_path_len   = options->min_rr_path_len,
        .min_survival_prob = options->min_survival_prob,
        .max_survival_prob = options->max_survival_prob,
        .ray_offset        = options->ray_offset
    });

    sol::Image output(options->output_width, options->output_height, 3);
    sol::RenderJob render_job(*renderer, output);

    render_job.sample_count = options->samples_per_pixel;
    render_job.samples_per_frame = options->samples_per_pixel;

    auto render_start = std::chrono::system_clock::now();
    render_job.start();
    std::cout << "Rendering started..." << std::endl;
    render_job.wait();
    auto render_end = std::chrono::system_clock::now();
    auto rendering_ms = std::chrono::duration_cast<std::chrono::milliseconds>(render_end - render_start).count();
    std::cout << "Rendering finished in " << rendering_ms << "ms" << std::endl;

    if (!options->out_file.empty()) {
        output.scale(1.0f / static_cast<float>(render_job.sample_count));
        if (!save_image(output, *options))
            return 1;
    }
    return 0;
}
