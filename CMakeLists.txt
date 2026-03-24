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

set( CMAKE_EXE_LINKER_FLAGS_SHIPPING "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /LTCG /OPT:ICF /OPT:REF /SUBSYSTEM:WINDOWS /DEBUG" )

add_executable (Fitd ${SOURCES})

# Output executable as Tatou.exe (Alone In The Dark Re-Haunted)
set_target_properties(Fitd PROPERTIES OUTPUT_NAME "Tatou")

TARGET_LINK_LIBRARIES(Fitd FitdLib bgfx zlibstatic SDL3-static soloud ${EXTRA_LIBS})

set_target_properties(Fitd PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/data/AITD1_CD")

# Hide console window in Release builds, show in Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set_target_properties(Fitd PROPERTIES WIN32_EXECUTABLE TRUE)
else()
    set_target_properties(Fitd PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

set_property( DIRECTORY PROPERTY VS_STARTUP_PROJECT Fitd)


