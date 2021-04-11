@echo off

call :StartTimer


set build_options= -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS
set compile_flags=  -nologo /MT /EHsc /W0 /DEBUG:FULL /Zi /FC /O2 /Fa /INCREMENTAL:NO /F 16777216 
set linker_flags=  "fltLib.lib" "vssapi.lib" "../../minispy_user/libzstd_static.lib" "../../minispy_user/libzstd.dll.a"

rem /fsanitize=address
rem  /fsanitize=address


rem "../../minispy_user/file_explorer.cpp" "../../minispy_user/platform_io.cpp" "../../minispy_user/restore.cpp"

if not exist build\minispy_user mkdir build\minispy_user
pushd build\minispy_user\
cl "../../minispy_user/mspyUser.cpp"  %build_options% %compile_flags% /I "../../inc" %linker_flags%
popd
rem 
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
echo Build Time : %TookTime:~0,-2%.%TookTimePadded:~-3% seconds
goto :EOF
