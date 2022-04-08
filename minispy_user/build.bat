@echo off
set build_options=-D_DEBUG -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS
set common_compile_flags=-D"BG_DEVELOPER=1" -nologo /I"../inc" /GS- /W3 /wd4018 /Od /fsanitize=address /Zi /DEBUG:FULL /FC /F 16777216 /std:c++17 /MP 
set native_compile_flags=/MTd /EHsc -D"BG_BUILD_AS_DLL=1"
set clr_compile_flags=/clr /LD /MDd NarDiskImageNative.lib -FeNarDiskImageCLR.dll
rem /fsanitize=address /Zi /DEBUG:FULL /FC
rem /d2cgsummary /showIncludes
rem /Bt
rem -ftime-trace

rem /LD -FeNarDiskImageNative.dll

ctime.exe -begin DiskImageNative  

REM BUILD NATIVE LIB
cl main.cpp bg.cpp file_explorer.cpp platform_io.cpp nar_win32.cpp nar.cpp package.cpp %build_options% %common_compile_flags% %native_compile_flags%

set build_options=%build_options% -D"MANAGED=1"
rem cl clr_build.cpp %build_options% %common_compile_flags% %clr_compile_flags% /I"../inc"
REM BUILD DOTNET


ctime.exe -end DiskImageNative



rem CLEANUP

if exist *.ilk del *.ilk
if exist *.exp del *.exp

REM delete all .obj files except precompiled.obj one
if exist precompiled.obj ren precompiled.obj precompiled.obj.keep
if exist *.obj del *.obj
if exist precompiled.obj.keep ren precompiled.obj.keep precompiled.obj


