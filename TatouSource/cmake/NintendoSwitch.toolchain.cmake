# Nintendo Switch Toolchain for CMake
# NintendoSDK NX-NXFP2-a64 (ARM64)

set(CMAKE_SYSTEM_NAME NintendoSwitch)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Set Nintendo SDK root from environment or cache
if(NOT DEFINED NINTENDO_SDK_ROOT)
    # Try environment variable first
    if(DEFINED ENV{NINTENDO_SDK_ROOT})
        set(NINTENDO_SDK_ROOT "$ENV{NINTENDO_SDK_ROOT}")
    else()
        message(FATAL_ERROR "NINTENDO_SDK_ROOT must be set either as environment variable or CMake cache variable")
    endif()
endif()

set(NINTENDO_SDK_ROOT "${NINTENDO_SDK_ROOT}" CACHE PATH "Nintendo SDK Root Directory")
message(STATUS "Nintendo SDK Root: ${NINTENDO_SDK_ROOT}")

# Verify SDK exists
if(NOT EXISTS "${NINTENDO_SDK_ROOT}/Include")
    message(FATAL_ERROR "Nintendo SDK not found at: ${NINTENDO_SDK_ROOT}")
endif()

# Platform identifier
set(NX_PLATFORM "NX-NXFP2-a64")

# Compiler paths
set(NINTENDO_CLANG_ROOT "${NINTENDO_SDK_ROOT}/Compilers/NintendoClang")
set(CMAKE_C_COMPILER "${NINTENDO_CLANG_ROOT}/bin/clang.exe")
set(CMAKE_CXX_COMPILER "${NINTENDO_CLANG_ROOT}/bin/clang++.exe")
set(CMAKE_AR "${NINTENDO_CLANG_ROOT}/bin/llvm-ar.exe")
set(CMAKE_RANLIB "${NINTENDO_CLANG_ROOT}/bin/llvm-ranlib.exe")

# Target triple for Nintendo Switch
set(NX_TRIPLE "aarch64-nintendo-nx-elf")

# Compiler flags
set(NX_ARCH_FLAGS "-target ${NX_TRIPLE} -march=armv8-a+crc+crypto -mtune=cortex-a57 -fPIC")
set(NX_COMMON_FLAGS "${NX_ARCH_FLAGS} -D__SWITCH__ -DNX64 -ffunction-sections -fdata-sections")

# C/C++ flags
set(CMAKE_C_FLAGS_INIT "${NX_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${NX_COMMON_FLAGS} -std=c++17 -fno-rtti -fno-exceptions")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "-target ${NX_TRIPLE} -Wl,--gc-sections -Wl,--build-id=sha1")

# Build types
set(CMAKE_C_FLAGS_DEBUG_INIT "-g -O0 -DDEBUG -D_DEBUG")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-g -O0 -DDEBUG -D_DEBUG")
set(CMAKE_C_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")

# Include directories
include_directories(SYSTEM 
    "${NINTENDO_SDK_ROOT}/Include"
)

# Library directories
link_directories(
    "${NINTENDO_SDK_ROOT}/Libraries/${NX_PLATFORM}/Release"
)

# Set root path for find_* commands
set(CMAKE_FIND_ROOT_PATH "${NINTENDO_SDK_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Platform-specific settings
set(NINTENDO_SWITCH ON CACHE BOOL "Building for Nintendo Switch")
add_definitions(-DNINTENDO_SWITCH=1)

# Disable builds that don't work on Switch
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
