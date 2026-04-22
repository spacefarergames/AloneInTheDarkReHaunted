cmake_minimum_required(VERSION 3.9)
project(BuildHDAArchive)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Archive builder ───────────────────────────────────────────────────────────
add_executable(build_hda_archive build_hda_archive.cpp)

target_link_libraries(build_hda_archive PRIVATE zlibstatic)
target_include_directories(build_hda_archive PRIVATE
    ${CMAKE_SOURCE_DIR}/ThirdParty/zlib
    ${CMAKE_BINARY_DIR}/zlib/zlib-build   # for generated zconf.h
)

if(MSVC)
    target_compile_options(build_hda_archive PRIVATE /std:c++17)
endif()

if(WIN32)
    target_link_libraries(build_hda_archive PRIVATE shell32)
endif()

# ── Archive unpacker ──────────────────────────────────────────────────────────
add_executable(unpack_hda_archive unpack_hda_archive.cpp)

target_link_libraries(unpack_hda_archive PRIVATE zlibstatic)
target_include_directories(unpack_hda_archive PRIVATE
    ${CMAKE_SOURCE_DIR}/ThirdParty/zlib
    ${CMAKE_BINARY_DIR}/zlib/zlib-build   # for generated zconf.h
)

if(MSVC)
    target_compile_options(unpack_hda_archive PRIVATE /std:c++17)
endif()

if(WIN32)
    target_link_libraries(unpack_hda_archive PRIVATE shell32)
endif()
