/* date = August 6th 2021 10:35 am */
#pragma once

//#include "memory.h"
#include <stdint.h>
struct nar_arena;

struct NarUTF8{
    uint8_t *Str;
    uint32_t Len;
    uint32_t Cap;
};


#define NARUTF8(ch) NarUTF8{(uint8_t*)ch, sizeof(ch) - 1, 0}



NarUTF8
NarStringCopy(NarUTF8 Input, nar_arena *Arena);

bool
NarStringCopy(NarUTF8 *Destination, NarUTF8 Source);

bool
NarStringConcatenate(NarUTF8 *Destination, NarUTF8 Append);

NarUTF8
NarUTF8Init(void *Memory, uint32_t Len);


NarUTF8
NarGetRootPath(NarUTF8 FileName, nar_arena *Arena);