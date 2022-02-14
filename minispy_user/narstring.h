/* date = August 6th 2021 10:35 am */
#pragma once

//#include "memory.h"
#include <stdint.h>
#include "utf8.h"
typedef char UTF8;


// windows implementation

#if _WIN32
#include <Windows.h>


static inline wchar_t * NarUTF8ToWCHAR(const UTF8 *s) {
    auto CharactersNeeded = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s, -1, NULL, 0);
    wchar_t *Result = (wchar_t *)calloc(CharactersNeeded, sizeof(Result[0]));    
    
    MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s, -1, Result, CharactersNeeded);
    return Result;
}


static inline UTF8* NarWCHARToUTF8(wchar_t *Str) {
    
    auto OutLen = WideCharToMultiByte(CP_UTF8, 0, Str, -1, NULL, 0, NULL, NULL);
    OutLen += 1;

    UTF8* Result = (UTF8 *)calloc(OutLen, 1);
    
    WideCharToMultiByte(CP_UTF8, 0, Str, -1, (LPSTR)Result, OutLen, NULL, NULL);
    
    return Result;
}



static inline int NarGetWCHARToUTF8ConversionSize(const wchar_t *Input) {
    int Result = WideCharToMultiByte(CP_UTF8, 0, Input, -1, NULL, 0, NULL, NULL); 
    return Result;
}


static inline int NarGetUTF8ToWCHARConversionSize(const UTF8 *Input) {
    int Result = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)Input, -1, NULL, 0);
    Result *= 2;
    return Result;
}


// returns true if Input can fit Output buffer.
static inline bool NarWCHARToUTF8WithoutAllocation(const wchar_t *Input, UTF8 *Output, int OutputSize){
    auto OutputNeeded = WideCharToMultiByte(CP_UTF8, 0, Input, -1, NULL, 0, NULL, NULL);
    if (OutputNeeded < OutputSize) 
        WideCharToMultiByte(CP_UTF8, 0, Input, -1, (LPSTR)Output, OutputSize, NULL, NULL);
    else                            
        return false;

    return true;
}


// _WIN32
#else
#error implement linux?
#endif 



