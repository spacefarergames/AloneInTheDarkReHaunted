cmake_minimum_required(VERSION 3.9)
project(pack_hda)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(pack_hda pack_hda.cpp)

if(WIN32)
    target_link_libraries(pack_hda PRIVATE shlwapi)
endif()
