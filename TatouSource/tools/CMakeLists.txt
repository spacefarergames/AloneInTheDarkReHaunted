cmake_minimum_required(VERSION 3.9)
project(BuildHDAArchive)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build the archive builder tool
add_executable(build_hda_archive build_hda_archive.cpp)

if(MSVC)
    # Enable filesystem support on MSVC
    target_compile_options(build_hda_archive PRIVATE /std:c++17)
endif()

# Windows-specific settings
if(WIN32)
    target_link_libraries(build_hda_archive PRIVATE shell32)
endif()
