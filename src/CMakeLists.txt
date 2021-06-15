add_library(sol
    formats/png.cpp
    formats/jpeg.cpp
    formats/tiff.cpp
    formats/exr.cpp
    algorithms/path_tracer.cpp
    triangle_mesh.cpp
    image.cpp
    cameras.cpp
    lights.cpp
    bsdfs.cpp
    scene.cpp)

set_target_properties(sol PROPERTIES
    CXX_STANDARD 20
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(proto REQUIRED)
find_package(bvh REQUIRED)

target_link_libraries(sol PUBLIC proto::proto pcg)
target_link_libraries(sol PRIVATE bvh::bvh)
target_include_directories(sol PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>)

# Add image libraries
target_link_libraries(sol PRIVATE tinyexr)

find_package(PNG QUIET)
if (PNG_FOUND)
    message(STATUS "Adding PNG support")
    target_compile_definitions(sol PRIVATE -DSOL_ENABLE_PNG)
    target_link_libraries(sol PRIVATE PNG::PNG)
endif ()

find_package(JPEG QUIET)
if (JPEG_FOUND)
    message(STATUS "Adding JPEG support")
    target_compile_definitions(sol PRIVATE -DSOL_ENABLE_JPEG)
    target_link_libraries(sol PRIVATE JPEG::JPEG)
endif ()

find_package(TIFF QUIET)
if (TIFF_FOUND)
    message(STATUS "Adding TIFF support")
    target_compile_definitions(sol PRIVATE -DSOL_ENABLE_TIFF)
    target_link_libraries(sol PRIVATE TIFF::TIFF)
endif ()

include(CMakePackageConfigHelpers)

# Allow using this library by locating the config file from the build directory
export(TARGETS sol tinyexr pcg NAMESPACE sol:: FILE sol-targets.cmake)
configure_package_config_file(
    ../cmake/sol-config.cmake.in
    sol-config.cmake
    INSTALL_DESTINATION .
    INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR})
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/sol-config-version.cmake COMPATIBILITY SameMajorVersion)

# Allow using this library by installing it
configure_package_config_file(
    ../cmake/sol-config.cmake.in
    sol-config-install.cmake
    INSTALL_DESTINATION lib/cmake)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sol-config-install.cmake RENAME sol-config.cmake DESTINATION lib/cmake)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sol-config-version.cmake DESTINATION lib/cmake)
install(DIRECTORY ../include DESTINATION .)
install(TARGETS sol tinyexr pcg EXPORT sol-targets)
install(EXPORT sol-targets NAMESPACE sol:: DESTINATION lib/cmake)
