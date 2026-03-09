@echo off
chcp 65001 > nul
"F:\串口助手\build\tests\Debug\ComAssistant_tests.exe" > "F:\串口助手\full_test_output.txt" 2>&1
echo Exit code: %ERRORLEVEL%
