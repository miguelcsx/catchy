cmake_minimum_required(VERSION 3.20)
project(tree-sitter-langs
        VERSION 0.1.0
        DESCRIPTION "Tree-sitter language parsers"
        LANGUAGES C CXX)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find ICU package
find_package(ICU COMPONENTS uc i18n REQUIRED)

function(CHECKOUT proj tag)
  if (NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${proj}")
    message(STATUS "Cloning ${proj} ${tag}")
    execute_process(
      COMMAND git clone -b ${tag} https://github.com/tree-sitter/${proj}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      RESULT_VARIABLE ret
      OUTPUT_QUIET
      ERROR_QUIET
    )
    if (NOT ret EQUAL 0)
      message(FATAL_ERROR "Error cloning ${proj}")
    endif()
  endif()
endfunction()

function(add_language lang)
  string(TOLOWER "${lang}" lang_str)
  if (NOT ARGN)
    set(src_dir "tree-sitter-${lang_str}")
  else()
    set(src_dir "${ARGN}")
  endif()

  # Set source files
  set(PARSER_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${src_dir}/src/parser.c")
  set(SCANNER_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${src_dir}/src/scanner.cc")
  
  if(NOT EXISTS "${SCANNER_SOURCE}")
    set(SCANNER_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${src_dir}/src/scanner.c")
  endif()

  # Create parser library target
  add_library(tree-sitter-${lang} STATIC)

  # Add parser source
  target_sources(tree-sitter-${lang} 
    PRIVATE 
      "${PARSER_SOURCE}"
  )

  # If scanner exists, add it
  if(EXISTS "${SCANNER_SOURCE}")
    target_sources(tree-sitter-${lang} 
      PRIVATE 
        "${SCANNER_SOURCE}"
    )
  endif()

  # Set include directories
  target_include_directories(tree-sitter-${lang}
    PRIVATE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/${src_dir}/src>
    PUBLIC
      ${CMAKE_CURRENT_BINARY_DIR}/tree-sitter/lib/include
      ${ICU_INCLUDE_DIRS}
  )

  # Link dependencies
  target_link_libraries(tree-sitter-${lang}
    PUBLIC
      tree-sitter
      ICU::uc
      ICU::i18n
  )

  # Set compile flags for C++
  if(EXISTS "${SCANNER_SOURCE}")
    if(${SCANNER_SOURCE} MATCHES "\\.cc$")
      set_source_files_properties(${SCANNER_SOURCE} PROPERTIES LANGUAGE CXX)
      target_compile_features(tree-sitter-${lang} PRIVATE cxx_std_11)
    endif()
  endif()
endfunction()

# Clone repositories
checkout(tree-sitter v0.20.1)
checkout(tree-sitter-cpp v0.20.0)
# checkout(tree-sitter-python rust-0.19.1)

# Build tree-sitter library
add_library(tree-sitter STATIC 
    "${CMAKE_CURRENT_BINARY_DIR}/tree-sitter/lib/src/lib.c")

target_include_directories(tree-sitter
  PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/tree-sitter/lib/include
    ${ICU_INCLUDE_DIRS}
)

target_link_libraries(tree-sitter
  PUBLIC
    ICU::uc
    ICU::i18n
)

# Build language parsers
add_language(CPP)
