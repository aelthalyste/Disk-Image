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


#include <Windows.h>
struct linear_allocator{
    void* Memory;
    size_t Used;
    size_t Committed;
    size_t Reserved;
    size_t CommitStepSize = 16*1024*1024;
};


void*
LinearAllocateAligned(linear_allocator* Allocator, size_t N, size_t Align){
    void* Result = 0;
    void* VirtualAllocResult = 0;
	int64_t CommitLeft   = Allocator->Committed - Allocator->Used;
	int64_t ReservedLeft = Allocator->Reserved  - Allocator->Used;
    if(ReservedLeft <= N){
        return NULL;
    } 
    
    uint64_t AligmentOff = (uintptr_t)((uint8_t*)Allocator->Memory + Allocator->Used) % Align;
    
    N += AligmentOff;
    
    if(CommitLeft < N){
        size_t StepCountToAllocate = (N-CommitLeft)/Allocator->CommitStepSize + 1;
        VirtualAllocResult = VirtualAlloc((uint8_t*)Allocator->Memory + Allocator->Committed,  StepCountToAllocate*Allocator->CommitStepSize, 
                                          MEM_COMMIT, 
                                          PAGE_READWRITE);
        Allocator->Committed += StepCountToAllocate*Allocator->CommitStepSize;
    }
    Result = (void*)((uint8_t*)Allocator->Memory + Allocator->Used + AligmentOff);
    Allocator->Used += N;
    
    return Result;
}


void*
LinearAllocate(linear_allocator* Allocator, size_t N){
    return LinearAllocateAligned(Allocator, N, 1);
}


linear_allocator
NarCreateLinearAllocator(size_t Capacity, 
                         size_t CommitStepSize = 16*1024*1024)
{
    if(CommitStepSize > Capacity){
        CommitStepSize = Capacity;
    }
    
    linear_allocator Result = {0};
    Result.Memory         = VirtualAlloc(0, Capacity, MEM_RESERVE, PAGE_READWRITE);
    Result.Used           = 0;
    Result.Committed      = 0;
    Result.Reserved       = Capacity;
    Result.CommitStepSize = CommitStepSize;
    
    Result.Committed = CommitStepSize;
    VirtualAlloc(Result.Memory, Result.CommitStepSize, MEM_COMMIT, PAGE_READWRITE);
    
    return Result;
}

void
LinearCommitFirstN(linear_allocator* Allocator, size_t N){
    if(N > Allocator->Reserved){
        N = Allocator->Reserved;
    }
    VirtualAlloc(Allocator->Memory, N, MEM_COMMIT, PAGE_READWRITE);
}

void
NarFreeLinearAllocator(linear_allocator* Allocator){
    VirtualFree(Allocator->Memory, 0, MEM_RELEASE);
}
