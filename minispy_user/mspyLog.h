#pragma once

#include "precompiled.h"

inline void
PrintDebugRecords();

#if 0
// some debug stuff
int64_t NarGetPerfCounter();
int64_t NarGetPerfFrequency();

// time elapsed in ms
double NarTimeElapsed(int64_t start);
#endif


#if 0
template<typename DATA_TYPE>
struct array {
    DATA_TYPE* Data;
    uint32_t   Count;
    uint32_t   ReserveCount;
    nar_arena  Arena;
    
    array(){
        Data         = 0;
        Count        = 0;
        ReserveCount = 0;
        Arena        = {0};
    }
    
    inline void Insert(const DATA_TYPE &Val) {
        if(Count < ReserveCount){
            memcpy(&Data[Count], &Val, sizeof(DATA_TYPE));
            Count++;
        }
        else{
            // TODO
            ASSERT(FALSE);
        }
        ASSERT(FALSE);
    }
    
    inline DATA_TYPE& operator[](uint64_t s){return Data[s];}
    
};

template<typename DT> array<DT>
InitArray(nar_arena *Arena, size_t InitalReserveCount){
    
    array<DT> Result;
    memset(&Result, 0, sizeof(Result));
    Result.Data         = (DT*)ArenaAllocateAligned(Arena, InitalReserveCount*sizeof(DT), sizeof(DT));
    Result.Count        = 0;
    Result.ReserveCount = InitalReserveCount;
    
    return Result;
}

#endif



#if 0
BOOLEAN
NarGetBackupsInDirectory(const wchar_t* Directory, backup_metadata* B, int BufferSize, int* FoundCount);
#endif
