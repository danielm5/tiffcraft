
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
