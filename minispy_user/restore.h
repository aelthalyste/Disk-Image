#pragma once

#include "platform_io.h"
#include "compression.h"


/*
In order to export these symbosl to .NET, we have to put public keyword before enum
*/


#ifdef  _MANAGED
public
#endif

enum class RestoreSource_Errors: int{
    Error_NoError,
    Error_InsufficentBufferSize,
    Error_DecompressionUnknownContentSize,
    Error_DecompressionErrorContentsize,
    Error_DecompressionCompressedSize,
    Error_Decompression,
    Error_NullCompBuffer,
    Error_NullFileViews,
    Error_NullArg,
    Error_Count
};



#ifdef  _MANAGED
#define DOTNET_ENUM public
#endif

enum class RestoreStream_Errors: int{
    Error_NoError,
    Error_Read,
    Error_Needle,
    Error_Write,
    Error_Count
};

struct restore_instruction {
    nar_record WhereToRead;
    nar_record WhereToWrite;
    int version;
};

struct restore_stream {

    restore_instruction *Instructions;
    uint64_t             InstructionCount;
    uint64_t             InstructionIndex;
    
    // Total bytes needs to be copied to successfully reconstruct given backup. This is cummulative sum from full backup to given version
    size_t BytesToBeCopied; 
    
    // Resultant operation's target size, if target version is not fullbackup, TargetRestoreSize != BytesToBeCopied
    // (it must be but I'm just too lazy to implement that)
    size_t TargetRestoreSize;

};


