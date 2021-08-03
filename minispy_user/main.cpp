/*
  I do unity builds, so I dont check if declared functions in .h file, I just write it in cpp file and if any build gives me error then
  I add it to .h file.
*/
/*
 *  // TODO(Batuhan): We are double sorting regions, just to append indx and lcn regions. Rather than doing that, remove sort from setupdiffrecords & setupincrecords and do it in the setupstream function, this way we would just append then sort + merge once, not twice(which may be too expensive for regions files > 100M)
*/

#include <DriverSpecs.h>
_Analysis_mode_(_Analysis_code_type_user_code_)

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#if 1
#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif
#endif

// VDS includes

#include "precompiled.h"

#include "mspyLog.h"
#include "minispy.h"

#include <strsafe.h>
#include <stdlib.h>
#include <cstdio>

#include "zstd.h"
#include "nar_build_include.cpp"

#if 0
inline void* 
narmalloc(size_t len){
    static uint32_t i =0;
    fprintf(stdout, "(%4d)allocating size %4.3f\n", i++, (double)len/(1024.0*1024.0));
    return malloc(len);
}
inline void
narfree(void* m){
    static uint32_t i =0;
    free(m);
}

inline void*
narrealloc(void* m, size_t ns){
    static uint32_t i =0;
    fprintf(stdout, "(%4d)reallocating to size %4.3f\n", i++, (double)ns/(1024.0*1024.0));
    return realloc(m, ns);
}

void*
narvirtualalloc(void* m, size_t len, DWORD f1, DWORD f2){
    static uint32_t i=0;
    fprintf(stdout, "(%4d)valloc size %4.f\n", i++, (double)len/(1024.0*1024.0));
    return VirtualAlloc(m, len, f1, f2);
}

#define malloc       narmalloc
#define free         narfree
#define realloc      narrealloc
#define VirtualAlloc narvirtualalloc
#endif







