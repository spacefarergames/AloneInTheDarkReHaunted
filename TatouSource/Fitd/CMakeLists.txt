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
    set_source_files_properties("bgfxPatch.mm" PROPERTIES
        LANGUAGE CXX
        COMPILE_FLAGS "-x objective-c++"
    )
endif()

source_group("src" FILES ${SRC_ROOT})

if(MSVC)
    set( CMAKE_EXE_LINKER_FLAGS_SHIPPING "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /LTCG /OPT:ICF /OPT:REF /SUBSYSTEM:WINDOWS /DEBUG" )
endif()

add_executable (Fitd ${SOURCES})

# Output executable as Tatou / Tatou.app on macOS
set_target_properties(Fitd PROPERTIES OUTPUT_NAME "Tatou")

IF(${CMAKE_SYSTEM_NAME} MATCHES Darwin)
    # Build as a proper .app bundle
    set_target_properties(Fitd PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    )

    # Collect game assets to embed in Resources
    set(MACOS_ASSETS
        "${CMAKE_BINARY_DIR}/Fitd/aitd_remaster.cfg"
        "${CMAKE_BINARY_DIR}/Fitd/atlases.hda"
        "${CMAKE_BINARY_DIR}/Fitd/audio.hda"
        "${CMAKE_BINARY_DIR}/Fitd/audio_fre.hda"
        "${CMAKE_BINARY_DIR}/Fitd/backgrounds_hd.hda"
        "${CMAKE_BINARY_DIR}/Fitd/BLKCHCRY.TTF"
        "${CMAKE_BINARY_DIR}/Fitd/version.txt"
    )

    # Mark asset files so CMake places them in Contents/Resources
    foreach(_asset ${MACOS_ASSETS})
        if(EXISTS "${_asset}")
            target_sources(Fitd PRIVATE "${_asset}")
            set_source_files_properties("${_asset}" PROPERTIES
                MACOSX_PACKAGE_LOCATION "Resources"
            )
        endif()
    endforeach()

    # Copy any assets that might not exist yet at configure time via a post-build step
    add_custom_command(TARGET Fitd POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_CONTENT_DIR:Fitd>/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/Fitd/aitd_remaster.cfg"
            "${CMAKE_BINARY_DIR}/Fitd/atlases.hda"
            "${CMAKE_BINARY_DIR}/Fitd/audio.hda"
            "${CMAKE_BINARY_DIR}/Fitd/audio_fre.hda"
            "${CMAKE_BINARY_DIR}/Fitd/backgrounds_hd.hda"
            "${CMAKE_BINARY_DIR}/Fitd/BLKCHCRY.TTF"
            "${CMAKE_BINARY_DIR}/Fitd/version.txt"
            "$<TARGET_BUNDLE_CONTENT_DIR:Fitd>/Resources/"
        COMMENT "Copying game assets into Tatou.app/Contents/Resources"
    )
endif()


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
    if(APPLE)
        target_link_libraries(Fitd "-framework CoreFoundation")
    endif()
endif()

set_target_properties(Fitd PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "D:/SteamLibrary/steamapps/common/Alone in the Dark 2/INDARK2")

# Hide console window in Release builds, show in Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set_target_properties(Fitd PROPERTIES WIN32_EXECUTABLE TRUE)
else()
    set_target_properties(Fitd PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

set_property( DIRECTORY PROPERTY VS_STARTUP_PROJECT Fitd)


