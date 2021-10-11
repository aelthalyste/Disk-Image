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
#include "restore.h"
#include "zstd.h"
#include "nar_win32.h"
#include "platform_io.h"
#include "nar.h"
#include "file_explorer.h"
#include "backup.h"
//#include "nar_build_include.cpp"


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
    
#if 1    
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






#include <conio.h>

#if 0
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
#endif



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

void TEST_STRINGS(){
	
	nar_arena Arena = ArenaInit(malloc(1024), 1024);
    
	NarUTF8 MainStr = NARUTF8("RANDOM_FILE_NAME");
    
	auto LongStr = NarUTF8Init(ArenaAllocate(&Arena, 400), 400);
    
	auto Root = NarGetRootPath(MainStr, &Arena);
	NarStringConcatenate(&LongStr, Root);
	NarStringConcatenate(&LongStr, NARUTF8("my other file name"));
	NarStringConcatenate(&LongStr, NARUTF8("\\some other file"));
	NarStringConcatenate(&LongStr, NARUTF8("\\124234"));
	NarStringConcatenate(&LongStr, NARUTF8("\\fasjdflasdklasjfhd"));
	
	printf("%s\n", LongStr.Str);
	return;
}

void TEST_ExtensionFinder(){
    HANDLE Vol = NarOpenVolume('C');
    auto Mem = NarSetupExtensionFinderMemory(Vol);
    wchar_t *_h0 = L".png";
    wchar_t *_h1 = L".exe"; 
    wchar_t *_h2 = L".lib"; 

#if 0
    wchar_t **list[3] = {};
    list[0] = _h0;
    list[1] = _h1;
    list[2] = _h2;
#endif
    wchar_t *list[] = {_h0, _h1, _h2};

    NarFindExtensions('C', Vol, list, 3, &Mem);
}

void
TEST_MockSaveBootState(){
    
    LOG_CONTEXT C = {0};
    C.Port = INVALID_HANDLE_VALUE;
    volume_backup_inf Temp = {};
    Temp.Letter = 'C';
    Temp.Version = 2;
    Temp.BT  = BackupType::Inc;
    Temp.IncLogMark.LastBackupRegionOffset = Kilobyte(30);
    nar_backup_id ID;
    
    ID.Q = 2893749281;
    ID.Letter = 'C';
    
    Temp.BackupID  = ID;
    C.Volumes.Insert(Temp);
    NarSaveBootState(&C);
    
    
}



int
wmain(int argc, wchar_t* argv[]) {
    
    TEST_ExtensionFinder();
    
    return 0;

    printf("222  test1\n");
    printf("222  test2\n");
    printf("222  test3\n");
    printf("222  test4\n");
    printf("222  test5\n");
return 0;
    //TEST_MockSaveBootState();
        
#if 0    
    uint32_t s1, s2;
    auto vh = NarOpenVolume('C');
    auto r1 = OLD_GetVolumeRegionsFromBitmap(vh, &s1);
    auto r2 = GetVolumeRegionsFromBitmap(vh, &s2);
    ASSERT(s1 == s2);
    for(int i =0; i<s1 && i<s2; i++){
        ASSERT(memcmp(&r1[i], &r2[i], 8) == 0);
    }
    printf("Done!\n");
    return 0;
#endif
    
    if(0){
        auto Ctx = SetupFullOnlyStream('C', new DotNetStreamInf, false, true);
        void *tb = malloc(Megabyte(32));
        size_t br = 0;
        while(1){
            auto v = ReadStream(&Ctx.Stream, tb, Megabyte(32));
            br += v;
            if(v == 0){
                NAR_BREAK;
                break;
            }
        }
        TerminateFullOnlyStream(&Ctx, false, 0);
        
    }
    
    
    //TEST_MockSaveBootState();
    
#if 0    
    {
        char Bf[1024];
        memset(Bf, 0, 1024);
        HANDLE Iter = FindFirstVolumeA(Bf, 1024); 
        for(;;){
            printf("%s\n", Bf);
            if(!FindNextVolumeA(Iter,Bf, 1024)){
                break;
            }
        }
        FindVolumeClose(Iter); 
    }
#endif
    
#if 0    
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
#endif
    
#if 0
    
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
    
    file_read EverythingView = NarReadFile(L"C:\\Users\\bcbilisim\\Desktop\\everythingoutput.txt");
    
    char bf[4096*5];
    char* Needle = (char*)(EverythingView.Data);
    char* End    = (char*)EverythingView.Data + EverythingView.Len;
    size_t Count = 0;
    
    wchar_t wcsbf[4096*5];
    
    for(;;){
        memset(bf, 0, sizeof(bf));
        memset(wcsbf, 0, sizeof(wcsbf));
        
        Needle = NarConsumeNextLine(Needle, bf, sizeof(bf), End);
        
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
            
            printf("ENTER LETTER TO DO BACKUP\n");
            std::cin>>Volume;
            
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
                    printf("starting backup\n");
#if 0                    
                    loop{
                        int Read = ReadStream(&v->Stream, MemBuf, bsize);
                        TotalRead += Read;
                        
                        ucs += v->Stream.BytesProcessed;
                        
                        if(Read == 0){
                            break;
                        }
                        else{
                            DWORD BytesWritten = 0;
                            if(WriteFile(file, MemBuf, Read, &BytesWritten, 0) && BytesWritten == Read){
                                TotalWritten += BytesWritten;
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
                        TerminateBackup(v, NAR_SUCC, L"T:\\llvm\\");
                    }
                    else{
                        NAR_BREAK;
                        TerminateBackup(v, NAR_FAILED, L"T:\\llvm\\");
                    }
                    
                    NarSaveBootState(&C);
                }
                else{
                    // NOTE(Batuhan): couldnt create file to save backup
                    NAR_BREAK;
                    int ret = TerminateBackup(v, NAR_FAILED, 0);
                    ret++;// to inspect ret in debugging
                }
                
                CloseHandle(file);
                
            }
            else{
                NAR_BREAK;
                printf("couldnt setup stream\n");
            }
        }
    }
    else{
        NAR_BREAK;
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
