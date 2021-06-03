#ifndef SOL_FORMATS_JPEG_H
#define SOL_FORMATS_JPEG_H

#include <string_view>

#include "sol/image.h"

namespace sol::jpeg {

std::optional<Image> load(const std::string_view&);
bool save(const Image&, const std::string_view&);

} // namespace sol::jpeg

#endif
