@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

echo ==========================================
echo GameSWF Windows Build Script
echo ==========================================
echo.

:: Check for Visual Studio environment
where cl >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Visual Studio compiler not found!
    echo Please run this script from a Visual Studio Developer Command Prompt.
    echo.
    echo Or run one of the following:
    echo   - "x64 Native Tools Command Prompt for VS 2022"
    echo   - "x86 Native Tools Command Prompt for VS 2022"
    echo   - "Developer Command Prompt for VS 2022"
    echo.
    pause
    exit /b 1
)

echo [OK] Visual Studio compiler found

:: Check for CMake
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake not found! Please install CMake and add it to PATH.
    echo Download from: https://cmake.org/download/
    pause
    exit /b 1
)

echo [OK] CMake found

:: Set build configuration
set BUILD_TYPE=Release
set BUILD_DIR=build_windows

:: Parse command line arguments
if "%~1"=="debug" (
    set BUILD_TYPE=Debug
    echo [INFO] Building DEBUG configuration
) else (
    echo [INFO] Building RELEASE configuration (use 'build.bat debug' for debug build)
)

:: Check for vcpkg
if defined VCPKG_ROOT (
    echo [OK] vcpkg found at: %VCPKG_ROOT%
    set VCPKG_TOOLCHAIN=-DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
) else (
    echo [WARNING] VCPKG_ROOT not set. Will try to find dependencies manually.
    echo.
    echo To use vcpkg for dependencies, install it and set VCPKG_ROOT:
    echo   git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
    echo   C:\vcpkg\bootstrap-vcpkg.bat
    echo   setx VCPKG_ROOT "C:\vcpkg"
    echo   C:\vcpkg\vcpkg install sdl2 zlib libpng libjpeg-turbo
    echo.
)

echo.
echo ==========================================
echo Starting Build
echo ==========================================
echo.

:: Create build directory
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
cd %BUILD_DIR%

:: Configure with CMake
echo [1/3] Configuring with CMake...
cmake .. -G "NMake Makefiles" %VCPKG_TOOLCHAIN% -DCMAKE_BUILD_TYPE=%BUILD_TYPE%

if errorlevel 1 (
    echo.
    echo [ERROR] CMake configuration failed!
    echo.
    echo Common issues:
    echo   1. Missing dependencies (SDL2, zlib, libpng, libjpeg)
    echo   2. Incorrect Visual Studio environment
    echo.
    echo To install dependencies with vcpkg:
    echo   vcpkg install sdl2:x64-windows zlib:x64-windows libpng:x64-windows libjpeg-turbo:x64-windows
    echo.
    cd ..
    pause
    exit /b 1
)

:: Build
echo.
echo [2/3] Building...
nmake

if errorlevel 1 (
    echo.
    echo [ERROR] Build failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo [3/3] Build completed successfully!
echo.

cd ..

:: Copy executable to root for easy access
echo Copying executable to project root...
if exist %BUILD_DIR%\bin\gameswf_test_ogl.exe (
    copy /Y %BUILD_DIR%\bin\gameswf_test_ogl.exe gameswf_test_ogl.exe >nul
    echo [OK] Executable copied: gameswf_test_ogl.exe
)

echo.
echo ==========================================
echo Build Summary
echo ==========================================
echo.
echo Output files:
echo   - %BUILD_DIR%\bin\gameswf_test_ogl.exe
echo   - gameswf_test_ogl.exe (copied to root)
echo.
echo Usage:
echo   gameswf_test_ogl.exe [options] movie.swf
echo.
echo Examples:
echo   gameswf_test_ogl.exe gameswf\samples\gameswf_logo.swf
echo   gameswf_test_ogl.exe -w 1024x768 gameswf\samples\test_gradients_alpha.swf
echo.

pause
