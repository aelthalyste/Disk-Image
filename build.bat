@echo off

call :StartTimer

set build_options= -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS
set compile_flags=   /Debug:FULL /Zi /FC /W0 /Od /INCREMENTAL:NO /EHsc
set linker_flags=  "fltLib.lib" "vssapi.lib"

if not exist build\minispy_user mkdir build\minispy_user
pushd build\minispy_user\
cl "../../minispy_user/mspyUser.cpp" %build_options% %compile_flags% /I "../../inc" %linker_flags%
popd

call :StopTimer
call :DisplayTimerResult



REM TIMING THINGS BELOW

:StartTimer
:: Store start time
set StartTIME=%TIME%
for /f "usebackq tokens=1-4 delims=:., " %%f in (`echo %StartTIME: =0%`) do set /a Start100S=1%%f*360000+1%%g*6000+1%%h*100+1%%i-36610100
goto :EOF

:StopTimer
:: Get the end time
set StopTIME=%TIME%
for /f "usebackq tokens=1-4 delims=:., " %%f in (`echo %StopTIME: =0%`) do set /a Stop100S=1%%f*360000+1%%g*6000+1%%h*100+1%%i-36610100
:: Test midnight rollover. If so, add 1 day=8640000 1/100ths secs
if %Stop100S% LSS %Start100S% set /a Stop100S+=8640000
set /a TookTime=%Stop100S%-%Start100S%
set TookTimePadded=0%TookTime%
goto :EOF

:DisplayTimerResult
:: Show timer start/stop/delta
echo Build TIme : %TookTime:~0,-2%.%TookTimePadded:~-3% seconds
goto :EOF
