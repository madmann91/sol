#ifndef SOL_FORMATS_OBJ_H
#define SOL_FORMATS_OBJ_H

#include <ostream>

#include "scene_loader.h"
#include "sol/triangle_mesh.h"

namespace sol::obj {

std::unique_ptr<TriangleMesh> load(SceneLoader&, const std::string_view&);

} // namespace sol::obj

#endif
