@echo off
set build_options= -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS
set compile_flags= /Debug:FULL /Zi /W4 /Od /INCREMENTAL:NO /clr
set linker_flags=  "fltLib.lib" "vssapi.lib" "kernel32.lib" "user32.lib" "gdi32.lib" "winspool.lib" "comdlg32.lib" "advapi32.lib" "shell32.lib" "ole32.lib" "oleaut32.lib" "uuid.lib" "odbc32.lib" "odbccp32.lib"

if not exist build\NarDiWrapper mkdir build\NarDiWrapper
pushd build\NarDIWrapper\
cl "../../NarDIWrapper/NarDIWrapper.cpp" %build_options% %compile_flags% /I "../../inc" /I "../../minispy_user"  %linker_flags%
popd
