/*
  I do unity builds, so I dont check if declared functions in .h file, I just write it in cpp file and if any build gives me error then
  I add it to .h file.
*/
/*
 *  // TODO(Batuhan): We are double sorting regions, just to append indx and lcn regions. Rather than doing that, remove sort from setupdiffrecords & setupincrecords and do it in the setupstream function, this way we would just append then sort + merge once, not twice(which may be too expensive for regions files > 100M)
*/

#include <DriverSpecs.h>

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

#if 1
#include <strsafe.h>
#include <stdlib.h>
#include <cstdio>
#include "restore.h"
#include "zstd.h"
#include "nar_win32.h"
#include "platform_io.h"
#include "nar.h"
#include "file_explorer.h"
#else
#include "nar_build_include.cpp"
#endif


#include "performance.h"

#define NAR_FAILED 0
#define NAR_SUCC   1

#define loop for(;;)

#include <unordered_map>
#include <conio.h>

int main() {

    {
        file_read FR = NarReadFile("C:\\Users\\User\\Desktop\\NAR_LOG_FILE_C.nlfx");
        nar_record *regs = (nar_record *)FR.Data;
        int32_t RegCount = FR.Len/sizeof(nar_record);

        int64_t qs = NarGetPerfCounter();
        qsort(regs, RegCount, sizeof(nar_record), CompareNarRecords);
        double qelapsed = NarTimeElapsed(qs);

        data_array<nar_record> dr;
        dr.Data = regs;
        dr.Count = RegCount;

        int64_t merges = NarGetPerfCounter();
        MergeRegionsWithoutRealloc(&dr);
        double mergeelapsed = NarTimeElapsed(merges);

        fprintf(stdout, "Region count : %d", RegCount);
        fprintf(stdout, "qsort elapsed : %.4f, merge elapsed %.4f", qelapsed, mergeelapsed);

        return 0;
    }


    backup_package Output[1024];
    {
        int c = NarGetBackupsInDirectory("D:\\", Output, 1024);
        for(int i=0;i<c;++i) {
            fprintf(stdout, "Volume  %c\n", Output[i].BackupInformation.Letter);
            fprintf(stdout, "version %2d\n",  Output[i].BackupInformation.Version);
            fprintf(stdout, "Size of binary data      %10llu (gb -> %10llu)\n", Output[i].BackupInformation.SizeOfBinaryData, Output[i].BackupInformation.SizeOfBinaryData/(1024ull * 1024 * 1024));
            fprintf(stdout, "Original size of volume  %10llu (gb -> %10llu)\n", Output[i].BackupInformation.OriginalSizeOfVolume, Output[i].BackupInformation.SizeOfBinaryData / (1024ull * 1024 * 1024));
            fprintf(stdout, "########\n");
        }
    }

    size_t bsize = 64 * 1024 * 1024;
    void *MemBuf = malloc(bsize);
    
    LOG_CONTEXT C = {0};
    C.Port = INVALID_HANDLE_VALUE;
    
    if(SetupVSS() && ConnectDriver(&C)){
        char Volume = 0;
        int Type = 1;
        
        DotNetStreamInf inf = {0};
        printf("ENTER LETTER TO DO BACKUP\n");
        std::cin>>Volume;
        
        for (int __i=0;
            __i<2;
            ++__i) 
        {
            
            memset(&inf, 0, sizeof(inf));
            
            
            BackupType bt = BackupType::Inc;

            uint64_t TotalRead    = 0;
            uint64_t TotalWritten = 0;
            uint64_t BytesToTransfer = 0;

            char BinaryExtension[256];
            memset(BinaryExtension,0, sizeof(BinaryExtension));

            if(SetupStream(&C, (wchar_t)Volume, bt, &BytesToTransfer, BinaryExtension, NAR_COMPRESSION_ZSTD)){
                
                int id = GetVolumeID(&C, (wchar_t)Volume);
                volume_backup_inf *v = &C.Volumes.Data[id];

                char fn[1024];
                fn[0] = 0;
                snprintf(fn, sizeof(fn), "D:\\tempfile-%d.%s", __i, BinaryExtension);


                HANDLE file = CreateFileA(fn, 
                                  GENERIC_WRITE | GENERIC_READ, 
                                  0, 0, 
                                  CREATE_ALWAYS, 
                                  0, 0);

                char MetadataOutput[1024];
                if (TerminateBackup(v, 1, "D:\\", MetadataOutput)) {
                    printf("Metadata saved as %s", MetadataOutput);
                }
                else {
                    printf("Unable to terminate backup!\n");
                }

                CloseHandle(file);

            }
        }

    }
    else {

    }

    return 0;
}

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


#if 0
int
wmain(int argc, wchar_t* argv[]) {
    
        
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
    
    wchar_t *extensions[] = {L".dll"};
    extension_search_result result = NarFindExtensions('C', NarOpenVolume('C'), extensions, 1, &ExMemory);

    return 0;
    std::unordered_map<std::wstring, int> NarResult;
    
    file_read EverythingView = NarReadFile("C:\\Users\\bcbilisim\\Desktop\\everythingoutput.txt");
    
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
    
    
    
    size_t bsize = 64 * 1024 * 1024;
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
#if 1                    
                    loop{
                        int Read = ReadStream(&v->Stream, MemBuf, (uint32_t)bsize);
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
                        TerminateBackup(v, NAR_SUCC, L"D:\\");
                    }
                    else{
                        NAR_BREAK;
                        TerminateBackup(v, NAR_FAILED, L"D:\\");
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

#endif




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
