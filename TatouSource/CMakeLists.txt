cmake_minimum_required(VERSION 3.9)
project(FITD)

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBX_CONFIG_DEBUG=0")
#set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /DBX_CONFIG_DEBUG=1")

IF(MSVC)
    #set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /D_MSVC_STL_HARDENING=1")
    #set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_MSVC_STL_HARDENING=1")
ENDIF(MSVC)

# Nintendo Switch platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch" OR NINTENDO_SWITCH)
    set(NINTENDO_SWITCH ON)
    message(STATUS "Building for Nintendo Switch")
    message(STATUS "Nintendo SDK Root: ${NINTENDO_SDK_ROOT}")

    # Set Nintendo Switch specific flags
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    # Disable components that don't work on Switch
    set(BUILD_SDL OFF CACHE BOOL "Disable SDL for Nintendo Switch" FORCE)
    set(BUILD_SOLOUD OFF CACHE BOOL "Disable SoLoud for Nintendo Switch (use nn::audio)" FORCE)

    # Add Nintendo SDK libraries
    set(NX_LIBS
        nn_os
        nn_diag
        nn_fs
        nn_gfx
        nn_vi
        nn_hid
        nn_mem
        nn_init
        nn_oe
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    # UWP (Universal Windows Platform) detection
    set(UWP ON)
    message(STATUS "Building for UWP (Universal Windows Platform)")
    message(STATUS "Windows SDK Version: ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")

    # Set UWP specific flags
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    # SDL3 supports UWP, SoLoud should work too
    set(BUILD_SDL ON CACHE BOOL "Build SDL for UWP" FORCE)
    set(BUILD_SOLOUD ON CACHE BOOL "Build SoLoud for UWP" FORCE)

    # UWP-specific definitions
    add_definitions(-DUWP=1 -DWINAPI_FAMILY=WINAPI_FAMILY_APP)

    # Required UWP libraries
    set(UWP_LIBS
        WindowsApp
    )
else()
    set(NINTENDO_SWITCH OFF)
    set(UWP OFF)
    set(BUILD_SDL ON CACHE BOOL "Build SDL" FORCE)
    set(BUILD_SOLOUD ON CACHE BOOL "Build SoLoud" FORCE)
endif()

# Linux-specific: find required system packages
if(UNIX AND NOT APPLE AND NOT NINTENDO_SWITCH AND NOT UWP)
    find_package(PkgConfig REQUIRED)
    find_package(Threads REQUIRED)
    find_package(X11 REQUIRED)
    pkg_check_modules(GL REQUIRED gl)

    # Additional Linux libraries needed by bgfx/SDL
    set(EXTRA_LIBS
        ${CMAKE_THREAD_LIBS_INIT}
        ${X11_LIBRARIES}
        ${GL_LIBRARIES}
        dl
        rt
    )
endif()

set(THIRD_PARTY "${CMAKE_SOURCE_DIR}/ThirdParty")

set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory( ${THIRD_PARTY}/bgfx.cmake )

# Only build SDL and SoLoud if not on Nintendo Switch
if(NOT NINTENDO_SWITCH)
    add_subdirectory( ${THIRD_PARTY}/soloud.cmake )

    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    add_subdirectory( ${THIRD_PARTY}/SDL )
endif()

set(SKIP_INSTALL_ALL on)
set(ZLIB_LIBRARY zlibstatic)
# Don't build zlib tests
set(ZLIB_TESTS OFF CACHE BOOL "Build zlib tests")
add_subdirectory(${THIRD_PARTY}/zlib ${CMAKE_BINARY_DIR}/zlib/zlib-build)

add_subdirectory( tools )
add_subdirectory( FitdLib )
add_subdirectory( Fitd )
if(WIN32)
    add_subdirectory( DOSBoxStub )
endif()

#set(USE_SANITIZER ON)
if(USE_SANITIZER)
    IF(MSVC)
        set(SANITIZER_TYPE "address")
    ELSE()
        set(SANITIZER_TYPE "address,undefined,leak")
    ENDIF()
	
	target_link_options(Fitd PRIVATE "-fsanitize=${SANITIZER_TYPE}")
	target_compile_options(Fitd PRIVATE "-fsanitize=${SANITIZER_TYPE}")
	target_compile_options(FitdLib PRIVATE "-fsanitize=${SANITIZER_TYPE}")
	target_compile_options(bimg PRIVATE "-fsanitize=${SANITIZER_TYPE}")
	target_compile_options(bgfx PRIVATE "-fsanitize=${SANITIZER_TYPE}")
	target_compile_options(soloud PRIVATE "-fsanitize=${SANITIZER_TYPE}")
endif()
