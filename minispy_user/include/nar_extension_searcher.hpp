#pragma once



#include "nar_ntfs_utils.hpp"
#include "memory.hpp"
#include "bg.hpp"

#include <stdint.h>
#include <windows.h>

struct extension_search_result {
    wchar_t **Files;
    uint64_t Len;
};


struct extension_finder_memory{
    nar_arena Arena;
    linear_allocator StringAllocator;
    
    void*       FileBuffer;
    uint64_t    FileBufferSize;
    
    nar_fs_region * MFTRecords;
    uint64_t           MFTRecordCount;
    
    void*       DirMappingMemory;
    void*       PIDArrMemory;
    
    uint64_t    TotalFC;
};


void                    BG_API NarFreeExtensionFinderMemory(extension_finder_memory* Memory);
extension_finder_memory BG_API NarSetupExtensionFinderMemory(HANDLE VolumeHandle);
extension_search_result BG_API NarFindExtensions(char VolumeLetter, HANDLE VolumeHandle, wchar_t** ExtensionList, size_t ExtensionListCount, extension_finder_memory* Memory);
