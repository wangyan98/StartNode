cmake_minimum_required(VERSION 3.30)
project(imrefl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# enable_testing() at the top level so `ctest` works from the build dir
# when StartNode tests are enabled.
enable_testing()

add_subdirectory(StartNode)

add_library(ImRefl INTERFACE)
target_include_directories(ImRefl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ImRefl INTERFACE MetaCore)

if (IMREFL_BUILD_EXAMPLE)
  add_subdirectory(example)
endif()
