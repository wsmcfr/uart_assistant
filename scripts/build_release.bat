@echo off
REM ============================================
REM ComAssistant Release Build Script
REM Author: ComAssistant Team
REM Date: 2026-01-16
REM ============================================

setlocal enabledelayedexpansion

REM 设置变量
set PROJECT_DIR=%~dp0..
set BUILD_DIR=%PROJECT_DIR%\build_release
set VERSION=1.1.0

echo ============================================
echo ComAssistant Release Build Script
echo Version: %VERSION%
echo ============================================

REM 检查 CMake
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: CMake not found in PATH
    exit /b 1
)

REM 检查 NSIS
where makensis >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Warning: NSIS not found in PATH. Installer will not be created.
    set HAS_NSIS=0
) else (
    set HAS_NSIS=1
)

REM 清理旧的构建目录
echo.
echo [1/5] Cleaning old build directory...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

REM CMake 配置
echo.
echo [2/5] Configuring CMake (Release)...
cmake -B "%BUILD_DIR%" -S "%PROJECT_DIR%" -DCMAKE_BUILD_TYPE=Release

if %ERRORLEVEL% neq 0 (
    echo Error: CMake configuration failed
    exit /b 1
)

REM 编译
echo.
echo [3/5] Building project...
cmake --build "%BUILD_DIR%" --config Release --parallel

if %ERRORLEVEL% neq 0 (
    echo Error: Build failed
    exit /b 1
)

REM Qt 部署
echo.
echo [4/5] Deploying Qt runtime...
for /f "tokens=*" %%i in ('where qmake') do set QMAKE_PATH=%%i
for %%i in ("%QMAKE_PATH%") do set QT_BIN_DIR=%%~dpi

"%QT_BIN_DIR%windeployqt.exe" --release --no-translations "%BUILD_DIR%\Release\ComAssistant.exe"

if %ERRORLEVEL% neq 0 (
    echo Warning: Qt deployment may have failed
)

REM 复制翻译文件
echo Copying translation files...
if not exist "%BUILD_DIR%\Release\translations" mkdir "%BUILD_DIR%\Release\translations"
copy /y "%BUILD_DIR%\*.qm" "%BUILD_DIR%\Release\translations\" >nul 2>nul

REM 创建安装包
echo.
echo [5/5] Creating installer...
if %HAS_NSIS%==1 (
    pushd "%PROJECT_DIR%\installer\nsis"
    makensis /DVERSION=%VERSION% ComAssistant.nsi
    if %ERRORLEVEL% neq 0 (
        echo Warning: NSIS installer creation failed
    ) else (
        move /y "ComAssistant_%VERSION%_Setup.exe" "%BUILD_DIR%\" >nul
        echo Installer created: %BUILD_DIR%\ComAssistant_%VERSION%_Setup.exe
    )
    popd
) else (
    echo Skipping installer creation (NSIS not available)
)

echo.
echo ============================================
echo Build completed successfully!
echo Output: %BUILD_DIR%\Release
echo ============================================

endlocal
