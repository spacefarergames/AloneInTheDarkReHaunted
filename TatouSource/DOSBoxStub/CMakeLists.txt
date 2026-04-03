cmake_minimum_required(VERSION 3.9)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(DOSBoxStub WIN32 main.cpp DOSBox.rc)

# Output as DOSBox.exe so it can replace the original launcher.
set_target_properties(DOSBoxStub PROPERTIES OUTPUT_NAME "DOSBox")
