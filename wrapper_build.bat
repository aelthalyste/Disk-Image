@echo off

set build_options= -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS
set compile_flags= -nologo /clr /MD /W0 /O2 /INCREMENTAL:NO /F 16777216 /GL /std:c++17 /MP /Gs- /GS-
set linker_flags= /WX "fltLib.lib" "vssapi.lib" "libzstd_static.lib" "libzstd.lib" /LD

cd NarDIWrapper

cl NarDIWrapper.cpp ../minispy_user/backup.cpp ../minispy_user/file_explorer.cpp ../minispy_user/platform_io.cpp ../minispy_user/restore.cpp ../minispy_user/nar_win32.cpp ../minispy_user/nar.cpp ../minispy_user/narstring.cpp %build_options% %compile_flags% /I"../minispy_user" %linker_flags%

cd ..

rem ctime.exe -begin dinative  
rem /fsanitize=address /DEBUG:FULL /Zi /FC /Fa 
rem 
rem /fsanitize=address
rem /d2cgsummary /showIncludes
rem /Bt
rem -ftime-trace
