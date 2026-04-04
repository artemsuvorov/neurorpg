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

# Enable folder support for IDE (Visual Studio)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

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
set(GGML_CUDA_ARCHS "61" CACHE STRING "CUDA architectures for llama.cpp" FORCE)
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -arch=sm_61")

find_package(CUDAToolkit REQUIRED)

add_subdirectory(lib/llama.cpp)

# --------------------------------------------------
# Multithreading & Net support
# --------------------------------------------------

# find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_library(asio INTERFACE)
target_sources(asio INTERFACE lib/asio/include/asio.hpp)
target_include_directories(asio INTERFACE lib/asio/include)

# --------------------------------------------------
# spdlog
# --------------------------------------------------
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "Build shared library" FORCE)
set(SPDLOG_BUILD_STATIC ON  CACHE BOOL "Build static library" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "Build examples" FORCE)
set(SPDLOG_BUILD_TESTS   OFF CACHE BOOL "Build tests" FORCE)

add_subdirectory(lib/spdlog)

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
target_link_libraries(shared PRIVATE spdlog::spdlog)
target_include_directories(shared PUBLIC src/shared)

# ------------------------------------------------------------
#  Server executable
# ------------------------------------------------------------
add_executable(server)
target_sources(server PRIVATE ${SERVER_SOURCES})
target_link_libraries(server PRIVATE
    shared
    llama
    Threads::Threads
    asio
    spdlog::spdlog
)
if (WIN32)
    target_link_libraries(server PRIVATE ws2_32)
endif()
target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/asio/include
)
target_compile_definitions(server PRIVATE
    ASIO_STANDALONE
)

# ------------------------------------------------------------
#  Client executable
# ------------------------------------------------------------
add_executable(client)
target_sources(client PRIVATE ${CLIENT_SOURCES})
target_link_libraries(client PRIVATE shared llama spdlog::spdlog)

# ------------------------------------------------------------
#  All targets settings
# ------------------------------------------------------------

# Enable C++23 Standard
set_target_properties(client server shared PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

# Enable asserts
target_compile_definitions(client PRIVATE NR_ENABLE_ASSERTS)
target_compile_definitions(server PRIVATE NR_ENABLE_ASSERTS)
target_compile_definitions(shared PRIVATE NR_ENABLE_ASSERTS)

# Enable logging only for Debug & Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(shared PUBLIC  NR_ENABLE_LOGGING NR_LOGGING_LEVEL=0)
    target_compile_definitions(server PRIVATE NR_ENABLE_LOGGING NR_LOGGING_LEVEL=0)
    target_compile_definitions(client PRIVATE NR_ENABLE_LOGGING NR_LOGGING_LEVEL=0)
endif()

# Set working directory to project root for debugging
set_target_properties(client server PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

if (MSVC)
    target_compile_options(client PRIVATE /W4)
    target_compile_options(server PRIVATE /W4)
else()
    target_compile_options(client PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# ------------------------------------------------------------
#  Tests (optional)
# ------------------------------------------------------------
option(NEURO_BUILD_TESTS "Build unit tests" ON)

if(NEURO_BUILD_TESTS)
    enable_testing()

    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
    add_executable(tests ${TEST_SOURCES})
    target_link_libraries(tests PRIVATE shared Threads::Threads asio spdlog::spdlog)
    target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/asio/include)
    target_compile_definitions(tests PRIVATE ASIO_STANDALONE)
    set_target_properties(tests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

    # Enable asserts for now, later use a proper testing lib
    target_compile_definitions(tests PRIVATE NR_ENABLE_ASSERTS)

    if (MSVC)
        target_compile_options(tests PRIVATE /W4)
    else()
        target_compile_options(tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME NeuroCoreTests COMMAND $<TARGET_FILE:tests>)
endif()

# ------------------------------------------------------------
#  Organize targets into folders in the IDE
# ------------------------------------------------------------
set(our_targets client server shared)
foreach(target ${our_targets})
    set_target_properties(${target} PROPERTIES FOLDER "Game")
endforeach()

if(NEURO_BUILD_TESTS)
    set_target_properties(tests PROPERTIES FOLDER "Tests")
endif()

set_target_properties(spdlog PROPERTIES FOLDER "Libs")
set_target_properties(asio PROPERTIES FOLDER "Libs")
set_target_properties(llama PROPERTIES FOLDER "Libs")
set_target_properties(ggml PROPERTIES FOLDER "Libs")
set_target_properties(ggml-base PROPERTIES FOLDER "Libs")
set_target_properties(ggml-cpu PROPERTIES FOLDER "Libs")
set_target_properties(ggml-cuda PROPERTIES FOLDER "Libs")

# Optional: print a message after successful build
message(STATUS "Project configured. Build with: cmake --build build --config ${CMAKE_BUILD_TYPE}")
