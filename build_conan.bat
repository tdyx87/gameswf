@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

echo ==========================================
echo GameSWF Windows Build Script (Conan)
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

:: Check for Conan
where conan >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Conan not found! Please install Conan.
    echo.
    echo Install via pip:
    echo   pip install conan
    echo.
    echo Or download from: https://conan.io/downloads
    pause
    exit /b 1
)

echo [OK] Conan found

:: Set build configuration
set BUILD_TYPE=Release
set BUILD_DIR=build_conan

:: Parse command line arguments
if "%~1"=="debug" (
    set BUILD_TYPE=Debug
    echo [INFO] Building DEBUG configuration
) else (
    echo [INFO] Building RELEASE configuration (use 'build_conan.bat debug' for debug build)
)

echo.
echo ==========================================
echo Installing Dependencies with Conan
echo ==========================================
echo.

:: Detect Conan profile if not exists
conan profile detect --force >nul 2>&1

:: Install dependencies with Conan
echo [1/4] Installing Conan dependencies...
conan install . --output-folder=%BUILD_DIR% --build=missing --settings=build_type=%BUILD_TYPE%

if errorlevel 1 (
    echo.
    echo [ERROR] Conan install failed!
    echo.
    echo Common issues:
    echo   1. No internet connection
    echo   2. Conan remote not configured
    echo.
    echo Try: conan remote add conancenter https://center.conan.io
    pause
    exit /b 1
)

echo.
echo ==========================================
echo Building Project
echo ==========================================
echo.

:: Create build directory and navigate
cd %BUILD_DIR%

:: Configure with CMake
echo [2/4] Configuring with CMake...
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_PREFIX_PATH="%CD%"

if errorlevel 1 (
    echo.
    echo [ERROR] CMake configuration failed!
    cd ..
    pause
    exit /b 1
)

:: Build
echo.
echo [3/4] Building...
nmake

if errorlevel 1 (
    echo.
    echo [ERROR] Build failed!
    cd ..
    pause
    exit /b 1
)

echo.
echo [4/4] Build completed successfully!
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
