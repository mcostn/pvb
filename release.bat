@echo off
setlocal

set "APP_NAME=pvb"
set "BUILD_DIR=build"

set "PACKAGE_ROOT=.release"
set "PACKAGE_NAME=%APP_NAME%-windows-x86_64"
set "PACKAGE_DIR=%PACKAGE_ROOT%\%PACKAGE_NAME%"
set "ARCHIVE=%PACKAGE_NAME%.zip"

if exist "%PACKAGE_ROOT%" rmdir /s /q "%PACKAGE_ROOT%"
if exist "%ARCHIVE%" del "%ARCHIVE%"

call init.bat --release --reconfigure -DBUILD_TESTS=OFF
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%" --config Release --parallel
if errorlevel 1 exit /b %errorlevel%

mkdir "%PACKAGE_DIR%"

copy "%BUILD_DIR%\Release\%APP_NAME%.exe" "%PACKAGE_DIR%\" >nul
xcopy "examples" "%PACKAGE_DIR%\examples\" /E /I /Q >nul

tar -a -c -f "%ARCHIVE%" -C "%PACKAGE_ROOT%" "%PACKAGE_NAME%"
if errorlevel 1 exit /b %errorlevel%

rmdir /s /q "%PACKAGE_ROOT%"

echo Created %ARCHIVE%

endlocal
