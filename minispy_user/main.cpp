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

#include "minispy.h"

#if 1
#include <strsafe.h>
#include <stdlib.h>
#include <cstdio>
 
#include "nar_compression.hpp"
#include "nar_win32.hpp"
#include "platform_io.hpp"
#include "nar.hpp"
#include "file_explorer.hpp"
#else
#include "nar_build_include.cpp"
#endif


#include "performance.hpp"

#define NAR_FAILED 0
#define NAR_SUCC   1

#define loop for(;;)

#include "nar_extension_searcher.hpp"
#include <unordered_map>
#include <conio.h>




void TestBackup(const char *OutputDirectory, char Volume, int Count) {
    

    // useful when calculating compression ratio
    uint64_t g_TotalBytesTransferred = 0;
    uint64_t g_ActualBytes           = 0; 

    size_t bsize = 64 * 1024 * 1024;
    void *MemBuf = malloc(bsize);
    
    LOG_CONTEXT C;
    C.Port = INVALID_HANDLE_VALUE;

    if (SetupVSS() && ConnectDriver(&C)) {
        BackupType bt = BackupType::Inc;
        AddVolumeToTrack(&C, Volume, bt);

        DotNetStreamInf inf = {0};

        uint64_t BFCAP = Megabyte(16);
        void *bf = malloc(BFCAP);

        for(int _vi=0;_vi<Count;++_vi) {

            uint64_t TotalRead    = 0;
            uint64_t TotalWritten = 0;
            uint64_t BytesToTransfer = 0;

            char BinaryExtension[256];
            memset(BinaryExtension, 0, sizeof(BinaryExtension));

            char BackupIDAsStr[1024];
            memset(BackupIDAsStr,0,sizeof(BackupIDAsStr));
            
            char fn[1024];
            memset(fn,0,sizeof(fn));

            auto r = SetupStream(&C, Volume, bt, &BytesToTransfer, BinaryExtension, NAR_COMPRESSION_NONE);
            ASSERT(r);

            int id = GetVolumeID(&C, (wchar_t)Volume);
            volume_backup_inf *v = &C.Volumes[id];
            
            NarBackupIDToStr(v->BackupID, BackupIDAsStr, sizeof(BackupIDAsStr));

            snprintf(fn, sizeof(fn), "%s\\%s-%d.%s", OutputDirectory, BackupIDAsStr, _vi, BinaryExtension);

            File binary_file = create_file(fn);
            ASSERT(is_file_handle_valid(&binary_file));

            for(;;) {
                uint32_t BytesToWrite = ReadStream(&v->Stream, bf, BFCAP);
                if (BytesToWrite==0)
                    break;

                TotalWritten += BytesToWrite;
                r = write_file(&binary_file, bf, BytesToWrite);
                ASSERT(r);
            }

            if (TotalWritten==0) {
                LOG_ERROR("FATAL ERROR !!!! Couldnt write binary file");
                exit(0);
            }
            
            char MetadataName[1024];
            memset(MetadataName,0,sizeof(MetadataName));
            r = TerminateBackup(v, true, OutputDirectory, MetadataName);
            ASSERT(r);

            g_TotalBytesTransferred += TotalWritten;
            LOG_INFO("Backup %d done, Binary Name %s, Metadata name is %s", fn, MetadataName);
            close_file(&binary_file);
        }

    }

}


void ListChains(const char *Directory) {

    Array<Array<backup_information_ex>> Chains = NarGetChainsInDirectory(Directory);
    for_array (ci, Chains) {
        fprintf(stdout, "####################\n");
        for_array (vi, Chains[ci]) {
            fprintf(stdout, "Metadata path : %s\n", Chains[ci][vi].Path);
            fprintf(stdout, "Letter : %d\n" , Chains[ci][vi].Version);
            fprintf(stdout, "Version : %d\n", Chains[ci][vi].Letter);
            fprintf(stdout, "Binary size : %llu\n", Chains[ci][vi].SizeOfBinaryData);
            fprintf(stdout, "\n");
        }
        fprintf(stdout, "####################\n");
    }
    NarFreeChains(Chains);
}


