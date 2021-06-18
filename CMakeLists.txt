cmake_minimum_required(VERSION 3.11)
project(sol VERSION 1.0)

add_subdirectory(contrib)
add_subdirectory(src)

# Make sure to only build tests when building this project,
# and not when importing it into another one.
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    if (BUILD_TESTING)
        add_subdirectory(test)
    endif ()
endif ()
