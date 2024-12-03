# Get all source files recursively from each subdirectory
file(GLOB_RECURSE ANALYSIS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/analysis/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/analysis/*.hpp"
)

file(GLOB_RECURSE PARSER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/*.hpp"
)

file(GLOB_RECURSE COMPLEXITY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/complexity/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/complexity/*.hpp"
)

file(GLOB_RECURSE UTILS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.hpp"
)

# Create the library target
add_library(catchy_core STATIC
    ${ANALYSIS_SOURCES}
    ${PARSER_SOURCES}
    ${COMPLEXITY_SOURCES}
    ${UTILS_SOURCES}
)

# Set include directories for the library
target_include_directories(catchy_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LLVM_INCLUDE_DIRS}
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/build/tree_sitter/tree-sitter/lib/include
        ${CMAKE_SOURCE_DIR}/build/tree_sitter/tree-sitter-cpp/src
)

# Link dependencies
target_link_libraries(catchy_core
    PUBLIC
        fmt::fmt
        spdlog::spdlog
        tree-sitter
        tree-sitter-CPP
        LLVM
)

# Create the executable target
add_executable(catchy main.cpp)
target_link_libraries(catchy
    PRIVATE
        catchy_core
)

# Set include directories for the executable
target_include_directories(catchy
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
