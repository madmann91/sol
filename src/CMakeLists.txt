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

target_link_libraries(sol PUBLIC bvh pcg)
target_include_directories(sol PUBLIC ${PROJECT_SOURCE_DIR}/include)

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

set_target_properties(sol PROPERTIES
    CXX_STANDARD 20
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

export(TARGETS sol proto bvh tinyexr pcg NAMESPACE sol:: FILE ${PROJECT_BINARY_DIR}/sol-targets.cmake)
configure_file(../cmake/sol-config.cmake.in ${PROJECT_BINARY_DIR}/sol-config.cmake @ONLY)
