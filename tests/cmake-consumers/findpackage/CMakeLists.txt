cmake_minimum_required(VERSION 3.22.0)
project(findpackage_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(TiffCraft REQUIRED)

add_executable(findpackage_consumer main.cpp)
target_link_libraries(findpackage_consumer PRIVATE TiffCraft::TiffCraft)
