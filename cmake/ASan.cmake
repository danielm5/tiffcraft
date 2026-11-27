

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(TIFFCRAFT_USE_ASAN_DEFAULT ON)
else()
  set(TIFFCRAFT_USE_ASAN_DEFAULT OFF)
endif()

option(TIFFCRAFT_USE_ASAN
  "Use Address Sanitizer (ASan)" ${TIFFCRAFT_USE_ASAN_DEFAULT})

if(TIFFCRAFT_USE_ASAN)
  message(STATUS "Address Sanitizer (ASan) enabled")
  if(MSVC)
    get_filename_component(ASAN_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    file(COPY
      "${ASAN_RUNTIME_DIR}/clang_rt.asan_dynamic-x86_64.dll"
      DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    )
    add_compile_options(/fsanitize=address)
    add_link_options(/INCREMENTAL:NO)
  else()
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
  endif()
endif()
