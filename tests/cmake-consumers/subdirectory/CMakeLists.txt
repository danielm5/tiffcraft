cmake_minimum_required(VERSION 3.22.0)
project(subdirectory_consumer LANGUAGES CXX)

# Repo root is three directories above this consumer project directory
# (tests/cmake-consumers/subdirectory -> repository root)
add_subdirectory(${CMAKE_SOURCE_DIR}/../../.. ${CMAKE_BINARY_DIR}/tiffcraft)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(subdirectory_consumer main.cpp)
set_target_properties(subdirectory_consumer PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
target_link_libraries(subdirectory_consumer PRIVATE TiffCraft::TiffCraft)

# When consumed via add_subdirectory, the library's own tests/apps/examples
# must not be built. Fail the configure step if any leaked through.
foreach(TIFFCRAFT_FORBIDDEN_TARGET
    tiffImageTest
    tiffExporterTest
    twoTuLinkTest
    readmeUsageTest
    tiffVersionTest
    tiff_exporter
    tiff_image_example
    tiff_exporter_example)
  if(TARGET ${TIFFCRAFT_FORBIDDEN_TARGET})
    message(FATAL_ERROR
      "TiffCraft target '${TIFFCRAFT_FORBIDDEN_TARGET}' was created while consuming via add_subdirectory; the tests/apps/examples should only build in standalone mode.")
  endif()
endforeach()
