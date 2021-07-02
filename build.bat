@echo off

call :StartTimer

set build_options= -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS
set compile_flags= -nologo /MT /EHsc /DEBUG:FULL /Zi /FC /Fa /W0 /Od /F 16777216 /std:c++17 /fsanitize=address
rem /fsanitize=address
rem /DEBUG:FULL /Zi /FC /Fa
rem /fsanitize=address
rem /d2cgsummary /showIncludes
rem /Bt
rem -ftime-trace

set linker_flags= /INCREMENTAL:NO "fltLib.lib" "vssapi.lib" "libzstd_static.lib" "libzstd.lib"

pushd minispy_user\
rem if not exist precompiled.obj 
if not exist precompiled.obj cl /c  %compile_flags% /Yc"precompiled.h" "precompiled.cpp" 

rem "file_explorer.cpp" "restore.cpp" "platform_io.cpp"
cl /Yu"precompiled.h" "mspyUser.cpp" "precompiled.obj" %build_options% %compile_flags% /I"../inc" %linker_flags%

rem CLEANUP

call :StopTimer
call :DisplayTimerResult

if exist *.ilk del *.ilk
if exist *.exp del *.exp

popd

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
