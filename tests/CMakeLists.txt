
Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.9.0
)

FetchContent_MakeAvailable(Catch2)

add_executable(tiffSimpleTest tiffSimpleTest.cpp)
target_link_libraries(tiffSimpleTest PRIVATE TiffCraft Catch2::Catch2WithMain)

add_executable(tiffExporterTest tiffExporterTest.cpp)
target_link_libraries(tiffExporterTest PRIVATE TiffCraft Catch2::Catch2WithMain)
