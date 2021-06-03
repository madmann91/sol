#ifndef SOL_FORMATS_TIFF_H
#define SOL_FORMATS_TIFF_H

#include <string_view>

#include "sol/image.h"

namespace sol::tiff {

std::optional<Image> load(const std::string_view&);
bool save(const Image&, const std::string_view&);

} // namespace sol::tiff

#endif
