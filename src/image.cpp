#include <cassert>

#include "sol/image.h"

#include "formats/png.h"
#include "formats/jpeg.h"
#include "formats/tiff.h"
#include "formats/exr.h"

namespace sol {

bool Image::save(const std::string_view& path, Format format) const {
    if (format == Format::Auto)
        format = Format::Exr;
    switch (format) {
        case Format::Png:  return png ::save(*this, path);
        case Format::Jpeg: return jpeg::save(*this, path);
        case Format::Tiff: return tiff::save(*this, path);
        case Format::Exr:  return exr ::save(*this, path);
        default:
            assert(false);
            return false;
    }
}

std::optional<Image> Image::load(const std::string_view& path, Format format) {
    switch (format) {
        case Format::Png:  return png ::load(path);
        case Format::Jpeg: return jpeg::load(path);
        case Format::Tiff: return tiff::load(path);
        case Format::Exr:  return exr ::load(path);
        default:
            if (auto image = png ::load(path)) return image;
            if (auto image = jpeg::load(path)) return image;
            if (auto image = tiff::load(path)) return image;
            if (auto image = exr ::load(path)) return image;
            return std::nullopt;
    }
}

} // namespace sol
