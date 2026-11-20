
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

add_executable(twoTuLinkTest twoTuLinkTestA.cpp twoTuLinkTestB.cpp)
target_link_libraries(twoTuLinkTest PRIVATE TiffCraft)
add_test(NAME twoTuLinkTest COMMAND twoTuLinkTest)

add_executable(readmeUsageTest readmeUsageTest.cpp)
target_link_libraries(readmeUsageTest PRIVATE TiffCraft)
add_test(NAME readmeUsageTest COMMAND readmeUsageTest ${CMAKE_CURRENT_SOURCE_DIR}/libtiff-pics/pc260001.tif)

add_executable(tiffVersionTest tiffVersionTest.cpp)
target_link_libraries(tiffVersionTest PRIVATE TiffCraft)
target_compile_definitions(tiffVersionTest PRIVATE TICFCRAFT_EXPECTED_VERSION="${PROJECT_VERSION}")
add_test(NAME tiffVersionTest COMMAND tiffVersionTest)

add_executable(tiffStdOutTest tiffStdOutTest.cpp)
target_link_libraries(tiffStdOutTest PRIVATE TiffCraft)
add_test(NAME tiffStdOutTest COMMAND tiffStdOutTest)
