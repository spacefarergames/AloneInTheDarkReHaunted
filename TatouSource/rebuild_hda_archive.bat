@echo off
REM Rebuild the backgrounds_hd.hda archive
REM Usage: rebuild_hda_archive.bat [configuration]
REM   Configurations: Debug, Release, MinSizeRel, RelWithDebInfo (default: Release)

setlocal enabledelayedexpansion

set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%\build\vs2026
set SOURCE_DIR=%SCRIPT_DIR%\Shipping\backgrounds_hd
set OUTPUT_DIR=%SCRIPT_DIR%\Shipping
set TOOL_EXE=%BUILD_DIR%\tools\%CONFIG%\build_hda_archive.exe
set OUTPUT_ARCHIVE=%OUTPUT_DIR%\backgrounds_hd.hda

echo.
echo ====================================================================
echo   HD Background Archive Rebuilder
echo ====================================================================
echo Configuration: %CONFIG%
echo Build directory: %BUILD_DIR%
echo Source directory: %SOURCE_DIR%
echo Output archive: %OUTPUT_ARCHIVE%
echo.

REM Check if source directory exists
if not exist "%SOURCE_DIR%" (
    echo Error: Source directory not found: %SOURCE_DIR%
    exit /b 1
)

REM Check if build tool exists
if not exist "%TOOL_EXE%" (
    echo Error: Build tool not found: %TOOL_EXE%
    echo.
    echo Please build the project first with CMake:
    echo   1. Open %BUILD_DIR%\FITD.sln in Visual Studio
    echo   2. Build the build_hda_archive target in %CONFIG% configuration
    echo   3. Run this script again
    exit /b 1
)

REM Backup existing archive if it exists
if exist "%OUTPUT_ARCHIVE%" (
    set BACKUP_FILE=!OUTPUT_ARCHIVE!.backup
    echo Backing up existing archive to: !BACKUP_FILE!
    move /Y "%OUTPUT_ARCHIVE%" "!BACKUP_FILE!" >nul
)

REM Run the builder
echo.
echo Running archive builder...
"%TOOL_EXE%" "%SOURCE_DIR%" "%OUTPUT_ARCHIVE%"

if errorlevel 1 (
    echo.
    echo Error: Failed to build archive!
    exit /b 1
)

echo.
echo ====================================================================
echo Archive rebuilt successfully!
echo Output: %OUTPUT_ARCHIVE%
echo ====================================================================
echo.

exit /b 0