backup_metadata
ReadMetadata(nar_backup_id ID, int Version, std::wstring RootDir) {
    
    if (RootDir.length() > 0) {
        if (RootDir[RootDir.length() - 1] != L'\\') {
            RootDir += L"\\";
        }
    }
    
    BOOLEAN ErrorOccured = 0;
    DWORD BytesOperated = 0;
    std::wstring FileName; 
    {
        std::wstring Garbage;
        GenerateMetadataName(ID, Version, Garbage);
        FileName = RootDir + Garbage;
    }
    
    backup_metadata BM = { 0 };
    
    HANDLE F = CreateFileW(FileName.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (F != INVALID_HANDLE_VALUE) {
        if (ReadFile(F, &BM, sizeof(BM), &BytesOperated, 0) && BytesOperated == sizeof(BM)) {
            ErrorOccured = FALSE;
        }
        else {
            ErrorOccured = TRUE;
            printf("Unable to read metadata, read %i bytes instead of %i\n", BytesOperated, (uint32_t)sizeof(BM));
            memset(&BM, 0, sizeof(BM));
            memset(&BM.Errors, 1, sizeof(BM.Errors)); // Set all error flags
        }
    }
    else {
        printf("Unable to open metadata file %S\n", FileName.c_str());
        memset(&BM, 0, sizeof(BM));
        memset(&BM.Errors, 1, sizeof(BM.Errors)); // Set all error flags
    }
    
    CloseHandle(F);
    return BM;
}



// NOTE(Batuhan): truncates file F to size TargetSize
BOOLEAN
NarTruncateFile(HANDLE F, ULONGLONG TargetSize) {
    
    LARGE_INTEGER MoveTo = { 0 };
    MoveTo.QuadPart = TargetSize;
    LARGE_INTEGER NewFilePointer = { 0 };
    SetFilePointerEx(F, MoveTo, &NewFilePointer, FILE_BEGIN);
    if (MoveTo.QuadPart == NewFilePointer.QuadPart) {
        return (SetEndOfFile(F) != 0);
    }
    else {
        printf("Couldn't truncate file to %I64d, instead set it's end to %I64d\n", TargetSize, NewFilePointer.QuadPart);
    }
    
    return FALSE;
    
}







#if 1


// Path: any path name that contains trailing backslash
// like = C:\\somedir\\thatfile.exe    or    \\relativedirectory\\dir2\\dir3\\ourfile.exe
// OutName: output buffer
// OutMaxLen: in characters
inline void
NarGetFileNameFromPath(const wchar_t* Path, wchar_t *OutName, INT32 OutMaxLen) {
    
    if (!Path || !OutName) return;
    
    memset(OutName, 0, OutMaxLen);
    
    size_t PathLen = wcslen(Path);
    
    if (PathLen > OutMaxLen) return;
    
    int TrimPoint = PathLen - 1;
    {
        while (Path[TrimPoint] != L'\\' && --TrimPoint != 0);
    }
    
    if (TrimPoint < 0) TrimPoint = 0;
    
    memcpy(OutName, &Path[TrimPoint], 2 * (PathLen - TrimPoint));
    
}


/*
path = path to the file, may be relative or absolute
Out, user given buffer to write filename with extension
Maxout, max character can be written to Out
*/
inline void
NarGetFileNameFromPath(const wchar_t *path, wchar_t* Out, size_t MaxOut){
    size_t pl = wcslen(path);
    
    size_t it = pl - 1;
    for(; path[it] != L'\\' && it > 0; it--);
    
    // NOTE(Batuhan): cant fit given buffer, 
    if(pl - it > MaxOut) Out[0] = 0;
    else{
        wcscpy(Out, &path[it]);
    }
}


#endif





#if 1


#endif


struct{
    uint64_t LastCycleCount;
    uint64_t WorkCycleCount;
    LARGE_INTEGER LastCounter;
    LARGE_INTEGER WorkCounter;
    INT64 GlobalPerfCountFrequency;
}NarPerfCounter;


inline float
GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End){
    float Result = (float)(End.QuadPart - Start.QuadPart)/(float)(NarPerfCounter.GlobalPerfCountFrequency);
    return Result;
}

inline LARGE_INTEGER
GetClock(){
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return Result;
}

inline void
NarInitPerfCounter(){
    NarPerfCounter.LastCycleCount = 0;
    NarPerfCounter.WorkCycleCount = 0;
    NarPerfCounter.LastCounter = {0};
    NarPerfCounter.WorkCounter = {0};
    
    LARGE_INTEGER PerfCountFreqResult;
    QueryPerformanceFrequency(&PerfCountFreqResult);
    NarPerfCounter.GlobalPerfCountFrequency = PerfCountFreqResult.QuadPart;
}

inline void
NarStartPerfCounter(){
    NarPerfCounter.LastCycleCount = __rdtsc();
    NarPerfCounter.LastCounter = GetClock();
}

inline void
NarEndPerfCounter(){
    NarPerfCounter.WorkCycleCount = __rdtsc();
    NarPerfCounter.WorkCounter = GetClock();
}


inline BOOLEAN
Test_NarGetRegionIntersection() {
    
    BOOLEAN Result = FALSE;
    
    nar_record r1[16], r2[16];
    r1[0] = { 0, 200 };
    r1[1] = { 300, 200 };
    r1[2] = { 800, 100 };
    r1[3] = { 1200, 800 };
    r1[4] = { 2100, 200 };
    
    r2[0] = { 80, 70 };
    r2[1] = { 250, 100 };
    r2[2] = { 700, 50 };
    r2[3] = { 1100, 300 };
    r2[4] = { 1500, 50 };
    r2[5] = { 1600, 800 };
    
    INT32 found = 0;
    nar_record* r3 = 0;
    NarGetRegionIntersection(r1, r2, &r3, 5, 6, &found);
    for (int i = 0; i < found; i++) {
        printf("%i\t%i\n", r3[i].StartPos, r3[i].Len);
    }
    NarFreeRegionIntersection(r3);
    
    for (int i = 0; i < found; i++) {
        printf("%i\t%i\n", r3[i].StartPos, r3[i].Len);
    }
    
    
    return Result;
}






