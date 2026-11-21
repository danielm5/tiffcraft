
add_executable(tiff_exporter tiff_exporter.cpp)
target_link_libraries(tiff_exporter PRIVATE TiffCraft)

add_executable(tiff_info tiff_info.cpp)
target_link_libraries(tiff_info PRIVATE TiffCraft)

if (APPLE)
  target_compile_options(tiff_exporter PRIVATE -Wno-deprecated-declarations)
endif()
