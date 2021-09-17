@echo off

set build_options= -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS
set compile_flags= -nologo /GS- /MT /EHsc /W0 /Od /F 16777216 /Zi /FC /FAs /std:c++17 /MP /fsanitize=address
rem /fsanitize=address /DEBUG:FULL /Zi /FC /Fa 
rem /FAs
rem /fsanitize=address
rem /d2cgsummary /showIncludes
rem /Bt
rem -ftime-trace

set linker_flags= /WX "fltLib.lib" "vssapi.lib" "libzstd.lib"
rem 

pushd minispy_user\

rem if not exist precompiled.obj 
cl /c %compile_flags% /Yc"precompiled.h" "precompiled.cpp" 

rem "file_explorer.cpp" "restore.cpp" "platform_io.cpp"

rem ctime.exe -begin DiskImageNative  

cl /Yu"precompiled.h" main.cpp backup.cpp file_explorer.cpp platform_io.cpp restore.cpp nar_win32.cpp nar.cpp narstring.cpp "precompiled.obj" %build_options% %compile_flags% /I"../inc" %linker_flags%

REM UNITY BUILD
rem cl /Yu"precompiled.h" main.cpp "precompiled.obj" %build_options% %compile_flags% /I"../inc" %linker_flags%

rem ctime.exe -end DiskImageNative

rem CLEANUP

if exist *.ilk del *.ilk
if exist *.exp del *.exp

REM delete all .obj files except precompiled.obj one
if exist precompiled.obj ren precompiled.obj precompiled.obj.keep
if exist *.obj del *.obj
if exist precompiled.obj.keep ren precompiled.obj.keep precompiled.obj

popd
