@echo off
setlocal
set ROOT=%~dp0
set CLI="%ROOT%tools\arduino\bin\arduino-cli.exe"
set CFG="%ROOT%tools\arduino\arduino-cli.yaml"
set FQBN=esp8266:esp8266:nodemcu:eesz=4M1M
set LOCALAPPDATA=%ROOT%tools\arduino\localappdata
if /I "%1"=="upload" goto up
if /I "%1"=="monitor" goto mon
if /I "%1"=="servotest" goto st
%CLI% --config-file %CFG% compile --fqbn %FQBN% --build-path "%ROOT%tools\arduino\build" "%ROOT%firmware\dorm_light"
echo.
echo [OK] binary for web-OTA: tools\arduino\build\dorm_light.ino.bin
goto end
:up
%CLI% --config-file %CFG% upload --port %2 --fqbn %FQBN% --input-dir "%ROOT%tools\arduino\build" "%ROOT%firmware\dorm_light"
goto end
:mon
%CLI% --config-file %CFG% monitor --port %2 --config baudrate=115200
goto end
:st
%CLI% --config-file %CFG% upload --port %2 --fqbn esp8266:esp8266:nodemcu "%ROOT%firmware\servo_test"
goto end
:end
pause
