add_subdirectory(bvh)

add_library(pcg INTERFACE)
target_include_directories(pcg INTERFACE pcg-cpp/include)

add_library(tinyexr INTERFACE)
target_include_directories(tinyexr INTERFACE tinyexr)
