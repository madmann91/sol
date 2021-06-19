#ifdef SOL_ENABLE_TIFF
#include <tiffio.h>
#endif

#include "formats/tiff.h"

namespace sol::tiff {

#ifdef SOL_ENABLE_TIFF
static void error_handler(const char*, const char*, va_list) {
    throw std::runtime_error("Cannot load/save TIFF image file");
}

std::optional<Image> load(const std::string_view& path) {
    std::string path_string(path);

    TIFFSetErrorHandler(error_handler);
    TIFFSetWarningHandler(error_handler);
    try {
        auto tiff = TIFFOpen(path_string.c_str(), "r");
        if (!tiff)
            return std::nullopt;
        std::unique_ptr<TIFF, decltype(&TIFFClose)> tiff_ptr(tiff, TIFFClose);

        uint32_t width, height;
        TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
        TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);

        auto pixels = std::make_unique<uint32_t[]>(width * height);
        if (!TIFFReadRGBAImageOriented(tiff, width, height, pixels.get(), ORIENTATION_TOPLEFT))
            return std::nullopt;

        Image image(width, height, 4);
        for (size_t y = 0; y < height; y++) {
            for (size_t x = 0; x < width; x++) {
                auto pixel = pixels[y * width + x];
                image.channel(0)[y * width + x] = Image::word_to_component<8>(pixel         & 0xFF);
                image.channel(1)[y * width + x] = Image::word_to_component<8>((pixel >>  8) & 0xFF);
                image.channel(2)[y * width + x] = Image::word_to_component<8>((pixel >> 16) & 0xFF);
                image.channel(3)[y * width + x] = Image::word_to_component<8>((pixel >> 24) & 0xFF);
            }
        }
        return std::make_optional(std::move(image));
    } catch (std::exception& e) {
        return std::nullopt;
    }
}

bool save(const Image& image, const std::string_view& path) {
    std::string path_string(path);

    TIFFSetErrorHandler(error_handler);
    TIFFSetWarningHandler(error_handler);

    try {
        auto tiff = TIFFOpen(path_string.c_str(), "w");
        if (!tiff)
            return false;
        std::unique_ptr<TIFF, decltype(&TIFFClose)> tiff_ptr(tiff, TIFFClose);

        uint32_t width = image.width(), height = image.height();
        size_t channel_count = std::min(image.channel_count(), size_t{3});
        TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, width);
        TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, height);
        TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 32);
        TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, channel_count);
        TIFFSetField(tiff, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
        TIFFSetField(tiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
        TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        TIFFSetField(tiff, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
        TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tiff, static_cast<uint32_t>(-1)));
        TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
        TIFFSetField(tiff, TIFFTAG_PREDICTOR, 1);

        auto row = std::make_unique<float[]>(channel_count * width);
        for (size_t y = 0; y < height; y++) {
            for (size_t x = 0; x < width; x++) {
                for (size_t i = 0; i < channel_count; ++i)
                    row[x * channel_count + i] = image.channel(i)[y * width + x];
            }
            TIFFWriteScanline(tiff, row.get(), y, 0);
        }
        return true;
    } catch (std::exception& e) {
        return false;
    }
}
#else
std::optional<Image> load(const std::string_view&) { return std::nullopt; }
bool save(const Image&, const std::string_view&) { return false; }
#endif // SOL_ENABLE_TIFF

} // namespace sol::tiff
