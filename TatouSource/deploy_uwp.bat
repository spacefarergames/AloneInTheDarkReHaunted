@echo off
setlocal

echo === FITD UWP Deployment ===
echo.

set BUILD_DIR=build\vs2026\Fitd\Debug
set PACKAGE_DIR=build\uwp-package
set DATA_DIR=D:\SteamLibrary\steamapps\common\Alone in the Dark 2\INDARK2

if not exist "%BUILD_DIR%\Tatou.exe" (
    echo ERROR: Tatou.exe not found!
    echo Build the solution in Visual Studio first.
    pause
    exit /b 1
)

echo Found build: %BUILD_DIR%\Tatou.exe
echo.

echo Creating package directory...
if exist "%PACKAGE_DIR%" rd /s /q "%PACKAGE_DIR%"
mkdir "%PACKAGE_DIR%"
mkdir "%PACKAGE_DIR%\Assets"
mkdir "%PACKAGE_DIR%\data"

echo Copying executables and DLLs...
copy /Y "%BUILD_DIR%\*.exe" "%PACKAGE_DIR%\" >nul
copy /Y "%BUILD_DIR%\*.dll" "%PACKAGE_DIR%\" >nul

echo Copying Package.appxmanifest...
copy /Y "Package.appxmanifest" "%PACKAGE_DIR%\" >nul

echo Copying Assets...
xcopy /E /Y /Q "Assets\*" "%PACKAGE_DIR%\Assets\" >nul

if exist "%DATA_DIR%" (
    echo Copying game data...
    xcopy /E /Y /Q "%DATA_DIR%\*" "%PACKAGE_DIR%\data\" >nul
)

echo.
echo Package ready in: %PACKAGE_DIR%
echo.

echo Installing UWP package...
echo NOTE: Developer Mode must be enabled!
echo.

powershell -Command "Add-AppxPackage -Register '%CD%\%PACKAGE_DIR%\Package.appxmanifest' -ForceApplicationShutdown"

if %errorlevel% equ 0 (
    echo.
    echo === Installation Complete! ===
    echo.
    echo Launch from Start Menu: "FITD - Tatou"
) else (
    echo.
    echo ERROR: Installation failed!
    echo.
    echo Enable Developer Mode:
    echo   Settings -^> System -^> For Developers -^> Developer Mode
)

echo.
pause
