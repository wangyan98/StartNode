cmake_minimum_required(VERSION 3.30)
project(StartNode LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Select a C++26 reflection-capable compiler. GCC 16 (-freflection) is the
# supported path on this host; Clang P2996 lacks <meta> in Homebrew builds.
# Allow the caller to pin STARTNODE_CXX_COMPILER, otherwise fall back to gcc-16.
if(NOT DEFINED STARTNODE_CXX_COMPILER)
    find_program(GCC16_CXX g++-16 g++-16
        PATHS /opt/homebrew/bin /usr/local/bin /usr/bin ENV PATH)
    if(GCC16_CXX)
        set(STARTNODE_CXX_COMPILER "${GCC16_CXX}" CACHE FILEPATH
            "C++ compiler for StartNode (requires C++26 reflection)")
    endif()
endif()

if(STARTNODE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER "${STARTNODE_CXX_COMPILER}" CACHE FILEPATH "" FORCE)
endif()

# Reflection requires either GCC (-freflection) or Clang (-freflection + P2996).
if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
    message(FATAL_ERROR
        "StartNode requires GCC 16+ or Clang with C++26 reflection support.\n"
        "Found: ${CMAKE_CXX_COMPILER_ID}\n"
        "Install via: brew install gcc\n"
        "Then pass: -DSTARTNODE_CXX_COMPILER=/opt/homebrew/bin/g++-16")
endif()

# The reflection flag is meaningful on both supported compilers. The
# Clang-specific -fexpansion-statements / -fannotation-attributes /
# -fparameter-reflection toggles are NOT passed: GCC implements the standard
# expansion-statement (`template for`) and annotation attribute (`[[= ...]]`)
# syntax directly under -freflection.

option(STARTNODE_BUILD_TESTS "Build StartNode tests" OFF)

add_subdirectory(src/meta_core)
add_subdirectory(src/reflect)

if(STARTNODE_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    add_subdirectory(tests/reflect_tests)
endif()
