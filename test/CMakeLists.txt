add_executable(driver driver.cpp)
target_link_libraries(driver PUBLIC sol)
set_target_properties(driver PROPERTIES
    CXX_STANDARD 20
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

add_test(NAME driver_cornell_box COMMAND driver ${CMAKE_CURRENT_SOURCE_DIR}/data/cornell_box.toml)
