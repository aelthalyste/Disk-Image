//#include "restore.cpp"
//#include "platform_io.cpp"




// forces templated functions to be compiled
void
nar_compilation_force(){
    //restore_stream *test = InitFileRestoreStream(std::wstring(L"hello"), 0, 0, 0);    
}

#include <stdio.h>

#include "precompiled.h"

int main(){
    printf("hello world");
    return GetLastError();
}






