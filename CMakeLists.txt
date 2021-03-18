cmake_minimum_required(VERSION 3.0)

# Project information.
project(glsl-include
        VERSION 1.0
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(lib)
add_subdirectory(src)