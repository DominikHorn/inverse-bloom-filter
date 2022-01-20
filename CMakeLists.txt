cmake_minimum_required(VERSION 3.20)
project("invertible-bloom-filter" VERSION 1.0
        DESCRIPTION "A header only cmake/c++ library that exposes an invertible bloom filter implementation"
        HOMEPAGE_URL "https://github.com/DominikHorn/invertible-bloom-filter"
        LANGUAGES CXX)

# Declare library & directories to include. See
# http://mariobadr.com/creating-a-header-only-library-with-cmake.html for more
# info/install instructions
add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

# Make IDE friendly
target_sources(${PROJECT_NAME} INTERFACE invertible_bloom_filter.hpp)

# Benchmark and test code
get_directory_property(hasParent PARENT_DIRECTORY)
if (NOT hasParent)
  add_subdirectory(src)
endif()
