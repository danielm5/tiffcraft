
# Test fixtures live in the tests/libtiff-pics submodule and are stored in
# Git LFS. A checkout without git-lfs (or with the smudge filter skipped)
# keeps small LFS pointer files instead of real TIFFs, which makes the tests
# fail at runtime with "Invalid byte order in TIFF header". Detect the
# problem at configure time and fail fast with actionable instructions.
#
# When TiffCraft is the top-level project this is a hard error; when vendored
# as a subproject (FetchContent / add_subdirectory) it is only a warning, so
# consumers who do not want the tests or cannot fetch the LFS objects are not
# blocked.
get_filename_component(TIFFCRAFT_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)
set(TIFFCRAFT_LFS_REMEDY "Install git-lfs (Debian: apt install git-lfs), then run:\n  git lfs install\n  git submodule update --init --recursive\n  git -C tests/libtiff-pics lfs pull")
if (CMAKE_SOURCE_DIR STREQUAL TIFFCRAFT_REPO_ROOT)
  set(FIXTURE_FAILURE_LEVEL FATAL_ERROR)
else()
  set(FIXTURE_FAILURE_LEVEL WARNING)
endif()

# All LFS-tracked binary fixtures. .g3/.png are not TIFF files, so they are
# only checked for the LFS pointer signature below.
file(GLOB_RECURSE PICS_FIXTURES LIST_DIRECTORIES false
     "${CMAKE_CURRENT_SOURCE_DIR}/libtiff-pics/*.tif"
     "${CMAKE_CURRENT_SOURCE_DIR}/libtiff-pics/*.tiff"
     "${CMAKE_CURRENT_SOURCE_DIR}/libtiff-pics/*.tif2"
     "${CMAKE_CURRENT_SOURCE_DIR}/libtiff-pics/*.g3"
     "${CMAKE_CURRENT_SOURCE_DIR}/libtiff-pics/*.png")

if (NOT PICS_FIXTURES)
  message(${FIXTURE_FAILURE_LEVEL}
    "TiffCraft: test fixtures missing under tests/libtiff-pics.\n"
    "The fixtures live in the tests/libtiff-pics submodule and are stored in Git LFS.\n"
    "${TIFFCRAFT_LFS_REMEDY}")
else()
  set(BAD_FIXTURES "")
  foreach(F IN LISTS PICS_FIXTURES)
    # Type-agnostic check: unresolved Git LFS pointers start with the ASCII
    # prefix "version https://git-lfs".
    file(READ "${F}" HEAD LIMIT 64 HEX)
    if (HEAD MATCHES "76657273696f6e2068747470733a2f2f6769742d6c6673")
      list(APPEND BAD_FIXTURES "${F} (unresolved Git LFS pointer)")
      continue()
    endif()
    get_filename_component(EXT "${F}" EXT)
    string(TOLOWER "${EXT}" EXT)
    if (EXT STREQUAL ".tif" OR EXT STREQUAL ".tiff" OR EXT STREQUAL ".tif2")
      file(READ "${F}" FIRST_BYTES LIMIT 4 HEX)
      # Classic TIFF: II*\0 (little-endian) / MM\0* (big-endian).
      # BigTIFF:    II+\0 / MM\0+ (0x2B instead of 0x2A).
      if (NOT (FIRST_BYTES STREQUAL "49492a00" OR FIRST_BYTES STREQUAL "4d4d002a"
            OR FIRST_BYTES STREQUAL "49492b00" OR FIRST_BYTES STREQUAL "4d4d002b"))
        list(APPEND BAD_FIXTURES "${F} (not a TIFF file)")
      endif()
    endif()
  endforeach()

  if (BAD_FIXTURES)
    string(REPLACE ";" "\n" BAD_FIXTURES_LIST "${BAD_FIXTURES}")
    message(${FIXTURE_FAILURE_LEVEL}
      "TiffCraft: test fixtures under tests/libtiff-pics are unresolved Git LFS pointers or not TIFF files:\n"
      "${BAD_FIXTURES_LIST}\n"
      "${TIFFCRAFT_LFS_REMEDY}")
  endif()
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
