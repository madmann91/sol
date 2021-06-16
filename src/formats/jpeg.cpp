#ifdef SOL_ENABLE_JPEG
#include <exception>
#include <cstdio>

#include <jpeglib.h>
#endif

#include "formats/jpeg.h"

namespace sol::jpeg {

#ifdef SOL_ENABLE_JPEG
static void error_exit(j_common_ptr) { throw std::runtime_error("cannot load/save JPEG image file"); }
static void output_message(j_common_ptr) {}

std::optional<Image> load(const std::string_view& path) {
    std::string path_string(path);
    auto close_file = [] (FILE* file) { if (file) fclose(file); };
    auto file = std::unique_ptr<FILE, decltype(close_file)>(fopen(path_string.c_str(), "rb"), close_file);
    if (!file)
        return std::nullopt;

    jpeg_error_mgr jerr;
    jerr.error_exit = error_exit;
    jerr.output_message = output_message;

    jpeg_decompress_struct decompress_info;
    decompress_info.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&decompress_info);
    jpeg_stdio_src(&decompress_info, file.get());

    Image final_image;
    try {
        jpeg_read_header(&decompress_info, true);
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
        final_image = std::move(image);
    } catch(std::exception& e) {
        jpeg_destroy_decompress(&decompress_info);
        return std::nullopt;
    }

    return std::make_optional(std::move(final_image));
}

bool save(const Image& image, const std::string_view& path) {
    std::string path_string(path);
    auto close_file = [] (FILE* file) { if (file) fclose(file); };
    auto file = std::unique_ptr<FILE, decltype(close_file)>(fopen(path_string.c_str(), "wb"), close_file);
    if (!file)
        return false;

    jpeg_error_mgr jerr;
    jerr.error_exit = error_exit;
    jerr.output_message = output_message;

    size_t channel_count = 3;

    jpeg_compress_struct compress_info;
    compress_info.err = jpeg_std_error(&jerr);
    compress_info.image_width = image.width();
    compress_info.image_height = image.height();
    compress_info.input_components = channel_count;
    compress_info.in_color_space = JCS_RGB;
    jpeg_create_compress(&compress_info);
    jpeg_stdio_dest(&compress_info, file.get());

    try {
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
        jpeg_destroy_compress(&compress_info);
    } catch(std::exception& e) {
        jpeg_destroy_compress(&compress_info);
        return false;
    }

    return true;
}
#else
std::optional<Image> load(const std::string_view&) { return std::nullopt; }
bool save(const Image&, const std::string_view&) { return false; }
#endif // SOL_ENABLE_JPEG

} // namespace sol::jpeg
