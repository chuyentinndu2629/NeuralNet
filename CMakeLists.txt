cmake_minimum_required(VERSION 3.15)
project(neuralnet VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(neuralnet)
target_compile_options(neuralnet PRIVATE -Wall -Wextra -O3)

target_sources(neuralnet PRIVATE
    src/main.cpp
    src/console.cpp
    src/renderer.cpp

    glad/src/gl.c
)

target_include_directories(neuralnet PRIVATE
    include/
    src/
    glad/include/
)

# Crucial! Add GLFW and OpenGL into the packages list
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

target_link_libraries(neuralnet PRIVATE
    glfw
)