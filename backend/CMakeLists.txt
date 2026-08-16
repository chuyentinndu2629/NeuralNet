cmake_minimum_required(VERSION 3.15)
project(neuralnet VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE)  # Fix on Windows PowerShell?

include(FetchContent)

# ------------------------------------------------------------------------------
# Dependencies via FetchContent
# ------------------------------------------------------------------------------

# CPR Options - Force using system curl instead of building bundled curl
# set(CPR_USE_SYSTEM_CURL ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json
)

FetchContent_MakeAvailable(glfw cpr json)

# System Packages
# find_package(OpenGL REQUIRED)
# find_package(nlohmann_json REQUIRED)

# ------------------------------------------------------------------------------
# Targets
# ------------------------------------------------------------------------------

# GLAD Static Target
# add_library(glad STATIC glad/src/gl.c)
# target_include_directories(glad PUBLIC glad/include/)

# Main Executable
add_executable(neuralnet
    src/main.cpp
    src/console.cpp
    src/renderer.cpp
)

target_compile_options(neuralnet PRIVATE -Wall -Wextra -O3)

target_include_directories(neuralnet PRIVATE
    include/
    src/
)

target_link_libraries(neuralnet PRIVATE
    cpr::cpr
    nlohmann_json::nlohmann_json
)