cmake_minimum_required(VERSION 3.16)
project(airwar LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

include(FetchContent)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 2.6.x)
FetchContent_MakeAvailable(SFML)

add_executable(airwar main.cpp)

if(MSVC)
    target_compile_options(airwar PRIVATE "/utf-8")
endif()

target_link_libraries(airwar PRIVATE sfml-graphics)
target_compile_features(airwar PRIVATE cxx_std_17)

add_custom_command(TARGET airwar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/plane.png"
        "$<TARGET_FILE_DIR:airwar>/plane.png")

add_custom_command(TARGET airwar POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/font.ttf"
    "$<TARGET_FILE_DIR:airwar>/font.ttf")

add_executable(sample1 sample1.cpp)
target_link_libraries(sample1 PRIVATE sfml-graphics)
target_compile_features(sample1 PRIVATE cxx_std_17)

add_executable(sample2 sample2.cpp)
target_link_libraries(sample2 PRIVATE sfml-graphics)
target_compile_features(sample2 PRIVATE cxx_std_17)