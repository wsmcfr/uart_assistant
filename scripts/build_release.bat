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

REM 从 CMakeLists.txt 读取项目版本，避免脚本版本号长期落后于主工程。
for /f "tokens=*" %%v in ('powershell -NoProfile -ExecutionPolicy Bypass -Command "$text = Get-Content -Raw -LiteralPath '%PROJECT_DIR%\CMakeLists.txt'; if ($text -match 'project\s*\(\s*ComAssistant[\s\S]*?VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') { $matches[1] } else { 'unknown' }"') do set VERSION=%%v

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

REM 兼容 MinGW/Ninja 单配置生成器和 MSVC 多配置生成器的输出目录差异。
set APP_EXE=%BUILD_DIR%\ComAssistant.exe
if not exist "%APP_EXE%" if exist "%BUILD_DIR%\Release\ComAssistant.exe" set APP_EXE=%BUILD_DIR%\Release\ComAssistant.exe
if not exist "%APP_EXE%" (
    echo Error: ComAssistant.exe not found in "%BUILD_DIR%" or "%BUILD_DIR%\Release"
    exit /b 1
)
for %%i in ("%APP_EXE%") do set APP_DIR=%%~dpi

"%QT_BIN_DIR%windeployqt.exe" --release --no-translations "%APP_EXE%"

if %ERRORLEVEL% neq 0 (
    echo Warning: Qt deployment may have failed
)

REM 复制翻译文件
echo Copying translation files...
if not exist "%APP_DIR%translations" mkdir "%APP_DIR%translations"
copy /y "%BUILD_DIR%\*.qm" "%APP_DIR%translations\" >nul 2>nul

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
echo Output: %APP_DIR%
echo ============================================

endlocal