#define NAR_FAILED 0
#define NAR_SUCC   1

#define loop for(;;)

void DEBUG_Restore(){
    
    size_t MemLen = Megabyte(512);
    std::wstring MetadataPath = L"";
    int DiskID;
    wchar_t TargetLetter;
    
    std::cout<<"Enter metadata path\n";
    std::wcin>>MetadataPath;
    
#if 0    
    std::cout<<"Enter disk id\n";
    std::cin>>DiskID;
#endif
    
    std::cout<<"Enter target letter\n";
    std::wcin>>TargetLetter;
    
    backup_metadata bm;
    NarReadMetadata(MetadataPath, &bm);
    
    if(false == NarReadMetadata(MetadataPath, &bm)){
        std::wcout<<L"Unable to read metadata "<<MetadataPath<<"\n";
        exit(0);
    }
    
    void* Mem = VirtualAlloc(0, MemLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    nar_arena Arena = ArenaInit(Mem, MemLen);
    
#if 0    
    if (NarSetVolumeSize(TargetLetter, bm.VolumeTotalSize/(1024*1024))) {
        NarFormatVolume(TargetLetter);
    }
    else {
        
    }
#endif
    
    restore_target *Target = InitVolumeTarget(TargetLetter, &Arena);
    restore_stream *Stream = 0;
    if(Target){
        Stream = InitFileRestoreStream(MetadataPath, Target, &Arena, Megabyte(16));
    }
    
    while(AdvanceStream(Stream)){
        int hold_me_here = 5;
        if(Stream->Error != RestoreStream_Errors::Error_NoError
           || Stream->SrcError != RestoreSource_Errors::Error_NoError){
            NAR_BREAK;
            break;
        }
    }
    
    std::cout<<"Done !\n";
    FreeRestoreStream(Stream);
    
}



#include <unordered_map>

char* 
ConsumeNextLine(char *Input, char* Out, size_t MaxBf, char* InpEnd){
    size_t i =0;
    char *Result = 0;
    for(i =0; Input[i] != '\r' && Input + i < InpEnd; i++);
    if(i<MaxBf){
        memcpy(Out, Input, i);
        Out[i] = 0;
        if(Input + (i + 2) < InpEnd){
            Result = &Input[i + 2];
        }
    }
    return Result;
}





#include <conio.h>

typedef struct { uint64_t state;  uint64_t inc; } pcg32_random_t;

uint32_t pcg32_random_r(pcg32_random_t* rng)
{
    uint64_t oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
    // Calculate output function (XSH RR), uses old state for max ILP
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

int
TEST_ReadBackupCrossed(wchar_t *cb, wchar_t *cm, wchar_t* db, wchar_t* dm, pcg32_random_t *state){
    
    nar_file_view CompressedBackupView = NarOpenFileView(cb);
    nar_file_view CompressedMetadataView = NarOpenFileView(cm);
    
    nar_file_view DecompressedBackupView = NarOpenFileView(db);
    nar_file_view DecompressedMetadataView = NarOpenFileView(dm);
    
    ASSERT(CompressedMetadataView.Data);
    ASSERT(DecompressedMetadataView.Data);
    ASSERT(CompressedBackupView.Data);
    ASSERT(DecompressedBackupView.Data);
    
    
    backup_metadata *BM = (backup_metadata*)CompressedMetadataView.Data;
    
    nar_record* Records  = (nar_record*)((uint8_t*)CompressedMetadataView.Data + BM->Offset.RegionsMetadata);
    uint64_t RecordCount = BM->Size.RegionsMetadata/sizeof(nar_record);
    
    uint64_t SelectedIndice  = 2;//pcg32_random_r(state) % RecordCount;
    uint64_t ClusterReadSize = 310910;//pcg32_random_r(state) % Records[SelectedIndice].Len;
    
    nar_arena Arena = ArenaInit(malloc(ClusterReadSize*4096*4), ClusterReadSize*4096*4);
    
    void* CompressedBuffer   = ArenaAllocateAligned(&Arena, ClusterReadSize*4096, 16);
    void* DecompressedBuffer = ArenaAllocateAligned(&Arena, ClusterReadSize*4096, 16);
    
    
    int CReadSize = NarReadBackup(&CompressedBackupView, &CompressedMetadataView, 
                                  Records[SelectedIndice].StartPos, ClusterReadSize, 
                                  CompressedBuffer, ClusterReadSize*4096, 
                                  0, 0);
    
    int DReadSize = NarReadBackup(&DecompressedBackupView, &DecompressedMetadataView, 
                                  Records[SelectedIndice].StartPos, ClusterReadSize, DecompressedBuffer, ClusterReadSize*4096, 
                                  0, 0);
    
    
    ASSERT(DReadSize == ClusterReadSize*4096);
    ASSERT(CReadSize == ClusterReadSize*4096);
    ASSERT(CReadSize == DReadSize);
    
    ASSERT(memcmp(CompressedBuffer, DecompressedBuffer, ClusterReadSize*4096) == 0);
    
    free(Arena.Memory);
    return 0;
}



int
TEST_ReadBackup(wchar_t *backup, wchar_t *metadata, pcg32_random_t *state){
    nar_file_view BView = NarOpenFileView(backup);
    nar_file_view MView = NarOpenFileView(metadata);
    
    
    backup_metadata *BM = (backup_metadata*)MView.Data;
    
    nar_record* Records  = (nar_record*)((uint8_t*)MView.Data + BM->Offset.RegionsMetadata);
    uint64_t RecordCount = BM->Size.RegionsMetadata/sizeof(nar_record);
    
    uint64_t SelectedIndice = 2;//pcg32_random_r(state) % RecordCount;
    uint64_t SelectedLen    = 500;//pcg32_random_r(state) % Records[SelectedIndice].Len;
    
    
    void* FEBuffer = malloc(SelectedLen*4096);
    void* RFBuffer = malloc(SelectedLen*4096);
    
    uint64_t FEResult = NarReadBackup(&BView, &MView, Records[SelectedIndice].StartPos, SelectedLen, FEBuffer, SelectedLen, 0, 0);
    ASSERT(FEResult == SelectedLen*4096);
    
    HANDLE VolumeHandle = NarOpenVolume('D');
    NarSetFilePointer(VolumeHandle, (uint64_t)Records[SelectedIndice].StartPos*4096ull);
    
    DWORD BR = 0;
    ReadFile(VolumeHandle, 
             RFBuffer,
             SelectedLen*4096ull,
             &BR,
             0);
    
    uint64_t UnmatchedCount = 0;
    uint64_t TotalCount = SelectedLen*4096ull/8ull;
    for(uint64_t i =0; i<SelectedLen; i++){
        uint8_t *TRFBuffer = (uint8_t*)RFBuffer + 4096*i;
        uint8_t *TFEBuffer = (uint8_t*)FEBuffer + 4096*i;
        int32_t BCount = 0;
        for(int j = 0; j<4096; j++){
            if(*(TRFBuffer + j) == *(TFEBuffer + j)){
                
            }
            else{
                BCount++;
            }
        }
        
        //ASSERT(BCount != 4096);
    }
    
    ASSERT((float)UnmatchedCount/TotalCount < 0.21f);
    printf("%.4f\n", (float)UnmatchedCount/TotalCount);
    
    free(RFBuffer);
    free(FEBuffer);
    NarFreeFileView(BView);
    NarFreeFileView(MView);
    return 0;
}

bool
TEST_LCNTOVCN(){
    
    nar_record R1[] = {
        {0, 100},
        {400, 100},
        {550, 50},
        {1000, 800},
        {2000, 400},
        {2450, 40},
        {2500, 40},
        {2600, 400}
    };
    
    size_t Offset = 580;
    size_t Result = NarLCNToVCN(R1, sizeof(R1)/8, Offset);
    int hold_m_here = 23423;
    
    return 0;
}

bool
TEST_RegionCoupleIter(){
    
    nar_record R1[] = {
        {0, 100},
        {400, 100},
        {550, 50},
        {1000, 800},
        {2000, 400},
        {2450, 40},
        {2500, 40},
        {2600, 400}
    };
    
    nar_record R2[] = {
        {90, 20},
        {340, 120},
        {490, 10},
        {900, 400},
        {2100, 100},
        {2430, 300}
    };
    
    for(
        RegionCoupleIter Iter = NarStartIntersectionIter(R1, R2, sizeof(R1)/8, sizeof(R2)/8);
        NarIsRegionIterValid(Iter);
        NarNextIntersectionIter(&Iter)){
        printf("%4d\t%4d\n", Iter.It.StartPos, Iter.It.Len);
    }
    printf("\n\n######\n\n");
    
    for(
        RegionCoupleIter Iter = NarStartExcludeIter(R1, R2, sizeof(R1)/8, sizeof(R2)/8);
        NarIsRegionIterValid(Iter);
        NarNextExcludeIter(&Iter)){
        printf("%4d\t%4d\n", Iter.It.StartPos, Iter.It.Len);
    }
    
    
    return true;
}



int
wmain(int argc, wchar_t* argv[]) {
    //TEST_LCNTOVCN();
    
    {
        nar_backup_id ID = {};
        ID.Q = 123442343;
        ID.Letter = 'C';
        
        auto Ctx = NarSetupVSSListen(ID);
        wchar_t VSSPath[512];
        
        if(NarGetVSSPath(&Ctx, VSSPath)){
            fprintf(stdout, "VSS PATH :%S\n", VSSPath);
            NarTerminateVSS(&Ctx, 1);
            NarFreeProcessListen(&Ctx);
        }
        else{
            fprintf(stderr, "Unable to get vss path\n");
        }
    }
    
#if 1
    
    nar_arena Arena = ArenaInit(malloc(1024*1024*512), 1024*1024*512);
    
    NarUTF8 CompletePath = NARUTF8("testfile.txt");
    NarUTF8 Root2 = NarGetRootPath(CompletePath, &Arena);
    
    
    file_explorer FE = NarInitFileExplorer(NARUTF8("G:\\NB_M_2-C07291017.nbfsm"));
    
    file_explorer_file *Target = FEFindFileWithID(&FE, 76831);
    
    //printf("%S\n", FEGetFileFullPath(&FE, Target));
    file_explorer_file *R = FEStartParentSearch(&FE, 5);
    
    nar_backup_id ID = {};
    ID.Q        = 18877558573959141;
    int Version = 2;
    
    
    NarUTF8 Root;
    Root.Str = (uint8_t*)"G:\\";
    Root.Len = 3;
    Root.Cap = 0;
    
    file_restore_ctx Ctx= NarInitFileRestoreCtx(&FE, Target, &Arena);
    size_t FCMSize = Megabyte(2);
    void* FCM = ArenaAllocateAligned(&Arena, FCMSize, 8);
    
    HANDLE File = CreateFileA("output", GENERIC_WRITE | GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0);
    ASSERT(File != INVALID_HANDLE_VALUE);
    for(;;){
        auto Result = NarAdvanceFileRestore(&Ctx, FCM, FCMSize);
        if(Result.Len == 0){
            break;
        }
        DWORD R = SetFilePointer(File, Result.Offset, 0, FILE_BEGIN);
        DWORD BytesWritten = 0;
        
        if(!WriteFile(File, FCM, Result.Len, &BytesWritten, 0) || BytesWritten!=Result.Len){
            ASSERT(FALSE);
            printf("Error code %X\n", GetLastError());
        }
        else{
            printf("%8u\t%8u  MAIN\n", Result.Offset/4096, Result.Len/4096);
        }
        
    }
    
    CloseHandle(File);
    NAR_BREAK;
    return 0;
    
#if 0    
    while(1){
        uint32_t FileFound = 0;
        for(file_explorer_file *File = FEStartParentSearch(&FE, UserGivenID);
            File != 0;
            File = FENextFileInDir(&FE, File)){
            printf("%4u %.20S %I64u\n", File->FileID, File->Name, File->Size);
            FileFound++;
        }
        
        printf("\nFound %u files\n", FileFound);
        
        scanf("%u \n", &UserGivenID);
        printf("\n\n#############\n\n");
    }
#endif
    
    
#endif
    
    
#if 0    
    if(SetupVSS())    
    {
        CComPtr<IVssBackupComponents> VSSPTR;
        wchar_t ShadowPath[1024];
        auto SnapshotID = GetShadowPath(argv[1], VSSPTR, ShadowPath, 1024);
        HANDLE Volume = INVALID_HANDLE_VALUE;
        if(ShadowPath != 0){
            Volume = CreateFileW(ShadowPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
            
            fprintf(stdout, "Press a key to STOP  VSS and do cleanup\n");
            int randomvalue = 0;
            scanf("%s", &randomvalue);
            
            LONG Deleted=0;
            VSS_ID NonDeleted;
            HRESULT hr;
            CComPtr<IVssAsync> async;
            hr = VSSPTR->BackupComplete(&async);
            if(hr == S_OK){
                async->Wait();
                hr = VSSPTR->DeleteSnapshots(SnapshotID, VSS_OBJECT_SNAPSHOT, TRUE, &Deleted, &NonDeleted);
                if(hr != S_OK){
                    fprintf(stderr, "Delete snapshots FAILED\n");
                }
            }
            else{
                fprintf(stderr, "Backup complete failed\n");
            }
            
            VSSPTR.Release();
            
            fprintf(stdout, "Success!\n");        
        }
        else{
            fprintf(stderr, "Unable to open volume C's shadow path\n");
        }
        
    }
    {
        fprintf(stderr, "Unable to set COM settings and privilages\n");
    }
    return 0;
#endif
    
#if 0 
    
    HANDLE VolumeHandle = NarOpenVolume('C');
    extension_finder_memory ExMemory = NarSetupExtensionFinderMemory(VolumeHandle);
    
    extension_search_result result = NarFindExtensions('C', NarOpenVolume('C'), L".png", &ExMemory);
    
    return 0;
    std::unordered_map<std::wstring, int> NarResult;
#if 0    
    for(size_t i =0; i<result.Len; i++){
        NarResult[result.Files[i]] = 0;
    }
#endif
    
    file_read EverythingView = NarReadFile(L"C:\\Users\\bcbilisim\\Desktop\\everythingoutput.txt");
    
    char bf[4096*5];
    char* Needle = (char*)(EverythingView.Data);
    char* End    = (char*)EverythingView.Data + EverythingView.Len;
    size_t Count = 0;
    
    wchar_t wcsbf[4096*5];
    
    for(;;){
        memset(bf, 0, sizeof(bf));
        memset(wcsbf, 0, sizeof(wcsbf));
        
        Needle = ConsumeNextLine(Needle, bf, sizeof(bf), End);
        
        mbstowcs(wcsbf, bf, sizeof(wcsbf)/2);
        if(NarResult.find(wcsbf) == NarResult.end()){
            printf("%S\n", wcsbf);
        }
        else{
            NarResult[wcsbf]++;
        }
        
        Count++;
        if(NULL == Needle)
            break;
        
    }
    
    for (auto& it: NarResult) {
        if(it.second != 1){
            printf("%S\n", it.first.c_str());
        }
    }
    
    return 0;
#endif
    
    
#if 0
    {
        LOG_CONTEXT C = {0};
        C.Port = INVALID_HANDLE_VALUE;
        ConnectDriver(&C);
        AddVolumeToTrack(&C, argv[1][0], BackupType::Diff);
        NarSaveBootState(&C);
    }
    
    return 0;
#endif
    
    size_t bsize = 64*1024*1024;
    void *MemBuf = malloc(bsize);
    
    LOG_CONTEXT C = {0};
    C.Port = INVALID_HANDLE_VALUE;
    
    if(SetupVSS() && ConnectDriver(&C)){
        DotNetStreamInf inf = {0};
        
        char Volume = 0;
        int Type = 1;
        loop{
            
            memset(&inf, 0, sizeof(inf));
            
            printf("ENTER LETTER TO DO BACKUP \n");
            scanf("%c", &Volume);
            
            BackupType bt = BackupType::Inc;
            
            if(SetupStream(&C, (wchar_t)Volume, bt, &inf, false)){
                
                int id = GetVolumeID(&C, (wchar_t)Volume);
                volume_backup_inf *v = &C.Volumes.Data[id];
                size_t TotalRead    = 0;
                size_t TotalWritten = 0;
                size_t TargetWrite  = (size_t)inf.ClusterSize * (size_t)inf.ClusterCount;
                
                printf("filename : %S\n", inf.FileName.c_str());
                
                HANDLE file = CreateFileW(inf.FileName.c_str(), 
                                          GENERIC_WRITE | GENERIC_READ, 
                                          0, 0, 
                                          CREATE_ALWAYS, 
                                          0, 0);
                if(file != INVALID_HANDLE_VALUE){
                    uint64_t ucs = 0;
                    
#if 1                    
                    loop{
                        int Read = ReadStream(v, MemBuf, bsize);
                        TotalRead += Read;
                        
                        ucs += v->Stream.BytesProcessed;
                        
                        if(Read == 0){
                            break;
                        }
                        else{
                            DWORD BytesWritten = 0;
                            if(WriteFile(file, MemBuf, Read, &BytesWritten, 0) && BytesWritten == Read){
                                TotalWritten += BytesWritten;
                                // NOTE(Batuhan): copied successfully
                                //FlushFileBuffers(file);
                            }
                            else{
                                NAR_BREAK;
                                printf("Couldnt write to file\n");
                            }
                            ASSERT(BytesWritten == Read);
                        }
                    }
#else
                    
                    TotalRead = TargetWrite;
                    TotalWritten = TargetWrite;
#endif
                    
                    
                    if(CheckStreamCompletedSuccessfully(v)){
                        TerminateBackup(v, NAR_SUCC);
                    }
                    else{
                        NAR_BREAK;
                        TerminateBackup(v, NAR_FAILED);
                    }
                    
                    NarSaveBootState(&C);
                }
                else{
                    // NOTE(Batuhan): couldnt create file to save backup
                    NAR_BREAK_CODE();
                    int ret = TerminateBackup(v, NAR_FAILED);
                    ret++;// to inspect ret in debugging
                }
                
                CloseHandle(file);
                
            }
            else{
                NAR_BREAK_CODE();
                printf("couldnt setup stream\n");
            }
        }
    }
    else{
        NAR_BREAK_CODE();
    }
    
    return 0;
    
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);
    
    return 0;
}





//debug_record GlobalDebugRecordArray[__COUNTER__];

inline void
PrintDebugRecords() {
    
#if 0    
    int len = sizeof(GlobalDebugRecordArray)/sizeof(GlobalDebugRecordArray[0]);
    for (int i = 0; i < len; i++) {
        if(GlobalDebugRecordArray[i].FunctionName == 0){
            continue;
        }
        printf("FunctionName: %s\tDescription: %s\tAVGclocks: %.3f\tat line %i\tHit count %i\n", GlobalDebugRecordArray[i].FunctionName, GlobalDebugRecordArray[i].Description, (double)GlobalDebugRecordArray[i].Clocks / GlobalDebugRecordArray[i].HitCount, GlobalDebugRecordArray[i].LineNumber, GlobalDebugRecordArray[i].HitCount);
        
    }
#endif
    
}
