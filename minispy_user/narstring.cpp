#include "precompiled.h"
#include "narstring.h"
#include "nar.h"


NarUTF8
NarStringCopy(NarUTF8 Input, nar_arena *Arena){
    NarUTF8 Result = {};
    Result.Str = (uint8_t*)ArenaAllocate(Arena, Input.Len);
    Result.Len = Input.Len;
    Result.Cap = Input.Cap;
    memcpy(Result.Str, Input.Str, Result.Len);
    ASSERT(Result.Str[Result.Len] == 0);
    return Result;
}

bool
NarStringCopy(NarUTF8 *Destination, NarUTF8 Source){
    if(Destination->Cap >= Source.Len){
        memcpy(Destination->Str, Source.Str, Source.Len);
        Destination->Len = Source.Len;
        Destination->Str[Destination->Len] = 0;
        ASSERT(Destination->Str[Destination->Len] == 0); 
        return true;
    }
    return false;
}


bool
NarStringConcatenate(NarUTF8 *Destination, NarUTF8 Append){
    
    // capacity with 0 means its immutable.
    if(Destination->Cap == 0){
        return false;
    }
    if(Append.Len <= 1){
    	return true;
    }
    
    ASSERT(Destination->Cap);
    
    ASSERT(Destination->Cap >= Destination->Len + Append.Len);
    if(Destination->Cap < Destination->Len + Append.Len){
        return false;
    }
    
    for(size_t i = 0; 
        i<Append.Len; 
        i++)
    {
        Destination->Str[i + Destination->Len] = Append.Str[i];
    }
    Destination->Len += Append.Len;
    Destination->Str[Destination->Len] = 0;// null termination
    return true;
}

NarUTF8
NarUTF8Init(void *Memory, uint32_t Len){
    NarUTF8 Result;
    Result.Str = (uint8_t*)Memory;
    Result.Len = 0;
    Result.Cap = Len;
    return Result;
}


NarUTF8
NarGetRootPath(NarUTF8 FileName, nar_arena *Arena){
    NarUTF8 Result = {};
    for(uint32_t i = FileName.Len; i>0; i--){
        if(FileName.Str[i] == '\\' || FileName.Str[i] == '/'){
            Result.Str = (uint8_t*)ArenaAllocateZero(Arena, i + 1);
            Result.Len = i + 1;
            memcpy(Result.Str, FileName.Str, Result.Len);
            return Result;
        }
    }
    
    Result.Str = (uint8_t*)ArenaAllocateZero(Arena, 1);
    Result.Len = 1;
    Result.Cap = 1;
    return Result;
}