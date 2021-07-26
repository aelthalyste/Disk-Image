#pragma once

#include <stdint.h>
#include <memory.h>


struct nar_arena{
	unsigned char* Memory;
	size_t Used;
	size_t Capacity;
    size_t Aligment;
};

struct memory_restore_point{
    size_t Used;
};

static inline memory_restore_point 
ArenaGetRestorePoint(nar_arena *Arena){
    memory_restore_point Result = {};
    Result.Used = Arena->Used;
    return Result;
}

static inline void
ArenaRestoreToPoint(nar_arena *Arena, memory_restore_point Point){
    Arena->Used = Point.Used;
}

static  nar_arena
ArenaInit(void* Memory, size_t MemSize, size_t Aligment = 8){
	nar_arena Result = {};
	Result.Memory = (unsigned char*)Memory;
	Result.Used = 0;
	Result.Capacity = MemSize;
	Result.Aligment = Aligment;
    return Result;
}

static inline void*
ArenaAllocateAligned(nar_arena *Arena, size_t s, size_t aligment){
	void* Result = 0;
    
	if(!Arena) {
        return Result;
    }
    
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
    return ArenaAllocateAligned(Arena, s, Arena->Aligment);
}

static inline void*
ArenaAllocateZero(nar_arena *Arena, size_t Size){
    void* Result = ArenaAllocate(Arena, Size);
    memset(Result, 0, Size);
    return Result;
}

static inline void*
ArenaAllocateZeroAligned(nar_arena *Arena, size_t Size, size_t Aligment){
    void *Result = ArenaAllocateAligned(Arena, Size, Aligment);
    memset(Result, 0, Size);
    return Result;
}


static void
ArenaReset(nar_arena *Arena){
    memset(Arena->Memory, 0, Arena->Used);
    Arena->Used = 0;
}


static inline void
ArenaFreeBytes(nar_arena* Arena, size_t s){
	if(!Arena){
		if(s >= Arena->Used){
			s = Arena->Used;
		}
		Arena->Used -= s;
	}
}



// POOL
struct nar_pool_entry{
    nar_pool_entry *Next;
};

struct nar_memory_pool{
    void *Memory;
    nar_pool_entry *Entries;
    int PoolSize;
    int EntryCount;
};


static inline nar_memory_pool
NarInitPool(void *Memory, uint32_t MemorySize, uint32_t PoolSize){
    
    if(Memory == NULL) return { 0 };
    
    nar_memory_pool Result = {0};
    Result.Memory          = Memory;
    Result.PoolSize        = PoolSize;
    Result.EntryCount      = MemorySize / PoolSize;
    
    for(size_t i = 0; i < Result.EntryCount - 1; i++){
        nar_pool_entry *entry = (nar_pool_entry*)((char*)Memory + (PoolSize * i));
        entry->Next           = (nar_pool_entry*)((char*)entry + PoolSize);
    }
    
    nar_pool_entry *entry = (nar_pool_entry*)((char*)Memory + (PoolSize * (Result.EntryCount - 1)));
    entry->Next = 0;
    Result.Entries = (nar_pool_entry*)Memory;
    
    return Result;
}

static inline void*
PoolAllocate(nar_memory_pool *Pool){
    if(Pool->Entries == 0){
        return 0;
    }
    void* Result = Pool->Entries;
    Pool->Entries = Pool->Entries->Next;
    return Result;
}

static inline void
PoolDeallocate(nar_memory_pool *Pool, void *Memory){
    if(Memory == 0){
        return;
    }
    
    nar_pool_entry* Entr = Pool->Entries;
    // edge case
    memset(Memory, 0, Pool->PoolSize);
    if(Pool->Entries == 0){
        Pool->Entries = (nar_pool_entry*)Memory;
        return;
    }
    
    while(1){
        if(Entr->Next == 0){
            Entr->Next = (nar_pool_entry*)Memory;
            break;
        }
        Entr = Entr->Next;
    }
}


#if _WIN32
#include <Windows.h>
struct linear_allocator{
    void* Memory;
    size_t Used;
    size_t Committed;
    size_t Reserved;
    size_t CommitStepSize = 16*1024*1024;
};


static inline void*
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



static inline void*
LinearAllocate(linear_allocator* Allocator, size_t N){
    return LinearAllocateAligned(Allocator, N, 1);
}


static inline linear_allocator
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

static inline void
LinearCommitFirstN(linear_allocator* Allocator, size_t N){
    if(N > Allocator->Reserved){
        N = Allocator->Reserved;
    }
    VirtualAlloc(Allocator->Memory, N, MEM_COMMIT, PAGE_READWRITE);
}

static inline void
NarFreeLinearAllocator(linear_allocator* Allocator){
    VirtualFree(Allocator->Memory, 0, MEM_RELEASE);
}
#endif
