
# Test fixtures live in the tests/libtiff-pics submodule and are stored in
# Git LFS. A checkout without git-lfs (or with the smudge filter skipped)
# keeps small LFS pointer files instead of real TIFFs, which makes the tests
# fail at runtime with "Invalid byte order in TIFF header". Detect the
# problem at configure time and fail fast with actionable instructions.
file(GLOB_RECURSE PICS_TIFFS LIST_DIRECTORIES false
     "${CMAKE_CURRENT_SOURCE_DIR}/libtiff-pics/*.tif"
     "${CMAKE_CURRENT_SOURCE_DIR}/libtiff-pics/*.tiff"
     "${CMAKE_CURRENT_SOURCE_DIR}/libtiff-pics/*.tif2")

if (NOT PICS_TIFFS)
  message(FATAL_ERROR
    "TiffCraft: test fixtures missing under tests/libtiff-pics.\n"
    "Run: git submodule update --init --recursive")
endif()

set(BAD_FIXTURES "")
foreach(F IN LISTS PICS_TIFFS)
  file(READ "${F}" FIRST_BYTES LIMIT 4 HEX)
  if (NOT (FIRST_BYTES STREQUAL "49492a00" OR FIRST_BYTES STREQUAL "4d4d002a"))
    list(APPEND BAD_FIXTURES "${F}")
  endif()
endforeach()

if (BAD_FIXTURES)
  string(REPLACE ";" "\n" BAD_FIXTURES_LIST "${BAD_FIXTURES}")
  message(FATAL_ERROR
    "TiffCraft: test fixtures under tests/libtiff-pics are unresolved Git LFS pointers:\n"
    "${BAD_FIXTURES_LIST}\n"
    "Install git-lfs (Debian: apt install git-lfs), then run:\n"
    "  git lfs install\n"
    "  git submodule update --init --recursive\n"
    "  git -C tests/libtiff-pics lfs pull")
endif()

option(BUILD_MAKE_REFERENCE_IMG "Build the makeReferenceImg utility" OFF)

if (BUILD_MAKE_REFERENCE_IMG)

Include(FetchContent)
  # Configure and build libtiff
  FetchContent_Declare(
    TIFF
    GIT_REPOSITORY https://gitlab.com/libtiff/libtiff.git
    GIT_TAG        v4.7.1rc1
    OVERRIDE_FIND_PACKAGE
  )
  set(BUILD_SHARED_LIBS OFF)
  set(tiff-static ON CACHE INTERNAL "Build TIFF static libraries")
  set(tiff-tools OFF CACHE INTERNAL "Build TIFF tools")
  set(tiff-tests OFF CACHE INTERNAL "Build TIFF tests")
  set(tiff-contrib OFF CACHE INTERNAL "Build TIFF contrib")
  set(tiff-docs OFF CACHE INTERNAL "Build TIFF documentation")
  set(tiff-deprecated OFF CACHE INTERNAL "build TIFF deprecated features")
  set(tiff-opengl OFF CACHE INTERNAL "OpenGL support")
  FetchContent_MakeAvailable(TIFF)
  find_package(TIFF REQUIRED)

  # Build the makeReferenceImg utility
  add_executable(makeReferenceImg makeReferenceImg.cpp netpbm.hpp)
  target_link_libraries(makeReferenceImg PRIVATE TIFF::tiff)

endif(BUILD_MAKE_REFERENCE_IMG)

enable_testing()

add_executable(tiffImageTest tiffImageTest.cpp)
target_link_libraries(tiffImageTest PRIVATE TiffCraft)
add_test(NAME tiffImageTest COMMAND tiffImageTest)

add_executable(tiffExporterTest tiffExporterTest.cpp netpbm.hpp)
target_link_libraries(tiffExporterTest PRIVATE TiffCraft)
add_test(NAME tiffExporterTest COMMAND tiffExporterTest)
