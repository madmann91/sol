#include <string_view>
#include <optional>
#include <iostream>
#include <sstream>

#include <sol/scene.h>

struct Options {
    std::string scene_file;
};

static void usage() {
    std::cout <<
        "Usage: driver [options] scene.toml\n"
        "Available options:\n"
        "  -h   --help    Shows this message" << std::endl;
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
        } else {
            std::cerr << "Too many input scene files" << std::endl;
            return std::nullopt;
        }
    }
    if (options.scene_file.empty()) {
        std::cerr << "Missing input scene file" << std::endl;
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
        << "'" << options->scene_file << "' loaded successfully\n"
        << "Summary:\n"
        << "    " << scene->bsdfs.size() << " BSDF(s)\n"
        << "    " << scene->lights.size() << " light(s)\n"
        << "    " << scene->textures.size() << " texture(s)\n"
        << "    " << scene->images.size() << " image(s)\n";

    return 0;
}
