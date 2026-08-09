cmake_minimum_required(VERSION 3.15)
project(neuralnet VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

include(FetchContent)

# ------------------------------------------------------------------------------
# Dependencies via FetchContent
# ------------------------------------------------------------------------------

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)

# CPR Options - Force using system curl instead of building bundled curl
set(CPR_USE_SYSTEM_CURL ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG        1.10.5
)

FetchContent_MakeAvailable(glfw cpr)

# System Packages
find_package(OpenGL REQUIRED)
find_package(nlohmann_json REQUIRED)

# ------------------------------------------------------------------------------
# Targets
# ------------------------------------------------------------------------------

# GLAD Static Target
add_library(glad STATIC glad/src/gl.c)
target_include_directories(glad PUBLIC glad/include/)

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
    glad
    glfw
    OpenGL::GL
    cpr::cpr
    nlohmann_json::nlohmann_json
)

# 1. Automatically collect all shader files
file(GLOB_RECURSE SHADER_FILES 
    CONFIGURE_DEPENDS 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*"
)

# 2. Use CMAKE_CURRENT_BINARY_DIR instead of $<TARGET_FILE_DIR:neuralnet>
add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders"
            "${CMAKE_CURRENT_BINARY_DIR}/shaders"
    SOURCES ${SHADER_FILES}
    COMMENT "Syncing shaders to output directory..."
)

# 3. Force your executable to depend on the copy task
add_dependencies(neuralnet copy_shaders)