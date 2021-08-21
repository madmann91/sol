set(SOL_MULTITHREADING_FRAMEWORK None CACHE STRING "Multithreading framework to use in SoL")
set_property(CACHE SOL_MULTITHREADING_FRAMEWORK PROPERTY STRINGS None TBB OpenMP)

add_library(sol
    formats/png.cpp
    formats/jpeg.cpp
    formats/tiff.cpp
    formats/exr.cpp
    formats/obj.cpp
    algorithms/path_tracer.cpp
    triangle_mesh.cpp
    image.cpp
    cameras.cpp
    lights.cpp
    bsdfs.cpp
    scene.cpp
    scene_loader.cpp
    shapes.cpp
    textures.cpp
    render_job.cpp)

set_target_properties(sol PROPERTIES
    CXX_STANDARD 20
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(proto REQUIRED)
find_package(bvh REQUIRED)
find_package(Threads REQUIRED)

if (SOL_MULTITHREADING_FRAMEWORK STREQUAL "TBB")
    find_package(TBB REQUIRED)
    if (TBB_FOUND)
        target_compile_definitions(sol PUBLIC -DSOL_ENABLE_TBB)
        target_link_libraries(sol PUBLIC TBB::tbb)
        set(SOL_DEPENDS_TBB "find_dependency(TBB)")
        message(STATUS "Building SoL with TBB")
    endif ()
elseif (SOL_MULTITHREADING_FRAMEWORK STREQUAL "OpenMP")
    find_package(OpenMP REQUIRED)
    if (OpenMP_FOUND)
        target_compile_definitions(sol PUBLIC -DSOL_ENABLE_OMP)
        target_link_libraries(sol PUBLIC OpenMP::OpenMP_CXX)
        set(SOL_DEPENDS_OMP "find_dependency(OpenMP)")
        message(STATUS "Building SoL with OpenMP")
    endif ()
else ()
    message(STATUS "Building SoL without multithreading support")
endif ()

target_link_libraries(sol PUBLIC proto::proto par::par Threads::Threads)
target_link_libraries(sol PRIVATE bvh::bvh tomlplusplus tinyexr)
target_include_directories(sol PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(sol PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>)

set_target_properties(sol PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# Add optional image libraries
find_package(PNG QUIET)
if (PNG_FOUND)
    message(STATUS "Adding PNG support")
    target_compile_definitions(sol PUBLIC -DSOL_ENABLE_PNG)
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

set(CONTRIB_TARGETS tinyexr tomlplusplus)

# Allow using this library by locating the config file from the build directory
export(TARGETS sol ${CONTRIB_TARGETS} NAMESPACE sol:: FILE ../sol-targets.cmake)
configure_package_config_file(
    ../cmake/sol-config.cmake.in
    ../sol-config.cmake
    INSTALL_DESTINATION .
    INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR})
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/../sol-config-version.cmake COMPATIBILITY SameMajorVersion)

# Allow using this library by installing it
configure_package_config_file(
    ../cmake/sol-config.cmake.in
    ../sol-config-install.cmake
    INSTALL_DESTINATION lib/cmake)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/../sol-config-install.cmake RENAME sol-config.cmake DESTINATION lib/cmake)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/../sol-config-version.cmake DESTINATION lib/cmake)
install(DIRECTORY ../include DESTINATION .)
install(TARGETS sol ${CONTRIB_TARGETS} EXPORT sol-targets)
install(EXPORT sol-targets NAMESPACE sol:: DESTINATION lib/cmake)
