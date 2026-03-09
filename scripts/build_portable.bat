@echo off
REM ============================================
REM ComAssistant Portable Build Script
REM Author: ComAssistant Team
REM Date: 2026-01-16
REM ============================================

setlocal enabledelayedexpansion

REM 设置变量
set PROJECT_DIR=%~dp0..
set BUILD_DIR=%PROJECT_DIR%\build_release
set PORTABLE_DIR=%PROJECT_DIR%\ComAssistant_Portable
set VERSION=1.1.0

echo ============================================
echo ComAssistant Portable Build Script
echo Version: %VERSION%
echo ============================================

REM 检查 Release 构建是否存在
if not exist "%BUILD_DIR%\Release\ComAssistant.exe" (
    echo Error: Release build not found. Please run build_release.bat first.
    exit /b 1
)

REM 清理旧的便携版目录
echo.
echo [1/3] Cleaning old portable directory...
if exist "%PORTABLE_DIR%" rmdir /s /q "%PORTABLE_DIR%"

REM 复制文件
echo.
echo [2/3] Copying files...
mkdir "%PORTABLE_DIR%"
xcopy /e /i /y "%BUILD_DIR%\Release\*" "%PORTABLE_DIR%\" >nul

REM 复制文档
if exist "%PROJECT_DIR%\README.md" copy /y "%PROJECT_DIR%\README.md" "%PORTABLE_DIR%\" >nul
if exist "%PROJECT_DIR%\LICENSE" copy /y "%PROJECT_DIR%\LICENSE" "%PORTABLE_DIR%\" >nul
if exist "%PROJECT_DIR%\CHANGELOG.md" copy /y "%PROJECT_DIR%\CHANGELOG.md" "%PORTABLE_DIR%\" >nul

REM 创建便携版配置文件
echo.
echo [3/3] Creating portable marker...
echo. > "%PORTABLE_DIR%\portable.txt"

REM 创建 ZIP 包
echo.
echo Creating ZIP archive...

REM 检查 7-Zip
where 7z >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Warning: 7-Zip not found. Please manually create ZIP archive.
    echo Portable files are in: %PORTABLE_DIR%
) else (
    pushd "%PROJECT_DIR%"
    7z a -tzip "ComAssistant_%VERSION%_Portable.zip" "ComAssistant_Portable\*" -r
    if %ERRORLEVEL% equ 0 (
        echo ZIP archive created: %PROJECT_DIR%\ComAssistant_%VERSION%_Portable.zip
    )
    popd
)

echo.
echo ============================================
echo Portable build completed!
echo Output: %PORTABLE_DIR%
echo ============================================

endlocal
