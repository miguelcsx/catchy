cmake_minimum_required(VERSION 3.20)
project(catchy 
        VERSION 0.1.0
        DESCRIPTION "Cognitive Complexity Analysis Tool"
        LANGUAGES CXX C)

# Use modern C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# Build options
option(CATCHY_BUILD_TESTS "Build the tests" OFF)
option(CATCHY_BUILD_DOCS "Build the documentation" OFF)
option(CATCHY_ENABLE_WARNINGS "Enable compiler warnings" ON)
option(CATCHY_ENABLE_WERROR "Treat warnings as errors" OFF)

# Find required packages - Using Mamba paths
set(CMAKE_PREFIX_PATH $ENV{CONDA_PREFIX} ${CMAKE_PREFIX_PATH})
find_package(LLVM REQUIRED CONFIG)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

# Build tree-sitter and parsers
add_subdirectory(tree_sitter)

# Compiler warnings
if(CATCHY_ENABLE_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(
            # -Wall
            # -Wextra
            # -Wpedantic
            # -Wconversion
            # -Wsign-conversion
            # -Wcast-qual
            # -Wformat=2
            # -Wundef
            # -Werror=return-type
            # -Wuninitialized
            -O3
        )
        if(CATCHY_ENABLE_WERROR)
            add_compile_options(-Werror)
        endif()
    endif()
endif()

# Add subdirectory containing the main library code
add_subdirectory(catchy)
