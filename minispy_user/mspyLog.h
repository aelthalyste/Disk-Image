#pragma once

#include "precompiled.h"

inline void
PrintDebugRecords();

#define __NAR_MEM_OP_ALLOCATE 1
#define __NAR_MEM_OP_FREE     2
#define __NAR_MEM_OP_ZERO     3
#define __NAR_MEM_OP_RESET    4

static inline void*
_NarInternalMemoryOp(int OpCode, size_t Size) {
    
    struct {
        void* Memory;
        size_t Reserve;
        size_t Used;
    }static MemArena = { 0 };
    
    void* Result = 0;
    
    if (!MemArena.Memory) {
        MemArena.Reserve = 1024LL * 1024LL * 64LL; // Reserve 64GB
        MemArena.Used = 0;
        MemArena.Memory = VirtualAlloc(0, MemArena.Reserve, MEM_RESERVE, PAGE_READWRITE);
    }
    if (OpCode == __NAR_MEM_OP_ALLOCATE) {
        VirtualAlloc(MemArena.Memory, Size + MemArena.Used, MEM_COMMIT, PAGE_READWRITE);
        Result = (void*)((char*)MemArena.Memory + MemArena.Used);
        MemArena.Used += Size;
    }
    if (OpCode == __NAR_MEM_OP_RESET) {
        memset(MemArena.Memory, 0, MemArena.Used);
        if (VirtualFree(MemArena.Memory, MemArena.Used, MEM_DECOMMIT) == 0) {
            printf("Cant decommit scratch memory\n");
        }
        MemArena.Used = 0;
    }
    if(OpCode == __NAR_MEM_OP_FREE){
        VirtualFree(MemArena.Memory, 0, MEM_RELEASE);
        MemArena.Reserve = 0;
        MemArena.Used    = 0;
        MemArena.Memory  = 0;
    }
    
    return Result;
}

#if 0
// some debug stuff
int64_t NarGetPerfCounter();
int64_t NarGetPerfFrequency();

// time elapsed in ms
double NarTimeElapsed(int64_t start);
#endif

static inline void*
NarScratchAllocate(size_t Size) {
    return _NarInternalMemoryOp(__NAR_MEM_OP_ALLOCATE, Size);
}

static inline void
NarScratchReset(){
    _NarInternalMemoryOp(__NAR_MEM_OP_RESET, 0);
}

static inline void
NarScratchFree() {
    _NarInternalMemoryOp(__NAR_MEM_OP_FREE, 0);
}

struct nar_log_time{
    BYTE YEAR; // 2000 + YEAR is the actual value
    BYTE MONTH;
    BYTE DAY;
    BYTE HOUR;
    BYTE MIN;
    BYTE SEC;
    // 6 bytes
};

struct nar_log{
    //char         *FunctionName;
    //unsigned int LineNumber;
    nar_log_time Time;
    char *LogString;
}static GlobalLogs[512];
static int GlobalLogCount = 0;
static HANDLE GlobalLogMutex = 0;


/*

*/
static void
NarLog(const char *str, ...){
    
    static BOOLEAN Init = FALSE;
    if (Init == FALSE) {
        GlobalLogMutex = CreateMutexA(NULL, FALSE, NULL);
        Init = TRUE;
    }
    
    va_list ap;
    
#define MAX_BUF_LEN 1024*2
    
    char buf[MAX_BUF_LEN];
    SYSTEMTIME Time = { 0 };
    nar_log_time NarTime = {0};
    
    memset(buf, 0, sizeof(buf));
    GetLocalTime(&Time);
    
    NarTime.YEAR  = (BYTE)(Time.wYear % 100);
    NarTime.MONTH = (BYTE)Time.wMonth;
    NarTime.DAY   = (BYTE)Time.wDay;
    NarTime.HOUR  = (BYTE)Time.wHour;
    NarTime.MIN   = (BYTE)Time.wMinute;
    NarTime.SEC   = (BYTE)Time.wSecond;
	
    
#if 1
    va_start(ap, str);
    vsprintf(buf, str, ap);
    va_end(ap);
#endif
    
    // safe cast
    DWORD Len = (DWORD)strlen(buf);
    
#if 1    
    if(WaitForSingleObject(GlobalLogMutex, 25) == WAIT_OBJECT_0){
        GlobalLogs[GlobalLogCount].LogString = (char*)NarScratchAllocate((Len + 1)*sizeof(buf[0]));
        memcpy(GlobalLogs[GlobalLogCount].LogString, &buf[0], (Len + 1)*sizeof(buf[0]));
        memcpy(&GlobalLogs[GlobalLogCount].Time, &NarTime, sizeof(NarTime));
        GlobalLogCount++;
        ReleaseMutex(GlobalLogMutex);
    }
#endif
    
#if 1    
	static bool fileinit = false;
	static FILE *File = 0;
	if(fileinit == false){
		File = fopen("C:\\ProgramData\\NarDiskBackup\\NAR_APP_LOG_FILE.txt", "a");
		//File = fopen("NAR_APP_LOG_FILE.txt", "a");
        fileinit = true;
	}	
	if(File){
	    static char time_buf[80];
	    snprintf(time_buf, 80, "[%02d/%02d/%04d | %02d:%02d:%02d] : ", NarTime.DAY, NarTime.MONTH, 2000 + NarTime.YEAR, NarTime.HOUR, NarTime.MIN, NarTime.SEC);
        char big_buffer[1024];
        big_buffer[0] = 0;
        strcat(big_buffer, time_buf);
        strcat(big_buffer, buf);
        fwrite(big_buffer, 1, strlen(big_buffer), File);		
		fflush(File);
        printf(buf);
    }
	else{
		OutputDebugStringA(buf);
        printf(buf);
    }
#endif
    
    
#undef MAX_BUF_LEN
    
}

#define printf(fmt, ...) NarLog(fmt, __VA_ARGS__)



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


//#define printf(format,...) LogFile((format),__VA_ARGS__)

#if 0
BOOLEAN
NarGetBackupsInDirectory(const wchar_t* Directory, backup_metadata* B, int BufferSize, int* FoundCount);
#endif
