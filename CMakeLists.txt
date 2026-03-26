cmake_minimum_required(VERSION 3.16)
project(NeuroRPG LANGUAGES CXX)

# --------------------------------------------------
# Global settings
# --------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/bin)

set(CMAKE_DEBUG_POSTFIX "_dbg")

# Set default build type (Release if not specified)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# --------------------------------------------------
# llama.cpp configuration with CUDA
# --------------------------------------------------

# Build llama.cpp as a shared library (DLL on Windows)
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "Build llama.cpp Tests" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "Build llama.cpp Examples" FORCE)
set(LLAMA_CURL OFF CACHE BOOL "llama.cpp CURL" FORCE)
set(LLAMA_SSL OFF CACHE BOOL "llama.cpp SSL" FORCE)
set(GGML_CUDA ON CACHE BOOL "Enable CUDA" FORCE)

# For GTX 1060 Ti compatibility
set(CMAKE_CUDA_ARCHITECTURES "61" CACHE STRING "CUDA architectures" FORCE)
set(CMAKE_CUDA_ARCHITECTURES "61" CACHE STRING "CUDA architectures" FORCE)
set(GGML_CUDA_ARCHS "61" CACHE STRING "CUDA architectures for llama.cpp" FORCE)
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -arch=sm_61")
set(CMAKE_CUDA_ARCHITECTURES OFF CACHE BOOL "" FORCE)

find_package(CUDAToolkit REQUIRED)

add_subdirectory(lib/llama.cpp)

# --------------------------------------------------
# Project sources
# --------------------------------------------------
file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS src/client/*.cpp src/client/*.h)
file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS src/server/*.cpp src/server/*.h)
file(GLOB_RECURSE SHARED_SOURCES CONFIGURE_DEPENDS src/shared/*.cpp src/shared/*.h)

# ------------------------------------------------------------
#  Shared code (static library)
# ------------------------------------------------------------
add_library(shared STATIC)
target_sources(shared PRIVATE ${SHARED_SOURCES})
target_include_directories(shared PUBLIC src/shared)

# ------------------------------------------------------------
#  Server executable
# ------------------------------------------------------------
add_executable(server)
target_sources(server PRIVATE ${SERVER_SOURCES})
target_link_libraries(server PRIVATE shared llama)

# ------------------------------------------------------------
#  Client executable
# ------------------------------------------------------------
add_executable(client)
target_sources(client PRIVATE ${CLIENT_SOURCES})
target_link_libraries(client PRIVATE shared llama)

set_target_properties(client server shared PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# Set working directory to project root for debugging
set_target_properties(client server PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

if (MSVC)
    target_compile_options(client PRIVATE /W4)
    target_compile_options(server PRIVATE /W4)
else()
    target_compile_options(client PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Optional: print a message after successful build
message(STATUS "Project configured. Build with: cmake --build build --config ${CMAKE_BUILD_TYPE}")
