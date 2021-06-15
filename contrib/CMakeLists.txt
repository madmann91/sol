add_library(pcg INTERFACE)
target_include_directories(pcg INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/pcg-cpp/include>
    $<INSTALL_INTERFACE:include/sol/contrib/pcg-cpp>)
install(FILES
    pcg-cpp/include/pcg_random.hpp
    pcg-cpp/include/pcg_extras.hpp
    pcg-cpp/include/pcg_uint128.hpp
    DESTINATION include/sol/contrib/pcg-cpp)

add_library(tinyexr INTERFACE)
target_include_directories(tinyexr INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tinyexr>
    $<INSTALL_INTERFACE:include/sol/contrib/tinyexr>)
install(FILES tinyexr/tinyexr.h DESTINATION include/sol/contrib/tinyexr)
