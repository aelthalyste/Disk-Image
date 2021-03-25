#pragma once

struct nar_arena{
	unsigned char* Memory;
	size_t Used;
	size_t Capacity;
};

static inline nar_arena
ArenaInit(void* Memory, size_t MemSize){
	nar_arena Result;
	Result.Memory = (unsigned char*)Memory;
	Result.Used = 0;
	Result.Capacity = MemSize;
	return Result;
}

static inline void*
ArenaAllocate(nar_arena *Arena, size_t s){
	void* Result = 0;

	if(0 == Arena) return Result;

	size_t left = Arena->Capacity - Arena->Used;
	if(s < left){
		Result = Arena->Memory + Arena->Used;
		Arena->Used += s;
	}
	return Result;
}

static inline void
ArenaFreeBytes(nar_arena* Arena, size_t s){
	if(0!=Arena){
		if(s >= Arena->Used){
			s = Arena->Used;
		}
		Arena->Used -= s;
	}
}
