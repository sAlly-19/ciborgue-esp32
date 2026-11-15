@echo off
title Gravar Firmware Ciborgue no ESP32-S3 (COM4)
cd /d "%~dp0"
echo Liberando a porta COM4 (fechando simulador se estiver aberto)...
powershell -NoProfile -Command "Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like '*ciborgue.py*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }"
timeout /t 1 /nobreak >nul

echo Compilando e gravando firmware C++ no ESP32-S3 (COM4)...
"C:\Users\aliss\AppData\Local\Python\pythoncore-3.14-64\Scripts\pio.exe" run --target upload

echo.
echo Iniciando novamente a Bancada Virtual 296x240...
start "" "C:\Users\aliss\AppData\Local\Python\pythoncore-3.14-64\python.exe" ciborgue.py
pause