void DoRestore(const char *Directory, char OutputVolumeLetter) {
    
    Array<Array<backup_information_ex>> Chains = NarGetChainsInDirectory(Directory);
    for_array (ci, Chains) {
        fprintf(stdout, "####################\n");
        fprintf(stdout, "%2llu -) Letter        : %c\n", ci, Chains[ci][0].Letter);
        fprintf(stdout, "%2llu -) Version Count : %lld\n", ci, Chains[ci].len);
        fprintf(stdout, "%2llu -) Backup type   : %s\n", ci, Chains[ci][0].BT == BackupType::Inc ? "Inc" : "Diff");
        fprintf(stdout, "####################\n");
    }

    int32_t ChainIndice;
    int32_t VersionIndice;

    fprintf(stdout, "Please select chain....");
    scanf("%d", &ChainIndice);

    
    fprintf(stdout, "Please select version...");
    scanf("%d", &VersionIndice);

    Restore_Ctx ctx;
    

    bool r = InitRestore(&ctx, Directory, Chains[ChainIndice][VersionIndice].BackupID, VersionIndice);
    nar_binary_files *BinaryFiles = NarGetBinaryFilesInDirectory(Directory, Chains[ChainIndice][VersionIndice].BackupID, VersionIndice);
    ASSERT(BinaryFiles);

    uint64_t BufferCap = 1024 * 1024 * 4;
    void *Buffer = malloc(BufferCap);
    
    HANDLE OutputVolume = NarOpenVolume(OutputVolumeLetter);
    ASSERT(OutputVolume != INVALID_HANDLE_VALUE);

    {
        uint64_t ZeroCount = 0;
        uint64_t NormalCount = 0;
        for(int i=0;i<ctx.instructions.len;++i) {
            if (ctx.instructions[i].instruction_type == NORMAL) {
                BG_ASSERT(ctx.instructions[i].version >= 0);
            }
            if (ctx.instructions[i].instruction_type == ZERO)
                ZeroCount += ctx.instructions[i].where_to_read.len;
            if (ctx.instructions[i].instruction_type == NORMAL)
                NormalCount += ctx.instructions[i].where_to_read.len;
            
            // if (ctx.instructions[i].instruction_type == NORMAL) {
            //     LOG_DEBUG("write : %10lld-%10lld, read : %10lld-%10lld", (uint64_t)ctx.instructions[i].where_to_write.off*4096ull, (uint64_t)ctx.instructions[i].where_to_write.len*4096ull, (uint64_t)ctx.instructions[i].where_to_read.off*4096ull, (uint64_t)ctx.instructions[i].where_to_read.len*4096ull);
            // }
            
        }

        LOG_DEBUG("Zero : %10llu --- Normal : %10llu, per : %.3f", ZeroCount, NormalCount, (double)ZeroCount/(double)(ZeroCount + NormalCount));
        // ASSERT(false);
    }


    for(int _ic=0;;++_ic){
        Restore_Instruction instruction;
        if (AdvanceRestore(&ctx, &instruction)) {
            if (instruction.instruction_type == ZERO)
                continue;

            uint64_t rem=instruction.where_to_read.len;

            {
                LARGE_INTEGER Target;
                LARGE_INTEGER SFPResult;
                Target.QuadPart = (uint64_t)instruction.where_to_write.off;

                bool sfpr = SetFilePointerEx(OutputVolume, Target, &SFPResult, FILE_BEGIN);
                BG_ASSERT(sfpr && SFPResult.QuadPart == Target.QuadPart);
            }

            while(instruction.where_to_read.len) {
                uint64_t ReadSize = BG_MIN(BufferCap, instruction.where_to_read.len); 
                
                if (instruction.instruction_type == NORMAL)  {
                    bool RVR = NarReadVersion(BinaryFiles, instruction.version, Buffer, (uint64_t)instruction.where_to_read.off, ReadSize);
                    BG_ASSERT(instruction.version >= 0);
                    ASSERT(RVR);
                    
                    DWORD Written = 0;
                    if (WriteFile(OutputVolume, Buffer, ReadSize, &Written, 0) && Written == ReadSize) {
                        instruction.where_to_read.len -= ReadSize;
                        instruction.where_to_read.off += ReadSize;

                        // ok
                    }
                    else {
                        LOG_INFO("Error code : %d", GetLastError());
                        LOG_INFO("Written : %lld, ReadSize : %lld, Instruction Offset : %lld, Instruction Len : %lld", Written, ReadSize, instruction.where_to_read.off, instruction.where_to_read.len); 
                        ASSERT(false);
                    }
                } 
                
            }
        }
        else {break;}
    }
    LOG_INFO("DONE!");

    CloseHandle(OutputVolume);
    ASSERT(r);

    NarFreeChains(Chains);

}


