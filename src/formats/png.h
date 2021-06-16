#ifndef SOL_FORMATS_PNG_H
#define SOL_FORMATS_PNG_H

#include <string_view>

#include "sol/image.h"

namespace sol::png {

std::optional<Image> load(const std::string_view&);
bool save(const Image&, const std::string_view&);

} // namespace sol::png

#endif
