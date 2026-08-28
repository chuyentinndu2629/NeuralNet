cmake_minimum_required(VERSION 3.15)
project(neuralnet VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# System Packages required for networking and multi-threading
find_package(Threads REQUIRED)
find_package(cpr REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(libwebsockets REQUIRED)

# Standalone Asio ships headers only under MSYS2 (no CMake config, no
# pkg-config file), so locate it manually and give it the same
# asio::asio target name the rest of the file already expects.
find_path(ASIO_INCLUDE_DIR asio.hpp)
if(NOT ASIO_INCLUDE_DIR)
    message(FATAL_ERROR "asio.hpp not found — install mingw-w64-ucrt-x86_64-asio")
endif()
add_library(asio INTERFACE)
add_library(asio::asio ALIAS asio)
target_include_directories(asio INTERFACE ${ASIO_INCLUDE_DIR})
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)

# include(FetchContent)

# # ------------------------------------------------------------------------------
# # Dependencies via FetchContent
# # ------------------------------------------------------------------------------

# # CPR Configuration: Force using bundled curl and configure it statically
# set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "Use bundled curl" FORCE)
# set(BUILD_SHARED_LIBS OFF CACHE BOOL "Force static libraries" FORCE)

# # 1. LibCPR (C++ Requests)
# FetchContent_Declare(
#     cpr
#     GIT_REPOSITORY https://github.com/libcpr/cpr.git
# )

# # 2. Nlohmann JSON
# FetchContent_Declare(
#     json
#     GIT_REPOSITORY https://github.com/nlohmann/json.git
# )

# # 3. Standalone Asio (GitHub)
# FetchContent_Declare(
#     asio
#     GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
# )

# # Make CPR and JSON available (they provide fully compatible native CMake targets)
# FetchContent_MakeAvailable(cpr json)

# # Standalone Asio doesn't expose a native CMake target by default in its root directory.
# # We populate it manually and construct an INTERFACE target matching your link target name.
# FetchContent_GetProperties(asio)
# if(NOT asio_POPULATED)
#     FetchContent_Populate(asio)
    
#     add_library(asio INTERFACE)
#     target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/include)
#     target_compile_definitions(asio INTERFACE ASIO_STANDALONE)
#     add_library(asio::asio ALIAS asio)
# endif()

# ------------------------------------------------------------------------------
# Targets
# ------------------------------------------------------------------------------

# Main Executable
add_executable(neuralnet
    src/main.cpp
    src/console.cpp
    src/socket.cpp
    src/trafficfetcher.cpp
)

# Compiler definitions & flags
if(MSVC)
    target_compile_options(neuralnet PRIVATE /W4 /O2)
else()
    target_compile_options(neuralnet PRIVATE -Wall -Wextra -O3)
endif()

target_include_directories(neuralnet PRIVATE
    include/
    src/
)

# Linking libraries and underlying system requirements
target_link_libraries(neuralnet PRIVATE
    cpr::cpr
    nlohmann_json::nlohmann_json
    asio::asio
    Threads::Threads
    websockets
)

# Windows specific sockets library requirements for network operations
if(WIN32)
    target_link_libraries(neuralnet PRIVATE ws2_32 mswsock)
endif()
