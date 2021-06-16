#ifndef SOL_FORMATS_EXR_H
#define SOL_FORMATS_EXR_H

#include <string_view>

#include "sol/image.h"

namespace sol::exr {

std::optional<Image> load(const std::string_view&);
bool save(const Image&, const std::string_view&);

} // namespace sol::exr

#endif
