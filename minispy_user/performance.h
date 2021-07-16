/* date = July 15th 2021 4:36 pm */

#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#if _WIN32

#define TIMED_BLOCK__(NAME, Number, ...) timed_block timed_##Number(__COUNTER__, __LINE__, __FUNCTION__, NAME);
#define TIMED_BLOCK_(NAME, Number, ...)  TIMED_BLOCK__(NAME, Number,  ## __VA__ARGS__)
#define TIMED_BLOCK(...)                 TIMED_BLOCK_("UNNAMED", __LINE__, ## __VA__ARGS__)
#define TIMED_NAMED_BLOCK(NAME, ...)     TIMED_BLOCK_(NAME, __LINE__, ## __VA__ARGS__)


struct debug_record {
    char* FunctionName;
    char* Description;
    uint64_t Clocks;
    
    uint32_t ThreadIndex;
    uint32_t LineNumber;
    uint32_t HitCount;
};

debug_record GlobalDebugRecordArray[1000];

struct timed_block {
    
    debug_record* mRecord;
    
    timed_block(int Counter, int LineNumber, char* FunctionName, char* Description) {
        mRecord = GlobalDebugRecordArray + Counter;
        mRecord->FunctionName   = FunctionName;
        mRecord->Description = Description;
        mRecord->LineNumber     = LineNumber;
        mRecord->ThreadIndex    = 0;
        mRecord->Clocks        -= __rdtsc();
        mRecord->HitCount++;
        
    }
    
    ~timed_block() {
        mRecord->Clocks += __rdtsc();
    }
    
};



// some debug stuff
int64_t NarGetPerfCounter(){
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li.QuadPart;
}
int64_t NarPerfFrequency(){
    static int64_t cache = 0;
    if(cache == 0){
        LARGE_INTEGER i;
        QueryPerformanceFrequency(&i);
        cache = i.QuadPart;
    }
    return cache;
}

// time elapsed in ms
double NarTimeElapsed(int64_t start){
    return ((double)NarGetPerfCounter() - (double)start)/(double)NarPerfFrequency();
}
#endif



#if __linux__
#error "not implemented"
#endif



#endif //PERFORMANCE_H
