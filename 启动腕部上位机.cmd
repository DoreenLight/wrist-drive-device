@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0"
set "APP=%CD%\build\Desktop_Qt_6_8_2_MinGW_64_bit_Release\Uart_controller.exe"
if not exist "%APP%" exit /b 2
if "%~1"=="--check-paths" (
  if not exist "%CD%\02_采集数据\telemetry" exit /b 3
  if not exist "%CD%\build\Desktop_Qt_6_8_2_MinGW_64_bit_Release\Qt6Core.dll" exit /b 4
  echo Deployment and archive paths OK.
  exit /b 0
)
start "" "%APP%" %*
