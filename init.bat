@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "BUILD_DIR=build"
set "BUILD_TYPE=Debug"
set "RECONFIGURE=0"
set "CLEAN=0"
set "CMAKE_ARGS="

:parse_args
if "%~1"=="" goto args_done

if "%~1"=="--reconfigure" set "RECONFIGURE=1"
if "%~1"=="-c" set "CLEAN=1"
if "%~1"=="--clean" set "CLEAN=1"
if "%~1"=="-d" set "BUILD_TYPE=Debug"
if "%~1"=="--debug" set "BUILD_TYPE=Debug"
if "%~1"=="-r" set "BUILD_TYPE=Release"
if "%~1"=="--release" set "BUILD_TYPE=Release"

if "%~1:~0,2%"=="-D" (
    set "CMAKE_ARGS=!CMAKE_ARGS! %~1"
)

shift
goto parse_args

:args_done

if "%CLEAN%"=="1" (
    echo Cleaning build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)

set "GENERATOR_ARGS="

where ninja >nul 2>&1
if not errorlevel 1 (
    set "GENERATOR_ARGS=-G Ninja"
)

if not exist "%BUILD_DIR%\CMakeCache.txt" goto configure
if "%RECONFIGURE%"=="1" goto configure

echo Build directory already initialized.
echo Run with --reconfigure to change the build type.
goto done


:configure
echo Initializing CMake build directory (%BUILD_TYPE%)...
cmake -S . -B "%BUILD_DIR%" %GENERATOR_ARGS% -DCMAKE_BUILD_TYPE=%BUILD_TYPE% %CMAKE_ARGS%
if errorlevel 1 exit /b %errorlevel%


:done
echo.
echo To build:
echo   cmake --build %BUILD_DIR% --parallel
echo.
echo To build a target:
echo   cmake --build %BUILD_DIR% --target ^<target^> --parallel

endlocal