int main(int argc, char *argv[]) {

    char Letter = 'C';
    HANDLE vh = NarOpenVolume('C');

    extension_finder_memory Memory = NarSetupExtensionFinderMemory(vh);
    wchar_t *ext[] = {
        L".dll",
        // L".exe"
    };
    extension_search_result Result = NarFindExtensions(Letter, vh, ext, sizeof(ext) / (sizeof(ext[0])), &Memory);
    LOG_INFO("Found %d files : ", Result.Len);

    // for (uint64_t i = 0; i < Result.Len; ++i) {
    //     LOG_INFO("-> %S", Result.Files[i]);
    // }
    return 0;

    if      (0 == strcmp(argv[1], "list"))    ListChains(argv[2]);
    else if (0 == strcmp(argv[1], "backup"))  TestBackup(argv[2], argv[3][0], atoi(argv[4]));
    else if (0 == strcmp(argv[1], "restore")) DoRestore(argv[2], argv[3][0]);
    return 0;


#if 0
    backup_package Output[1024];
    if (0) {
        int c = NarGetBackupsInDirectory("D:\\", Output, 1024);
        for(int i=0;i<c;++i) {
            fprintf(stdout, "Volume  %c\n", Output[i].BackupInformation.Letter);
            fprintf(stdout, "version %2d\n",  Output[i].BackupInformation.Version);
            fprintf(stdout, "Size of binary data      %10llu (gb -> %10llu)\n", Output[i].BackupInformation.SizeOfBinaryData, Output[i].BackupInformation.SizeOfBinaryData/(1024ull * 1024 * 1024));
            fprintf(stdout, "Original size of volume  %10llu (gb -> %10llu)\n", Output[i].BackupInformation.OriginalSizeOfVolume, Output[i].BackupInformation.SizeOfBinaryData / (1024ull * 1024 * 1024));
            fprintf(stdout, "########\n");
        }
    }

    if (0) {
        char Dir[] = "D:\\";
        auto arr = NarGetChainsInDirectory(Dir);
        NarFreeChains(arr);

        nar_binary_files *arr2 = NarGetBinaryFilesInDirectory(Dir, arr[0][0].BackupID, 4);
        NarFreeBinaryFilesInDirectory(arr2);        
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
        
        uint64_t BFCAP = Megabyte(16);
        void *bf = malloc(BFCAP);

        for (int __i=0;
            __i<5;
            ++__i) 
        {
            
            memset(&inf, 0, sizeof(inf));
            BackupType bt = BackupType::Inc;

            uint64_t TotalRead    = 0;
            uint64_t TotalWritten = 0;
            uint64_t BytesToTransfer = 0;

            char BinaryExtension[256];
            memset(BinaryExtension,0, sizeof(BinaryExtension));

            if(SetupStream(&C, (wchar_t)Volume, bt, &BytesToTransfer, BinaryExtension, NAR_COMPRESSION_NONE)){
                
                int id = GetVolumeID(&C, (wchar_t)Volume);
                volume_backup_inf *v = &C.Volumes[id];

                char fn[1024];
                fn[0] = 0;
                snprintf(fn, sizeof(fn), "D:\\tempfile-%d.%s", __i, BinaryExtension);


                File binary_file = create_file(fn);
                BG_ASSERT(is_file_handle_valid(&binary_file));

                v->Stream.RecIndex = v->Stream.RecordCount;
                
                {
                    bool r = write_file(&binary_file, bf, BFCAP);
                    BG_ASSERT(r);
                }


                {
                    uint32_t btw = ReadStream(&v->Stream, bf, BFCAP);
                    BG_ASSERT(btw);

                    bool r = write_file(&binary_file, bf, btw);
                    BG_ASSERT(r);
                }

                char MetadataOutput[1024];
                if (TerminateBackup(v, 1, "D:\\", MetadataOutput)) {
                    printf("Metadata saved as %s", MetadataOutput);
                }
                else {
                    BG_ASSERT(false);
                    printf("Unable to terminate backup!\n");
                }

                close_file(&binary_file);
            }
        }

    }
    else {

    }

    return 0;
#endif
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
