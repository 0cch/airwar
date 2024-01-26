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
target_link_libraries(airwar PRIVATE sfml-graphics)
target_compile_features(airwar PRIVATE cxx_std_17)