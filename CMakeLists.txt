cmake_minimum_required(VERSION 4.0.0)
project(TiffCraft VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Interface library for public headers
add_library(TiffCraft INTERFACE)
target_include_directories(TiffCraft INTERFACE include)

# Enable testing
enable_testing()
add_subdirectory(tests)
