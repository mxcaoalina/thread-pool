@echo off
echo Building Thread Pool project...

:: Check if CMake is installed
where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Error: CMake is not installed or not in PATH
    echo Please install CMake from https://cmake.org/download/
    exit /b 1
)

:: Check if Visual Studio is installed
if not exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    echo Error: Visual Studio 2022 is not installed
    echo Please install Visual Studio 2022 with C++ support
    exit /b 1
)

:: Set VCPKG_ROOT environment variable
set VCPKG_ROOT=%USERPROFILE%\vcpkg

:: Create build directory if it doesn't exist
if not exist build mkdir build
cd build

:: Configure with CMake
echo Configuring with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64

:: Build the project
echo Building project...
cmake --build . --config Release

:: Run tests
echo Running tests...
ctest -C Release

cd ..
echo Build complete! 