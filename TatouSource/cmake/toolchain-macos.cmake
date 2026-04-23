###############################################################################
# CMake toolchain file for cross-compiling FITD to macOS from Linux/WSL
# using osxcross (https://github.com/tpoechtrager/osxcross).
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-macos.cmake \
#         -DOSXCROSS_ROOT=/path/to/osxcross ...
#
# Defaults:
#   OSXCROSS_ROOT  defaults to $OSXCROSS_ROOT env var or /opt/osxcross
#   MACOS_TARGET   macOS deployment target (default 12.0)
#   ARCH           target architecture: x86_64 or arm64 (default x86_64)
###############################################################################

set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR "${ARCH}")

# Resolve osxcross root
if(NOT DEFINED OSXCROSS_ROOT)
    if(DEFINED ENV{OSXCROSS_ROOT})
        set(OSXCROSS_ROOT "$ENV{OSXCROSS_ROOT}")
    else()
        set(OSXCROSS_ROOT "/opt/osxcross")
    endif()
endif()

if(NOT DEFINED MACOS_TARGET)
    set(MACOS_TARGET "13.0")
endif()

if(NOT DEFINED ARCH)
    set(ARCH "x86_64")
endif()

# Locate the macOS SDK bundled with osxcross
file(GLOB _sdk_candidates "${OSXCROSS_ROOT}/SDK/MacOSX*.sdk")
if(_sdk_candidates)
    list(SORT _sdk_candidates ORDER DESCENDING)
    list(GET _sdk_candidates 0 CMAKE_OSX_SYSROOT)
else()
    message(WARNING "No macOS SDK found under ${OSXCROSS_ROOT}/SDK. "
                    "Copy MacOSX<version>.sdk there before building.")
endif()

# Derive the darwin target triple from available cross-compilers.
# osxcross names compilers <arch>-apple-darwin<N>-clang; pick the newest.
file(GLOB _cc_candidates "${OSXCROSS_ROOT}/bin/${ARCH}-apple-darwin*-clang")
list(SORT _cc_candidates ORDER DESCENDING)
if(_cc_candidates)
    list(GET _cc_candidates 0 _cc_path)
    get_filename_component(_cc_name "${_cc_path}" NAME)        # e.g. x86_64-apple-darwin20.4-clang
    string(REGEX REPLACE "-clang$" "" OSXCROSS_TARGET_TRIPLE "${_cc_name}")
else()
    # Fallback: darwin21 = macOS 12
    set(OSXCROSS_TARGET_TRIPLE "${ARCH}-apple-darwin21")
    message(WARNING "No cross-compiler found for ${ARCH} in ${OSXCROSS_ROOT}/bin. "
                    "Falling back to ${OSXCROSS_TARGET_TRIPLE}.")
endif()

set(CMAKE_C_COMPILER      "${OSXCROSS_ROOT}/bin/${OSXCROSS_TARGET_TRIPLE}-clang")
set(CMAKE_CXX_COMPILER    "${OSXCROSS_ROOT}/bin/${OSXCROSS_TARGET_TRIPLE}-clang++")
set(CMAKE_OBJC_COMPILER   "${OSXCROSS_ROOT}/bin/${OSXCROSS_TARGET_TRIPLE}-clang")
set(CMAKE_OBJCXX_COMPILER "${OSXCROSS_ROOT}/bin/${OSXCROSS_TARGET_TRIPLE}-clang++")

set(CMAKE_AR     "${OSXCROSS_ROOT}/bin/${OSXCROSS_TARGET_TRIPLE}-ar"     CACHE FILEPATH "")
set(CMAKE_RANLIB "${OSXCROSS_ROOT}/bin/${OSXCROSS_TARGET_TRIPLE}-ranlib" CACHE FILEPATH "")
set(CMAKE_NM     "${OSXCROSS_ROOT}/bin/${OSXCROSS_TARGET_TRIPLE}-nm"     CACHE FILEPATH "")

set(CMAKE_OSX_DEPLOYMENT_TARGET "${MACOS_TARGET}" CACHE STRING "macOS deployment target")
set(CMAKE_OSX_ARCHITECTURES    "${ARCH}"           CACHE STRING "macOS target architecture")

# Don't try to look up libraries on the build host
set(CMAKE_FIND_ROOT_PATH       "${CMAKE_OSX_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Flags
set(_osx_flags "-target ${OSXCROSS_TARGET_TRIPLE} -mmacosx-version-min=${MACOS_TARGET} --sysroot=${CMAKE_OSX_SYSROOT}")
set(CMAKE_C_FLAGS_INIT             "${_osx_flags}")
set(CMAKE_CXX_FLAGS_INIT           "${_osx_flags} -stdlib=libc++")
set(CMAKE_OBJC_FLAGS_INIT          "${_osx_flags}")
set(CMAKE_OBJCXX_FLAGS_INIT        "${_osx_flags} -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${_osx_flags}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_osx_flags}")

message(STATUS "[macOS toolchain] OSXCROSS_ROOT = ${OSXCROSS_ROOT}")
message(STATUS "[macOS toolchain] SDK           = ${CMAKE_OSX_SYSROOT}")
message(STATUS "[macOS toolchain] Target        = ${OSXCROSS_TARGET_TRIPLE}")
message(STATUS "[macOS toolchain] Arch          = ${ARCH}")
message(STATUS "[macOS toolchain] Min OS        = ${MACOS_TARGET}")
