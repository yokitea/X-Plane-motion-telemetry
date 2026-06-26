@echo off
echo Building X-Plane 12 Motion Telemetry Plugin...

:: Set path ke SDK yang baru saja di-clone
set XPSDK=E:\X-Plane-motion-telemetry\SDK

:: Compile ke win.xpl
g++ -shared -o win.xpl xplane_telemetry.cpp ^
    -I"%XPSDK%\sdk\CHeaders\XPLM" ^
    -I"%XPSDK%\sdk\CHeaders\Widgets" ^
    -DIBM=1 -DXPLM200 ^
    "%XPSDK%\sdk\Libraries\Win\XPLM_64.lib" ^
    -lws2_32 ^
    -static-libgcc -static-libstdc++ ^
    -O2

if %errorlevel% neq 0 (
    echo Build Failed!
    pause
    exit /b %errorlevel%
)

echo Build Success! 
echo Copying win.xpl to E:\Games\XP12\Resources\plugins\MotionTelemetry\64\
mkdir "E:\Games\XP12\Resources\plugins\MotionTelemetry\64" 2>nul
copy win.xpl "E:\Games\XP12\Resources\plugins\MotionTelemetry\64\win.xpl"

echo Done!
