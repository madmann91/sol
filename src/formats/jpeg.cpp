#ifdef SOL_ENABLE_JPEG
#include <exception>
#include <cstdio>

#include <jpeglib.h>
#endif

#include "formats/jpeg.h"

namespace sol::jpeg {

#ifdef SOL_ENABLE_JPEG
static void error_exit(j_common_ptr) { throw std::runtime_error("Cannot load/save JPEG image file"); }
static void output_message(j_common_ptr) {}

std::optional<Image> load(const std::string_view& path) {
    std::string path_string(path);
    auto file = fopen(path_string.c_str(), "rb");
    if (!file)
        return std::nullopt;
    std::unique_ptr<FILE, decltype(&fclose)> file_ptr(file, fclose);

    jpeg_error_mgr jerr;
    jpeg_std_error(&jerr);
    jerr.error_exit = error_exit;
    jerr.output_message = output_message;

    std::optional<Image> final_image;
    jpeg_decompress_struct decompress_info;
    try {
        decompress_info.err = &jerr;
        jpeg_create_decompress(&decompress_info);
        std::unique_ptr<jpeg_decompress_struct, decltype(&jpeg_destroy_decompress)>
            decompress_ptr(&decompress_info, jpeg_destroy_decompress);

        jpeg_stdio_src(&decompress_info, file);

        jpeg_read_header(&decompress_info, TRUE);
        jpeg_start_decompress(&decompress_info);
        size_t width = decompress_info.output_width;
        size_t height = decompress_info.output_height;
        size_t channel_count = decompress_info.output_components;
        Image image(width, height, channel_count);

        auto row = std::make_unique<JSAMPLE[]>(image.width() * image.channel_count());
        for (size_t y = 0; y < height; y++) {
            auto row_ptr = row.get();
            jpeg_read_scanlines(&decompress_info, &row_ptr, 1);
            for (size_t x = 0; x < width; ++x) {
                for (size_t i = 0; i < channel_count; ++i) {
                    image.channel(i)[y * width + x] =
                        Image::word_to_component<BITS_IN_JSAMPLE>(row[x * channel_count + i]);
                }
            }
        }
        jpeg_finish_decompress(&decompress_info);
        jpeg_destroy_decompress(&decompress_info);
        final_image = std::make_optional(std::move(image));
    } catch(std::exception& e) {
        // Do nothing
    }

    jpeg_destroy_decompress(&decompress_info);
    return final_image;
}

bool save(const Image& image, const std::string_view& path) {
    std::string path_string(path);
    auto file = fopen(path_string.c_str(), "wb");
    if (!file)
        return false;
    std::unique_ptr<FILE, decltype(&fclose)> file_ptr(file, fclose);

    jpeg_error_mgr jerr;
    jpeg_std_error(&jerr);
    jerr.error_exit = error_exit;
    jerr.output_message = output_message;

    size_t channel_count = 3;
    jpeg_compress_struct compress_info;
    try {
        compress_info.err = &jerr;
        jpeg_create_compress(&compress_info);
        std::unique_ptr<jpeg_compress_struct, decltype(&jpeg_destroy_compress)>
            compress_ptr(&compress_info, jpeg_destroy_compress);

        jpeg_stdio_dest(&compress_info, file);

        compress_info.image_width = image.width();
        compress_info.image_height = image.height();
        compress_info.input_components = channel_count;
        compress_info.in_color_space = JCS_RGB;
        jpeg_set_defaults(&compress_info);
        jpeg_start_compress(&compress_info, TRUE);

        auto row = std::make_unique<JSAMPLE[]>(image.width() * channel_count);
        for (size_t y = 0; y < image.height(); y++) {
            std::fill(row.get(), row.get() + image.width() * channel_count, 0);
            for (size_t x = 0; x < image.width(); ++x) {
                for (size_t i = 0, n = std::min(image.channel_count(), channel_count); i < n; ++i) {
                    row[x * channel_count + i] =
                        Image::component_to_word<BITS_IN_JSAMPLE>(image.channel(i)[y * image.width() + x]);
                }
            }
            auto row_ptr = row.get();
            jpeg_write_scanlines(&compress_info, &row_ptr, 1);
        }
        jpeg_finish_compress(&compress_info);
        return true;
    } catch(std::exception& e) {
        return false;
    }
}
#else
std::optional<Image> load(const std::string_view&) { return std::nullopt; }
bool save(const Image&, const std::string_view&) { return false; }
#endif // SOL_ENABLE_JPEG

} // namespace sol::jpeg
