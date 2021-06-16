#ifdef SOL_ENABLE_TIFF
#include <tiffio.h>
#endif

#include "formats/tiff.h"

namespace sol::tiff {

#ifdef SOL_ENABLE_TIFF
static void error_handler(const char*, const char*, va_list) {}

std::optional<Image> load(const std::string_view& path) {
    std::string path_string(path);

    TIFFSetErrorHandler(error_handler);
    TIFFSetWarningHandler(error_handler);
    auto tiff = TIFFOpen(path_string.c_str(), "r");
    if (!tiff)
        return std::nullopt;

    uint32_t width, height;
	TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
	TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
    Image image(width, height, 4);

    auto row = std::make_unique<uint32_t[]>(width);
    for (size_t y = 0; y < height; y++) {
        TIFFReadScanline(tiff, row.get(), y);
        for (size_t x = 0; x < width; x++) {
            auto pixel = row[x];
            image.channel(0)[y * width + x] = Image::word_to_component<8>(pixel         & 0xFF);
            image.channel(1)[y * width + x] = Image::word_to_component<8>((pixel >>  8) & 0xFF);
            image.channel(2)[y * width + x] = Image::word_to_component<8>((pixel >> 16) & 0xFF);
            image.channel(3)[y * width + x] = Image::word_to_component<8>((pixel >> 24) & 0xFF);
        }
    }

    TIFFClose(tiff);
    return std::make_optional(std::move(image));
}

bool save(const Image& image, const std::string_view& path) {
    std::string path_string(path);

    TIFFSetErrorHandler(error_handler);
    TIFFSetWarningHandler(error_handler);
    auto tiff = TIFFOpen(path_string.c_str(), "w");
    if (!tiff)
        return false;

    uint32_t width, height;
	TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
	TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, &height);

    auto row = std::make_unique<uint32_t[]>(width);
    for (size_t y = 0; y < height; y++) {
        auto row = std::make_unique<uint32_t[]>(width);
        for (size_t x = 0; x < width; x++) {
            uint32_t r = 0, g = 0, b = 0, a = 0;
            switch (image.channel_count()) {
                default:
                case 4: a = Image::component_to_word<8>(image.channel(3)[y * width + x]); [[fallthrough]];
                case 3: b = Image::component_to_word<8>(image.channel(2)[y * width + x]); [[fallthrough]];
                case 2: g = Image::component_to_word<8>(image.channel(1)[y * width + x]); [[fallthrough]];
                case 1: r = Image::component_to_word<8>(image.channel(0)[y * width + x]); [[fallthrough]];
                case 0:
                    break;
            }
            row[x] = r | (g << 8) | (b << 16) | (a << 24);
        }
        TIFFWriteScanline(tiff, row.get(), y);
    }
    TIFFClose(tiff);
    return true;
}
#else
std::optional<Image> load(const std::string_view&) { return std::nullopt; }
bool save(const Image&, const std::string_view&) { return false; }
#endif // SOL_ENABLE_TIFF

} // namespace sol::tiff
