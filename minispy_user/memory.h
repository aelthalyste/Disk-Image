#pragma once

#include <stdint.h>
#include <memory.h>

struct nar_arena{
	unsigned char* Memory;
	size_t Used;
	size_t Capacity;
};

static  nar_arena
ArenaInit(void* Memory, size_t MemSize){
	nar_arena Result;
	Result.Memory = (unsigned char*)Memory;
	Result.Used = 0;
	Result.Capacity = MemSize;
	return Result;
}

static void*
ArenaAllocateAligned(nar_arena *Arena, size_t s, size_t aligment){
	void* Result = 0;
    
	if(!Arena) 
        return Result;
    
	size_t left = Arena->Capacity - Arena->Used;
	if(s < left){
		uint64_t AligmentOff = (uintptr_t)((uint8_t*)Arena->Memory + Arena->Used) % aligment;
		Result = Arena->Memory + Arena->Used + AligmentOff;
		Arena->Used += AligmentOff;
		Arena->Used += s;
	}
    
	return Result;
	
}

static  void*
ArenaAllocate(nar_arena *Arena, size_t s){
    return ArenaAllocateAligned(Arena, s, 1);
}

static void
ArenaReset(nar_arena *Arena){
    memset(Arena->Memory, 0, Arena->Used);
    Arena->Used = 0;
}


static  void
ArenaFreeBytes(nar_arena* Arena, size_t s){
	if(!Arena){
		if(s >= Arena->Used){
			s = Arena->Used;
		}
		Arena->Used -= s;
	}
}
