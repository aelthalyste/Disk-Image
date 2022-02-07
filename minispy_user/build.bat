@echo off

set build_options= -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS -D_DEBUG
set compile_flags= -nologo /GS- /MTd /EHsc /W3 /wd4018 /Od /Zi /fsanitize=address /DEBUG:FULL /FC /F 16777216 /std:c++17 /MP
rem  
rem /d2cgsummary /showIncludes
rem /Bt
rem -ftime-trace

set linker_flags= "fltLib.lib" "vssapi.lib" "libzstd.lib"

rem if not exist precompiled.obj cl %compile_flags% %build_options% /c /Yc"precompiled.h" "precompiled.cpp" 

rem "file_explorer.cpp" "restore.cpp" "platform_io.cpp"

ctime.exe -begin DiskImageNative  

cl main.cpp backup.cpp file_explorer.cpp platform_io.cpp restore.cpp nar_win32.cpp nar.cpp narstring.cpp  %build_options% %compile_flags% /I"../inc" %linker_flags%
rem /Yu"precompiled.h" "precompiled.obj"

REM UNITY BUILD
rem cl /Yu"precompiled.h" main.cpp "precompiled.obj" %build_options% %compile_flags% /I"../inc" %linker_flags%

ctime.exe -end DiskImageNative

rem CLEANUP

if exist *.ilk del *.ilk
if exist *.exp del *.exp

REM delete all .obj files except precompiled.obj one
if exist precompiled.obj ren precompiled.obj precompiled.obj.keep
if exist *.obj del *.obj
if exist precompiled.obj.keep ren precompiled.obj.keep precompiled.obj
