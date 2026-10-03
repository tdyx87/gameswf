@echo off
chcp 65001 >nul
echo ==========================================
echo GameSWF Dependency Setup for Windows
echo ==========================================
echo.

:: Check if vcpkg is already installed
if defined VCPKG_ROOT (
    echo [OK] vcpkg already installed at: %VCPKG_ROOT%
    set VCPKG_DIR=%VCPKG_ROOT%
    goto INSTALL_DEPS
)

:: Try to find vcpkg in common locations
if exist C:\vcpkg\vcpkg.exe (
    set VCPKG_DIR=C:\vcpkg
    goto INSTALL_DEPS
)

if exist %USERPROFILE%\vcpkg\vcpkg.exe (
    set VCPKG_DIR=%USERPROFILE%\vcpkg
    goto INSTALL_DEPS
)

:: Install vcpkg
echo vcpkg not found. Installing vcpkg...
echo.

set VCPKG_DIR=C:\vcpkg

echo Cloning vcpkg repository...
git clone https://github.com/Microsoft/vcpkg.git "%VCPKG_DIR%"

if errorlevel 1 (
    echo [ERROR] Failed to clone vcpkg repository!
    echo Please install git or download vcpkg manually.
    pause
    exit /b 1
)

echo.
echo Bootstrapping vcpkg...
call "%VCPKG_DIR%\bootstrap-vcpkg.bat"

if errorlevel 1 (
    echo [ERROR] Failed to bootstrap vcpkg!
    pause
    exit /b 1
)

:: Set environment variable
echo.
echo Setting VCPKG_ROOT environment variable...
setx VCPKG_ROOT "%VCPKG_DIR%"
echo [OK] Environment variable set. You may need to restart your terminal.

:INSTALL_DEPS
echo.
echo ==========================================
echo Installing Dependencies
echo ==========================================
echo.
echo Installing: SDL2, zlib, libpng, libjpeg-turbo
echo This may take several minutes...
echo.

"%VCPKG_DIR%\vcpkg.exe" install sdl2:x64-windows zlib:x64-windows libpng:x64-windows libjpeg-turbo:x64-windows

if errorlevel 1 (
    echo.
    echo [ERROR] Failed to install dependencies!
    pause
    exit /b 1
)

echo.
echo ==========================================
echo Setup Complete!
echo ==========================================
echo.
echo Dependencies installed successfully!
echo.
echo You can now run build.bat to compile GameSWF.
echo.

pause
