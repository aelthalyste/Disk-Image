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

