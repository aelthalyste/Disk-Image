@echo off
rem  -D_DEBUG
set build_options= -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS
set compile_flags= -nologo /I"../inc" /GS- /MTd /EHsc /W3 /wd4018 /Od /fsanitize=address /Zi /DEBUG:FULL /FC /F 16777216 /std:c++17 /MP 
rem /fsanitize=address /Zi /DEBUG:FULL /FC
rem /d2cgsummary /showIncludes
rem /Bt
rem -ftime-trace

set linker_flags=
rem "liblz4_static.lib" "fltLib.lib" "vssapi.lib" "libzstd.lib"
rem if not exist precompiled.obj cl %compile_flags% %build_options% /c /Yc"precompiled.h" "precompiled.cpp" 


ctime.exe -begin DiskImageNative  

cl main.cpp bg.cpp file_explorer.cpp platform_io.cpp restore.cpp nar_win32.cpp nar.cpp package.cpp %build_options% %compile_flags% /I"../inc" %linker_flags%
rem /Yu"precompiled.h" "precompiled.obj"
rem  file_explorer.cpp platform_io.cpp restore.cpp nar_win32.cpp nar.cpp

ctime.exe -end DiskImageNative

rem CLEANUP

if exist *.ilk del *.ilk
if exist *.exp del *.exp

REM delete all .obj files except precompiled.obj one
if exist precompiled.obj ren precompiled.obj precompiled.obj.keep
if exist *.obj del *.obj
if exist precompiled.obj.keep ren precompiled.obj.keep precompiled.obj
