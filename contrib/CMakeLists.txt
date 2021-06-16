add_library(tomlplusplus INTERFACE)
target_include_directories(tomlplusplus INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tomlplusplus/include>)

add_library(tinyexr INTERFACE)
target_include_directories(tinyexr INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tinyexr>)
