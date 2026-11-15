cmake_minimum_required(VERSION 3.22.0)
project(fetchcontent_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Repo root is three directories above this consumer project dir
# (/home/daniel/tiffcraft/tests/cmake-consumers/fetchcontent -> /home/daniel/tiffcraft)
FetchContent_Declare(
  TiffCraft
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/../../..
)

FetchContent_MakeAvailable(TiffCraft)

add_executable(fetchcontent_consumer main.cpp)
target_link_libraries(fetchcontent_consumer PRIVATE TiffCraft::TiffCraft)
