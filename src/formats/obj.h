#ifndef SOL_FORMATS_OBJ_H
#define SOL_FORMATS_OBJ_H

#include <ostream>

#include "scene_loader.h"

namespace sol::obj {

std::unique_ptr<Scene::Node> load(SceneLoader&, const std::string_view&);

} // namespace sol::obj

#endif
