cmake_minimum_required(VERSION 3.9)

include_directories(
    "."
    "${CMAKE_SOURCE_DIR}/FitdLib"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB FITD_DATA
    "${CMAKE_SOURCE_DIR}/data/*"
)

file(GLOB SRC_ROOT
    "*.cpp"
    "*.h"
    "*.rc"
)

set(SOURCES
 ${SOURCES}
 ${SRC_ROOT}
)

IF(${CMAKE_SYSTEM_NAME} MATCHES Darwin)
    set(SOURCES
     ${SOURCES}
     "bgfxPatch.mm"
    )
endif()

source_group("src" FILES ${SRC_ROOT})

if(MSVC)
    set( CMAKE_EXE_LINKER_FLAGS_SHIPPING "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /LTCG /OPT:ICF /OPT:REF /SUBSYSTEM:WINDOWS /DEBUG" )
endif()

add_executable (Fitd ${SOURCES})

# Output executable as Tatou.exe (Alone In The Dark 2 - Jack Is Back Again)
set_target_properties(Fitd PROPERTIES OUTPUT_NAME "Tatou")

# Platform-specific linking
if(NINTENDO_SWITCH)
    # Nintendo Switch libraries (no SDL or SoLoud)
    TARGET_LINK_LIBRARIES(Fitd FitdLib bgfx zlibstatic ${NX_LIBS})
elseif(UWP)
    # UWP libraries (with SDL and SoLoud, plus UWP runtime)
    TARGET_LINK_LIBRARIES(Fitd FitdLib bgfx zlibstatic SDL3-static soloud ${UWP_LIBS})
    # Set UWP app type
    set_target_properties(Fitd PROPERTIES VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "10.0.17763.0")
else()
    # Standard platforms with SDL and SoLoud
    TARGET_LINK_LIBRARIES(Fitd FitdLib bgfx zlibstatic SDL3-static soloud ${EXTRA_LIBS})
endif()

set_target_properties(Fitd PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "D:/SteamLibrary/steamapps/common/Alone in the Dark 2/INDARK2")

# Hide console window in Release builds, show in Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set_target_properties(Fitd PROPERTIES WIN32_EXECUTABLE TRUE)
else()
    set_target_properties(Fitd PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

set_property( DIRECTORY PROPERTY VS_STARTUP_PROJECT Fitd)


