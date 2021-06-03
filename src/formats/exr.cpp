#include <cstring>

#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>

#include "sol/formats/exr.h"

namespace sol::exr {

std::optional<Image> load(const std::string_view& path) {
    std::string path_string(path);

    EXRVersion version;
    if (ParseEXRVersionFromFile(&version, path_string.c_str()) != 0 || version.multipart)
        return std::nullopt;

    EXRHeader exr_header;
    InitEXRHeader(&exr_header);

    const char* err = nullptr;
    if (ParseEXRHeaderFromFile(&exr_header, &version, path_string.c_str(), &err) != 0) {
        FreeEXRErrorMessage(err);
        return std::nullopt;
    }

    for (int i = 0; i < exr_header.num_channels; ++i)
        exr_header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;

    EXRImage exr_image;
    InitEXRImage(&exr_image);

    if (LoadEXRImageFromFile(&exr_image, &exr_header, path_string.c_str(), &err) != 0) {
        FreeEXRHeader(&exr_header);
        FreeEXRErrorMessage(err);
        return std::nullopt;
    }

    Image image(exr_image.width, exr_image.height, exr_header.num_channels);
    for (int i = 0; i < exr_header.num_channels; ++i)
        std::memcpy(image.channel(i).get(), exr_image.images[i], sizeof(float) * exr_image.width * exr_image.height);

    FreeEXRImage(&exr_image);
    FreeEXRHeader(&exr_header);
    return std::make_optional(std::move(image));
}

bool save(const Image& image, const std::string_view& path) {
    std::string path_string(path);

    EXRHeader exr_header;
    EXRImage exr_image;
    InitEXRHeader(&exr_header);
    InitEXRImage(&exr_image);

    std::vector<float*> images(image.channel_count());
    for (size_t i = 0; i < image.channel_count(); ++i)
        images[i] = image.channel(i).get();

    exr_image.images = reinterpret_cast<unsigned char**>(images.data());
    exr_image.width = image.width();
    exr_image.height = image.height();

    std::vector<EXRChannelInfo> channel_infos(image.channel_count());
    exr_header.num_channels = image.channel_count();
    exr_header.channels = channel_infos.data();

    size_t name_len = sizeof(EXRChannelInfo::name);
    switch (image.channel_count()) {
        default:
            for (size_t i = 4; i < image.channel_count(); ++i) {
                auto channel_name = "Channel_" + std::to_string(i);
                std::strncpy(exr_header.channels[i].name, channel_name.c_str(), name_len);
            }
            [[fallthrough]];
        case 4: std::strncpy(exr_header.channels[3].name, "A", name_len); [[fallthrough]];
        case 3: std::strncpy(exr_header.channels[2].name, "B", name_len); [[fallthrough]];
        case 2: std::strncpy(exr_header.channels[1].name, "G", name_len); [[fallthrough]];
        case 1: std::strncpy(exr_header.channels[0].name, "R", name_len); [[fallthrough]];
        case 0:
            break;
    }

    std::vector<int> pixel_types(image.channel_count(), TINYEXR_PIXELTYPE_FLOAT);
    exr_header.pixel_types = pixel_types.data();
    exr_header.requested_pixel_types = pixel_types.data();

    const char* err = nullptr;
    if (SaveEXRImageToFile(&exr_image, &exr_header, path_string.c_str(), &err) != 0) {
        FreeEXRErrorMessage(err);
        return false;
    }

    return true;
}

} // namespace sol::exr
