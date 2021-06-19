#ifdef SOL_ENABLE_PNG
#include <cstdio>

#include <png.h>
#endif

#include "formats/png.h"

namespace sol::png {

#ifdef SOL_ENABLE_PNG
std::optional<Image> load(const std::string_view& path) {
    std::string path_string(path);
    auto file = fopen(path_string.c_str(), "rb");
    if (!file)
        return std::nullopt;
    std::unique_ptr<FILE, decltype(&fclose)> file_ptr(file, fclose);

    // Read signature
    png_byte sig[8];
    fread(sig, 1, 8, file);
    if (!png_sig_cmp(sig, 0, 8))
        return std::nullopt;

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr)
        return std::nullopt;

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        return std::nullopt;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        return std::nullopt;
    }

    png_set_sig_bytes(png_ptr, 8);
    png_init_io(png_ptr, file);
    png_read_info(png_ptr, info_ptr);

    size_t width  = png_get_image_width(png_ptr, info_ptr);
    size_t height = png_get_image_height(png_ptr, info_ptr);

    auto color_type = png_get_color_type(png_ptr, info_ptr);
    auto bit_depth  = png_get_bit_depth(png_ptr, info_ptr);

    // Expand paletted and grayscale images to RGB, transform palettes to 8 bit per channel
    if (color_type & PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    if ((color_type & PNG_COLOR_TYPE_GRAY) || (color_type & PNG_COLOR_TYPE_GRAY_ALPHA))
        png_set_gray_to_rgb(png_ptr);
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);

    size_t channel_count = color_type & PNG_COLOR_MASK_ALPHA ? 4 : 3;

    Image image(width, height, channel_count);
    auto row_bytes = std::make_unique<png_byte[]>(width * channel_count);
    for (size_t y = 0; y < height; y++) {
        png_read_row(png_ptr, row_bytes.get(), nullptr);
        for (size_t x = 0; x < width; x++) {
            for (size_t i = 0; i < channel_count; ++i) {
                image.channel(i)[y * width + x] =
                    Image::word_to_component<8>(row_bytes[x * channel_count + i]);
            }
        }
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    return std::make_optional(std::move(image));
}

bool save(const Image& image, const std::string_view& path) {
    std::string path_string(path);
    auto file = fopen(path_string.c_str(), "wb");
    if (!file)
        return false;
    std::unique_ptr<FILE, decltype(&fclose)> file_ptr(file, fclose);

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr)
        return false;

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        return false;
    }

    auto channel_count = proto::clamp<size_t>(image.channel_count(), 3, 4);
    auto row_bytes = std::make_unique<png_byte[]>(image.width() * channel_count);
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return false;
    }

    png_init_io(png_ptr, file);

    png_set_IHDR(
        png_ptr, info_ptr, image.width(), image.height(), 8,
        channel_count == 3 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    for (size_t y = 0; y < image.height(); y++) {
        std::fill(row_bytes.get(), row_bytes.get() + image.width() * channel_count, 0);
        for (size_t x = 0; x < image.width(); x++) {
            for (size_t i = 0, n = std::min(image.channel_count(), channel_count); i < n; ++i) {
                row_bytes[x * channel_count + i] =
                    Image::component_to_word<8>(image.channel(i)[y * image.width() + x]);
            }
        }
        png_write_row(png_ptr, row_bytes.get());
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    return true;
}
#else
std::optional<Image> load(const std::string_view&) { return std::nullopt; }
bool save(const Image&, const std::string_view&) { return false; }
#endif // SOL_ENABLE_PNG

} // namespace sol::png
