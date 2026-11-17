cmake_minimum_required(VERSION 3.22.0)
project(TiffCraft VERSION 0.2.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG}
      "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG}
      "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}
      "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set(LINUX TRUE)
endif()

if(MSVC)
  add_compile_options(/permissive-)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Interface library for public headers
add_library(TiffCraft INTERFACE)
add_library(TiffCraft::TiffCraft ALIAS TiffCraft)

target_include_directories(TiffCraft INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Install all files in include/ directory and subdirectories
install(DIRECTORY include/ DESTINATION include)

# Only build the test suite, apps, and examples when consumed standalone;
# when used via FetchContent or add_subdirectory, build the library only.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(TIFFCRAFT_BUILD_ALL_DEFAULT ON)
else()
  set(TIFFCRAFT_BUILD_ALL_DEFAULT OFF)
endif()
option(TIFFCRAFT_BUILD_ALL
  "Build the TiffCraft tests, apps, and examples" ${TIFFCRAFT_BUILD_ALL_DEFAULT})

if(TIFFCRAFT_BUILD_ALL)
  # Enable testing
  enable_testing()
  add_subdirectory(tests)

  # Build apps and code examples
  add_subdirectory(apps)
  add_subdirectory(examples)
endif()

# Install/export for find_package consumption
install(TARGETS TiffCraft EXPORT TiffCraftTargets)
install(EXPORT TiffCraftTargets
  FILE TiffCraftTargets.cmake
  NAMESPACE TiffCraft::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TiffCraft
)

configure_package_config_file(
  cmake/TiffCraftConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/TiffCraftConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TiffCraft
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/TiffCraftConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/TiffCraftConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/TiffCraftConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TiffCraft
)
