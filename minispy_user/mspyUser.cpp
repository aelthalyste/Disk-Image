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

#if 0
#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif
#endif


#include <stdlib.h>
#include <stdio.h>
#include <cstdio>
#include <windows.h>
#include <assert.h>
#include <strsafe.h>
#include <functional>
#include <winioctl.h>
#include <emmintrin.h>

#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <vsmgmt.h>
#include <wchar.h>
#include <string>

#include "iphlpapi.h"

// VDS includes
#include <vds.h>

// ATL includes
#include <atlbase.h>
#include <vector>
#include <string>

#include <fstream>
#include <streambuf>
#include <sstream>

#include "nar.h"
#include "minispy.h"
#include "mspyLog.h"

#if 1
#include "platform_io.cpp"
#include "file_explorer.cpp"
#include "restore.cpp"
#endif


#if 0
inline void* 
narmalloc(size_t len){
    TIMED_BLOCK();
    return malloc(len);
}
inline void
narfree(void* m){
    TIMED_BLOCK();
    free(m);
}
#define malloc narmalloc
#define free narfree

#endif

unsigned int GlobalReadFileCallCount = 0;


/*
    ASSUMES RECORDS ARE SORTED
THIS FUNCTION REALLOCATES MEMORY VIA realloc(), DO NOT PUT MEMORY OTHER THAN ALLOCATED BY MALLOC, OTHERWISE IT WILL CRASH THE PROGRAM
*/
inline void
MergeRegions(data_array<nar_record>* R) {
    
    TIMED_BLOCK();
    
    uint32_t MergedRecordsIndex = 0;
    uint32_t CurrentIter = 0;
    
    for (;;) {
        if (CurrentIter >= R->Count) {
            break;
        }
        
        uint32_t EndPointTemp = R->Data[CurrentIter].StartPos + R->Data[CurrentIter].Len;
        
        if (IsRegionsCollide(R->Data[MergedRecordsIndex], R->Data[CurrentIter])) {
            uint32_t EP1 = R->Data[CurrentIter].StartPos + R->Data[CurrentIter].Len;
            uint32_t EP2 = R->Data[MergedRecordsIndex].StartPos + R->Data[MergedRecordsIndex].Len;
            
            EndPointTemp = MAX(EP1, EP2);
            R->Data[MergedRecordsIndex].Len = EndPointTemp - R->Data[MergedRecordsIndex].StartPos;
            
            CurrentIter++;
        }
        else {
            MergedRecordsIndex++;
            R->Data[MergedRecordsIndex] = R->Data[CurrentIter];
        }
        
        
    }
    
    R->Count = MergedRecordsIndex + 1;
    R->Data = (nar_record*)realloc(R->Data, sizeof(nar_record) * R->Count);
    
}

BOOLEAN
CopyData(HANDLE S, HANDLE D, ULONGLONG Len) {
    BOOLEAN Return = TRUE;
    
    uint32_t BufSize = 8*1024*1024; //8 MB
    ULONGLONG TotalCopied = 0;
    
    void* Buffer = malloc(BufSize);
    
    if (Buffer != NULL) {
        ULONGLONG BytesRemaining = Len;
        DWORD BytesOperated = 0;
        if (BytesRemaining > BufSize) {
            
            while (BytesRemaining > BufSize) {
                if (ReadFile(S, Buffer, BufSize, &BytesOperated, 0)) {
                    BOOL bResult = WriteFile(D, Buffer, BufSize, &BytesOperated, 0);
                    if (!bResult || BytesOperated != BufSize) {
                        printf("COPY_DATA: Writefile failed with code %X\n", bResult);
                        printf("Tried to write -> %I64d, Bytes written -> %d\n", Len, BytesOperated);
                        DisplayError(GetLastError());
                        Return = FALSE;
                        break;
                    }
                    else {
                        TotalCopied += BufSize;
                    }
                    
                    BytesRemaining -= BufSize;
                }
                else {
                    Return = FALSE;
                    //if readfile failed
                    break;
                }
                
            }
        }
        
        if (BytesRemaining > 0) {
            if (ReadFile(S, Buffer, (DWORD)BytesRemaining, &BytesOperated, 0) && BytesOperated == BytesRemaining) {
                
                if (!WriteFile(D, Buffer, (DWORD)BytesRemaining, &BytesOperated, 0) || BytesOperated != BytesRemaining) {
                    printf("COPY_DATA: Error occured while copying end of buffer\n");
                    printf("Bytes written -> %lu , instead of %I64u \n", BytesOperated, BytesRemaining);
                    DisplayError(GetLastError());
                    Return = FALSE;
                }
                else {
                    TotalCopied += BytesRemaining;
                }
                
            }
            
        }
        
        
    }//If Buffer != NULL
    else {
        printf("Can't allocate memory for buffer\n");
        Return = FALSE;
    }
    
    free(Buffer);
    if (Return == FALSE) {
        printf("COPYFILE error detected, copied %I64d bytes instead of %I64d\n", TotalCopied, Len);
    }
    return Return;
}






#pragma warning(push)
#pragma warning(disable:4477)

inline void
StrToGUID(const char* guid, GUID* G) {
    if (!G) return;
    sscanf(guid, "{%8X-%4hX-%4hX-%2hX%2hX-%2hX%2hX%2hX%2hX%2hX%2hX}", &G->Data1, &G->Data2, &G->Data3, &G->Data4[0], &G->Data4[1], &G->Data4[2], &G->Data4[3], &G->Data4[4], &G->Data4[5], &G->Data4[6], &G->Data4[7]);
}
#pragma warning(pop)


inline BOOLEAN
InitVolumeInf(volume_backup_inf* VolInf, wchar_t Letter, BackupType Type) {
    
    if(VolInf == NULL) return FALSE;
    *VolInf = {0};
    
    BOOLEAN Return = FALSE;
    
    VolInf->Letter = Letter;
    VolInf->FilterFlags.IsActive = FALSE;
    VolInf->IsOSVolume = FALSE;
    VolInf->INVALIDATEDENTRY = FALSE;
    VolInf->FullBackupExists = FALSE;
    VolInf->BT = Type;
    
    
    VolInf->DiffLogMark.BackupStartOffset = 0;
    
    
    VolInf->Version = -1;
    VolInf->ClusterSize = 0;
    VolInf->VSSPTR = 0;
    
    
    VolInf->Stream.Records = { 0,0 };
    VolInf->Stream.Handle = 0;
    VolInf->Stream.RecIndex = 0;
    VolInf->Stream.ClusterIndex = 0;
    
    VolInf->MFTLCN = 0;
    VolInf->MFTLCNCount = 0;
    
    DWORD BytesPerSector = 0;
    DWORD SectorsPerCluster = 0;
    DWORD ClusterCount = 0;
    
    wchar_t DN[] = L"C:";
    DN[0] = Letter;
    
    VolInf->BackupID = NarGenerateBackupID(Letter);
    
    wchar_t V[] = L"!:\\";
    V[0] = Letter;
    if (GetDiskFreeSpaceW(V, &SectorsPerCluster, &BytesPerSector, 0, &ClusterCount)) {
        VolInf->ClusterSize = SectorsPerCluster * BytesPerSector;
        VolInf->VolumeTotalClusterCount = ClusterCount;
        WCHAR windir[MAX_PATH];
        GetWindowsDirectory(windir, MAX_PATH);
        if (windir[0] == Letter) {
            //Selected volume contains windows
            VolInf->IsOSVolume = TRUE;
        }
        VolInf->FileWriteMutex = CreateMutexA(NULL, FALSE, NULL);
        if (VolInf->FileWriteMutex != NULL) {
            Return = TRUE;
        }
        else {
            printf("Couldnt create mutex for volume backup information structure\n");
        }
        
        printf("Initialized volume inf %c, cluster size %i, volume cluster count %i\n", VolInf->Letter, VolInf->ClusterSize, VolInf->VolumeTotalClusterCount);
        
    }
    else {
        printf("Cant get disk information from WINAPI\n");
        DisplayError(GetLastError());
    }
    // VolInf->IsOSVolume = FALSE;
    
    
    return Return;
}

BOOL
CompareNarRecords(const void* v1, const void* v2) {
    
    nar_record* n1 = (nar_record*)v1;
    nar_record* n2 = (nar_record*)v2;
    
    if(n1->StartPos == n2->StartPos){
        return (BOOL)((int64_t)n1->Len - (int64_t)n2->Len);
    }
    else{
        return (BOOL)((int64_t)n1->StartPos - (int64_t)n2->StartPos);
    }
    
#if 0    
    // old version
    if (n1->StartPos == n2->StartPos && n2->Len < n1->Len) {
        return 1;
    }
    
    if (n1->StartPos > n2->StartPos) {
        return 1;
    }
    return -1;
#endif
    
}

VSS_ID
GetShadowPath(std::wstring Drive, CComPtr<IVssBackupComponents>& ptr, wchar_t* OutShadowPath, size_t MaxOutCh) {
    
    BOOLEAN Result = FALSE;
    BOOLEAN Error = TRUE;
    VSS_ID sid = { 0 };
    HRESULT res;
    
    res = CreateVssBackupComponents(&ptr);
    
    if (S_OK == ptr->InitializeForBackup()) {
        if (S_OK == ptr->SetContext(VSS_CTX_BACKUP)) {
            if (S_OK == ptr->StartSnapshotSet(&sid)) {
                if (S_OK == ptr->SetBackupState(false, false, VSS_BACKUP_TYPE::VSS_BT_FULL, false)) {
                    if (S_OK == ptr->AddToSnapshotSet((LPWSTR)Drive.c_str(), GUID_NULL, &sid)) {
                        CComPtr<IVssAsync> Async;
                        if (S_OK == ptr->PrepareForBackup(&Async)) {
                            Async->Wait();
                            CComPtr<IVssAsync> Async2;
                            if (S_OK == ptr->DoSnapshotSet(&Async2)) {
                                Async2->Wait();
                                Error = FALSE;
                            }
                            else {
                                printf("Can't do snapshotset\n");
                            }
                        }
                        else {
                            printf("Can't prepare for backup");
                        }
                    }
                    else {
                        printf("Can't add volume to snapshot set\n");
                    }
                }
                else {
                    printf("Can't set VSS backup state\n");
                }
            }
            else {
                printf("Can't start VSS snapshotset\n");
            }
        }
        else {
            printf("Can't set VSS context\n");
        }
    }
    else {
        printf("Can't initialize VSS for backup\n");
    }
    
    
    if (!Error) {
        VSS_SNAPSHOT_PROP SnapshotProp;
        ptr->GetSnapshotProperties(sid, &SnapshotProp);
        
        size_t l = wcslen(SnapshotProp.m_pwszSnapshotDeviceObject);
        if(MaxOutCh > l){
            wcscpy(OutShadowPath, SnapshotProp.m_pwszSnapshotDeviceObject);
        }
        else{
            printf("Shadow path can't fit given string buffer\n");
        }
    }
    
    
    return sid;
}


inline BOOLEAN
IsRegionsCollide(nar_record R1, nar_record R2) {
    
    BOOLEAN Result = FALSE;
    uint32_t R1EndPoint = R1.StartPos + R1.Len;
    uint32_t R2EndPoint = R2.StartPos + R2.Len;
    
    if (R1.StartPos == R2.StartPos && R1.Len == R2.Len) {
        return TRUE;
    }
    
    
    if ((R1EndPoint <= R2EndPoint
         && R1EndPoint >= R2.StartPos)
        || (R2EndPoint <= R1EndPoint
            && R2EndPoint >= R1.StartPos)
        ) {
        Result = TRUE;
    }
    
    return Result;
}


/*
Pass by value, might be slow if input str is too big ??
*/
inline std::vector<std::string>
Split(std::string str, std::string delimiter) {
    std::vector<std::string> Result;
    Result.reserve(100);
    
    size_t pos = 0;
    std::string token;
    while ((pos = str.find(delimiter)) != std::string::npos) {
        token = str.substr(0, pos);
        str.erase(0, pos + delimiter.length());
        Result.emplace_back(std::move(token));
    }
    if (str.length() > 0) Result.emplace_back(std::move(str));
    Result.shrink_to_fit();
    
    return Result;
}

inline std::vector<std::wstring>
Split(std::wstring str, std::wstring delimiter) {
    UINT SizeAssumed = 100;
    
    std::vector<std::wstring> Result;
    Result.reserve(SizeAssumed);
    
    size_t pos = 0;
    std::wstring token;
    while ((pos = str.find(delimiter)) != std::string::npos) {
        token = str.substr(0, pos);
        str.erase(0, pos + delimiter.length());
        Result.emplace_back(std::move(token));
    }
    if (str.length() > 0) Result.emplace_back(std::move(str));
    Result.shrink_to_fit();
    
    return Result;
}




#pragma warning(push)
#pragma warning(disable:4365)
#pragma warning(disable:4244)
//THIS FUNCTION CONTAINS BYTE MASKING AND CODE FOR EXTRACTING NON-STANDART BYTE SIZES (3-5 bytes), BETTER OMIT THIS WARNING

data_array<nar_record>
GetMFTandINDXLCN(char VolumeLetter, HANDLE VolumeHandle) {
    
    TIMED_BLOCK();
    
    BOOLEAN JustExtractMFTRegions = FALSE;
    
    uint32_t MEMORY_BUFFER_SIZE = 1024LL * 1024LL * 256;
    uint32_t ClusterExtractedBufferSize = 1024 * 1024 * 128;
    
    uint32_t MaxOutputLen = ClusterExtractedBufferSize/sizeof(nar_record);
    
    INT32 ClusterSize = NarGetVolumeClusterSize(VolumeLetter);
    
    nar_record* ClustersExtracted = (nar_record*)malloc(ClusterExtractedBufferSize);
    unsigned int ClusterExtractedCount = 0;
    
    if (ClustersExtracted != 0) {
        //memset(ClustersExtracted, 0, ClusterExtractedBufferSize);
    }
    
    auto AutoCompressAndResizeOutput = [&](){
        if(ClusterExtractedCount >= MaxOutputLen/2){
            TIMED_NAMED_BLOCK("Autocompression stuff");
            
            printf("Sort&Merge %u regions\n", ClusterExtractedCount);
            qsort(ClustersExtracted, ClusterExtractedCount, sizeof(nar_record), CompareNarRecords);
            data_array<nar_record> temp;
            temp.Data = ClustersExtracted;
            temp.Count = ClusterExtractedCount;
            MergeRegions(&temp);
            
            printf("New region length %u\n", temp.Count);
            
            temp.Data = (nar_record*)realloc(temp.Data, MaxOutputLen*2*sizeof(nar_record));
            MaxOutputLen *= 2;
            ClustersExtracted = temp.Data;
            ClusterExtractedCount = temp.Count;
            
        }
    };
    
    void* FileBuffer = malloc(MEMORY_BUFFER_SIZE);
    uint32_t FileBufferCount = MEMORY_BUFFER_SIZE / 1024LL;
    
    if(NULL == FileBuffer
       || NULL == ClustersExtracted){
        free(ClustersExtracted);
        ClustersExtracted = 0;
        ClusterExtractedCount = 0;
        
        goto EARLY_TERMINATION;
    }
    
    if (FileBuffer) {
        char VolumeName[64];
        snprintf(VolumeName,sizeof(VolumeName), "\\\\.\\%c:", VolumeLetter);
        
        INT16 DirectoryFlag = 0x0002;
        INT32 FileRecordSize = 1024;
        INT16 FlagOffset = 22;
        
        
        if (VolumeHandle != INVALID_HANDLE_VALUE) {
            
            DWORD BR = 0;
            
            unsigned int RecCountByCommandLine = 0;
            nar_record* TempRecords = NarGetMFTRegionsByCommandLine(VolumeLetter, &RecCountByCommandLine);
            if (TempRecords != 0 && RecCountByCommandLine != 0) {
                printf("Parsed mft regions from command line !\n");
                memcpy(ClustersExtracted, TempRecords, RecCountByCommandLine*sizeof(nar_record));
                ClusterExtractedCount = RecCountByCommandLine;
            }
            NarFreeMFTRegionsByCommandLine(TempRecords);
            
            // TODO (Batuhan): remove this after testing on windows server, looks like rest of the code finds some invalid regions on volume.
            if (JustExtractMFTRegions) {
                printf("Found %i regions\n", ClusterExtractedCount);
                for(uint32_t indx = 0; indx < ClusterExtractedCount; indx++){
                    printf("0x%X\t0x%X\n", ClustersExtracted[indx].StartPos, ClustersExtracted[indx].Len);
                }
                goto EARLY_TERMINATION;
            }
            
            
            //nar_region *MFTRegions = 0;
            //goto EARLY_TERMINATION;
            
            unsigned int MFTRegionCount = ClusterExtractedCount;
            
            {
                size_t TotalFC = 0;
                printf("Region count %u\n", MFTRegionCount);
                for(unsigned int MFTOffsetIndex = 0; MFTOffsetIndex < MFTRegionCount; MFTOffsetIndex++){
                    TotalFC += ClustersExtracted[MFTOffsetIndex].Len;
                }
                printf("Total file count is %I64u\n", TotalFC*4ull);
            }
            
            for (unsigned int MFTOffsetIndex = 0; MFTOffsetIndex < MFTRegionCount; MFTOffsetIndex++) {
                
                ULONGLONG Offset = (ULONGLONG)ClustersExtracted[MFTOffsetIndex].StartPos * (uint64_t)ClusterSize;
                INT32 FilePerCluster = ClusterSize / 1024;
                ULONGLONG FileCount = (ULONGLONG)ClustersExtracted[MFTOffsetIndex].Len * (uint64_t)FilePerCluster;
                
                // set file pointer to actual records
                if (NarSetFilePointer(VolumeHandle, Offset)) {
                    
                    size_t FileRemaining = FileCount;
                    
                    while(FileRemaining > 0) {
                        // FileBufferCount > FileCount
                        // We can read all file records for given data region
                        
                        size_t TargetFileCount = MIN(FileBufferCount, FileRemaining);
                        
                        if (ReadFile(VolumeHandle, FileBuffer, TargetFileCount * 1024, &BR, 0) && BR == TargetFileCount * 1024) {
                            
                            GlobalReadFileCallCount++;
                            
                            
                            for (unsigned int FileRecordIndex = 0; FileRecordIndex < TargetFileCount; FileRecordIndex++) {
                                
                                
                                TIMED_NAMED_BLOCK("File record parser");
                                
                                void* FileRecord = (BYTE*)FileBuffer + (uint64_t)FileRecordSize * (uint64_t)FileRecordIndex;
                                
                                
#if 1                                
                                _mm_prefetch((const char*)FileRecord + 1024*1, _MM_HINT_T0);
                                _mm_prefetch((const char*)FileRecord + 1024*2, _MM_HINT_T0);
                                _mm_prefetch((const char*)FileRecord + 1024*2, _MM_HINT_T0);
                                _mm_prefetch((const char*)FileRecord + 1024*4, _MM_HINT_T0);
                                
                                
                                _mm_prefetch((const char*)FileRecord + 1024*5, _MM_HINT_T0);
                                _mm_prefetch((const char*)FileRecord + 1024*6, _MM_HINT_T0);
                                _mm_prefetch((const char*)FileRecord + 1024*7, _MM_HINT_T0);
                                _mm_prefetch((const char*)FileRecord + 1024*8, _MM_HINT_T0);
                                
                                
                                _mm_prefetch((const char*)FileRecord + 1024*9, _MM_HINT_T0);
                                _mm_prefetch((const char*)FileRecord + 1024*10, _MM_HINT_T0);
                                _mm_prefetch((const char*)FileRecord + 1024*11, _MM_HINT_T0);
                                _mm_prefetch((const char*)FileRecord + 1024*12, _MM_HINT_T0);
                                
                                
                                _mm_prefetch((const char*)FileRecord + 1024*13, _MM_HINT_T0);
                                _mm_prefetch((const char*)FileRecord + 1024*14, _MM_HINT_T0);
                                _mm_prefetch((const char*)FileRecord + 1024*15, _MM_HINT_T0);
                                _mm_prefetch((const char*)FileRecord + 1024*16, _MM_HINT_T0);
#endif
                                
                                
                                // file flags are at 22th offset in the record
                                if (*(INT32*)FileRecord != 'ELIF') {
                                    
                                    // block doesnt start with 'FILE0', skip
                                    continue;
                                }
                                
#if 1                                
                                _mm_prefetch((const char*)FileRecord, _MM_HINT_T0);
                                _mm_prefetch((const char*)FileRecord + 0x40, _MM_HINT_T0);
                                _mm_prefetch((const char*)FileRecord + 0x80, _MM_HINT_T0);
                                _mm_prefetch((const char*)FileRecord + 0xC0, _MM_HINT_T0);
#endif
                                
                                void *IndxOffset = NarFindFileAttributeFromFileRecord(FileRecord, NAR_INDEX_ALLOCATION_FLAG);
                                
                                
                                if(IndxOffset != NULL){
                                    AutoCompressAndResizeOutput();
                                    
                                    void *BitmapAttr = NarFindFileAttributeFromFileRecord(FileRecord, NAR_BITMAP_FLAG);
                                    
                                    if(BitmapAttr == NULL){
                                        uint32_t RegFound = 0;
                                        NarParseIndexAllocationAttribute(IndxOffset, &ClustersExtracted[ClusterExtractedCount], MaxOutputLen - ClusterExtractedCount, &RegFound);
                                        ClusterExtractedCount += RegFound;
                                    }
                                    else{
                                        // NOTE(Batuhan): parses indx allocation to one-cluster granuality blocks, to make bitmap parsing trivial. increases memory usage(at parsing stage, not for the final outcome), but makes parsing much easier.
                                        
                                        uint32_t RegFound = 0;
                                        NarParseIndexAllocationAttributeSingular(IndxOffset, &ClustersExtracted[ClusterExtractedCount], MaxOutputLen - ClusterExtractedCount, &RegFound);
                                        
                                        
                                        int BLen = NarGetBitmapAttributeDataLen(BitmapAttr);
                                        unsigned char* Bitmap = (unsigned char*)NarGetBitmapAttributeData(BitmapAttr);
                                        
                                        size_t ceil = MIN(RegFound, BLen);
                                        for(size_t ci = 0; ci<BLen; ci++){
                                            TIMED_NAMED_BLOCK("Bitmap merge stuff");
                                            size_t ByteIndex = ci/8;
                                            size_t BitIndex = ci%8;
                                            if(Bitmap[ByteIndex] & (1 << BitIndex)){
                                                // everything is ok
                                            }
                                            else{
                                                ClustersExtracted[ci + ClusterExtractedCount].StartPos = 0;
                                                ClustersExtracted[ci + ClusterExtractedCount].Len = 0;
                                            }
                                        }
                                        ClusterExtractedCount += RegFound;
                                        
                                    }
                                    
                                    
                                }
                                
                            }
                            
                            
                            
                            
                        }
                        else {
                            printf("Couldnt read file records\n");
                        }
                        
                        FileRemaining -= TargetFileCount;
                    }// END OF WHILE(FILE REMAINING)
                    
                }
                else {
                    printf("Couldnt set file pointer to %I64d\n", Offset);
                }
                
            }
        }
        else {
            // couldnt open volume as file
            printf("VSSVolumeHandle was invalid \n");
            DisplayError(GetLastError());
        }
        
    }
    else {
        printf("FATAL: Couldnt allocate memory for file buffer\n");
        // NOTE(Batuhan): cant allocate memory
    }
    
    
    EARLY_TERMINATION:
    
    free(FileBuffer);
    
    
    data_array<nar_record> Result = { 0 };
    if(ClusterExtractedCount > 0){
        ClustersExtracted = (nar_record*)realloc(ClustersExtracted, ClusterExtractedCount * sizeof(nar_record));
    }
    
    Result.Data = ClustersExtracted;
    Result.Count = ClusterExtractedCount;
    
    
    if(Result.Count > 0){
        printf("Will be sorting&merging %u regions\n", Result.Count); 
        qsort(Result.Data, Result.Count, sizeof(nar_record), CompareNarRecords);
        MergeRegions(&Result);
    }
    
    
    
    ULONGLONG VolumeSize = NarGetVolumeTotalSize(VolumeLetter);
    uint32_t TruncateIndex = 0;
    for (uint32_t i = 0; i < Result.Count; i++) {
        if ((ULONGLONG)Result.Data[i].StartPos * (ULONGLONG)ClusterSize + (ULONGLONG)Result.Data[i].Len * ClusterSize > VolumeSize) {
            NAR_BREAK;
            TruncateIndex = i;
            break;
        }
    }
    
    ASSERT(TruncateIndex == 0);
    
    if (TruncateIndex > 0) {
        printf("INDEX_ALLOCATION blocks exceeds volume size, truncating from index %i\n", TruncateIndex);
        Result.Data = (nar_record*)realloc(Result.Data, TruncateIndex * sizeof(nar_record));
        Result.Count = TruncateIndex;
    }
    
    return Result;
}

#pragma warning(pop)



/*
This operation just adds volume to list, does not starts to filter it,
until it's fullbackup is requested. After fullbackup, call AttachVolume to start filtering
*/
BOOLEAN
AddVolumeToTrack(PLOG_CONTEXT Context, wchar_t Letter, BackupType Type) {
    BOOLEAN ErrorOccured = TRUE;
    
    volume_backup_inf VolInf;
    BOOLEAN FOUND = FALSE;
    
    INT32 ID = GetVolumeID(Context, Letter);
    
    if (ID == NAR_INVALID_VOLUME_TRACK_ID) {
        NAR_COMMAND Command;
        Command.Type    = NarCommandType_AddVolume;
        Command.Letter  = Letter;
        
        if (NarGetVolumeGUIDKernelCompatible(Letter, Command.VolumeGUIDStr)) {
            
            DWORD BytesReturned;
            printf("Volume GUID for %c : %S\n", (char)Letter, Command.VolumeGUIDStr);
            HRESULT hResult = FilterSendMessage(Context->Port, &Command, sizeof(Command), 0, 0, &BytesReturned);
            
            if (SUCCEEDED(hResult)) {
                if (InitVolumeInf(&VolInf, Letter, Type)) {
                    ErrorOccured = FALSE;
                }
                Context->Volumes.Insert(VolInf);
                printf("Volume %c inserted to the list\n", (char)Letter);
            }
            else {
                printf("KERNEL error occured: Couldnt add volume @ kernel side, error code %i, volume %c\n", hResult, (char)Letter);
            }
            
        }
        else {
            printf("Couldnt get volume kernel compatible volume GUID\n");
            DisplayError(GetLastError());
        }
    }
    else {
        printf("Volume %c is already in list\n", (char)Letter);
    }
    
    return !ErrorOccured;
}


BOOLEAN
GetVolumesOnTrack(PLOG_CONTEXT C, volume_information* Out, unsigned int BufferSize, int* OutCount) {
    
    if (!Out || !C || !C->Volumes.Data) {
        return FALSE;
    }
    
    unsigned int VolumesFound = 0;
    unsigned int MaxFit = BufferSize / (unsigned int)sizeof(*Out);
    BOOLEAN Result = FALSE;
    
    for (unsigned int i = 0; i < C->Volumes.Count; i++) {
        
        volume_backup_inf* V = &C->Volumes.Data[i];
        if (MaxFit == VolumesFound) {
            Result = FALSE;
            break;
        }
        
        if (V->INVALIDATEDENTRY) {
            continue;
        }
        
        Out[VolumesFound].Letter    = (char)V->Letter;
        Out[VolumesFound].Bootable  = V->IsOSVolume;
        Out[VolumesFound].DiskID    = NarGetVolumeDiskID((char)V->Letter);
        Out[VolumesFound].DiskType  = (char)NarGetVolumeDiskType((char)V->Letter);
        Out[VolumesFound].TotalSize = NarGetVolumeTotalSize((char)V->Letter);
        
        VolumesFound++;
        
    }
    
    if (Result == FALSE) {
        memset(Out, 0, BufferSize); //If any error occured, clear given buffer
    }
    else {
        *OutCount = (int)VolumesFound;
    }
    
    return Result;
}



/*
For some reason, kernel and user GUID is not compatible for one character, which is just question mark (?)
. to be kernel compatible, one must call this function to communicate with kernel, otherwise kernel cant distinguish
given GUID and will return error.

VolumeGUID MUST have size of 98 bytes, (49 unicode char)
*/
BOOLEAN
NarGetVolumeGUIDKernelCompatible(wchar_t Letter, wchar_t *VolumeGUID) {
    
    if (VolumeGUID == NULL) return FALSE;
    BOOLEAN Result = FALSE;
    wchar_t MountPoint[] = L"!:\\";
    MountPoint[0] = Letter;
    
    
    wchar_t LocalGUID[50];
    // This function appends trailing backslash to string, we can ignore that, but still need to pass 50 cch long buffer
    // to not to fail in the function. silly way to do thing
    if (GetVolumeNameForVolumeMountPointW(MountPoint, LocalGUID, 50)) {     
        memcpy(VolumeGUID, LocalGUID, 49*sizeof(wchar_t));
        VolumeGUID[1] = L'?';
        // GetVolumeNameForVolumeMountPointW returns string with trailing backslash (\\), i dont remember that thing's name but it is it
        // but kernel doesnt use that, so just remove it
        if(VolumeGUID[48] == L'\\'){
            VolumeGUID[48] = '\0'; 
        }
        Result = TRUE;
    }
    
    
    return Result;
}



/*
 Just removes volume entry from kernel memory, does not unattaches it.
*/
inline BOOLEAN
NarRemoveVolumeFromKernelList(wchar_t Letter, HANDLE CommPort) {
    
    if(CommPort == INVALID_HANDLE_VALUE) return FALSE;
    BOOLEAN Result = FALSE;
    
    NAR_COMMAND Command = {};
    Command.Type = NarCommandType_DeleteVolume;
    
    if(NarGetVolumeGUIDKernelCompatible(Letter, Command.VolumeGUIDStr)){
        DWORD BR = 0;
        HRESULT hResult = FilterSendMessage(CommPort, &Command, sizeof(NAR_COMMAND), 0,0, &BR);
        if(SUCCEEDED(hResult)){
            Result = TRUE;
        }
        else{
            printf("FilterSendMessage failed, couldnt remove volume from kernel side, err %i\n", hResult);
            DisplayError(GetLastError());
        }
    }
    else{
        printf("Couldnt get kernel compatible guid\n");
    }
    
    return FALSE;
}



BOOLEAN
RemoveVolumeFromTrack(LOG_CONTEXT *C, wchar_t L) {
    BOOLEAN Result = FALSE;
    
    int ID = GetVolumeID(C, L);
    
    if (ID >= 0) {
        
        volume_backup_inf* V = &C->Volumes.Data[ID];
        DetachVolume(V);
        V->INVALIDATEDENTRY = TRUE;
        
        V->Letter = 0;
        
        if(NarRemoveVolumeFromKernelList(L, C->Port)){
            
        }
        
        V->FullBackupExists = FALSE;
        V->FilterFlags.IsActive = FALSE;
        
        
        FreeDataArray(&V->Stream.Records);
        V->Stream.RecIndex = 0;
        V->Stream.ClusterIndex = 0;
        CloseHandle(V->Stream.Handle);
        V->VSSPTR.Release();
        
        
        Result = TRUE;
    }
    else {
        printf("Unable to find volume %c in track list \n", L);
        Result = FALSE;
    }
    
    
    return Result;
}

BOOLEAN
DetachVolume(volume_backup_inf* VolInf) {
    BOOLEAN Return = TRUE;
    HRESULT Result = 0;
    std::wstring Temp = L"!:\\";
    Temp[0] = VolInf->Letter;
    
    Result = FilterDetach(MINISPY_NAME, Temp.c_str(), 0);
    
    if (SUCCEEDED(Result)) {
        VolInf->FilterFlags.IsActive = FALSE;
    }
    else {
        printf("Can't detach filter\n");
        printf("Result was -> %d\n", Result);
        DisplayError((DWORD)Result);
        Return = FALSE;
    }
    
    return Return;
}


/*
Attachs filter
SetActive: default value is TRUE
*/
inline BOOLEAN
AttachVolume(char Letter) {
    
    BOOLEAN Return = FALSE;
    HRESULT Result = 0;
    
    WCHAR Temp[] = L"!:\\";
    Temp[0] = (wchar_t)Letter;
    
    Result = FilterAttach(MINISPY_NAME, Temp, 0, 0, 0);
    if (SUCCEEDED(Result) || Result == ERROR_FLT_INSTANCE_NAME_COLLISION) {
        Return = TRUE;
    }
    else {
        printf("Can't attach filter\n");
        DisplayError(GetLastError());
    }
    
    return Return;
}


INT32
GetVolumeID(PLOG_CONTEXT C, wchar_t Letter) {
    
    INT ID = NAR_INVALID_VOLUME_TRACK_ID;
    if (C->Volumes.Data != NULL) {
        if (C->Volumes.Count != 0) {
            for (size_t i = 0; i < C->Volumes.Count; i++) {
                // TODO(Batuhan): check invalidated entry !
                
                if (!C->Volumes.Data[i].INVALIDATEDENTRY && C->Volumes.Data[i].Letter == Letter) {
                    ID = (int)i; // safe conversion
                    break;
                }
            }
        }
        else {
            printf("Context's volume array is empty");
        }
        
    }
    else {
        printf("Context's volume array was null\n");
    }
    
    return ID;
}

// Assumes CallerBufferSize >= NAR_COMPRESSION_FRAME_SIZE
uint32_t
ReadStream(volume_backup_inf* VolInf, void* CallerBuffer, unsigned int CallerBufferSize) {
    
    //TotalSize MUST be multiple of cluster size
    uint32_t Result = 0;
    
    void* BufferToFill = CallerBuffer;
    unsigned int TotalSize = CallerBufferSize;
    if(true == VolInf->Stream.ShouldCompress){
        BufferToFill = VolInf->Stream.CompressionBuffer;
        TotalSize = VolInf->Stream.BufferSize;
    }
    
    if (TotalSize == 0) {
        printf("Passed totalsize as 0, terminating now\n");
        return TRUE;
    }
    
    unsigned int RemainingSize = TotalSize;
    void* CurrentBufferOffset = BufferToFill;
    
    if ((UINT)VolInf->Stream.RecIndex >= VolInf->Stream.Records.Count) {
        printf("End of the stream\n", VolInf->Stream.RecIndex, VolInf->Stream.Records.Count);
        return Result;
    }
    
    while (RemainingSize) {
        
        if ((UINT)VolInf->Stream.RecIndex >= VolInf->Stream.Records.Count) {
            printf("Rec index was higher than record's count, result %u, rec_index %i rec_count %i\n", Result, VolInf->Stream.RecIndex, VolInf->Stream.Records.Count);
            break;
        }
        
        DWORD BytesReadAfterOperation = 0;
        
        uint64_t ClustersRemainingByteSize = (uint64_t)VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].Len - (uint64_t)VolInf->Stream.ClusterIndex;
        ClustersRemainingByteSize *= VolInf->ClusterSize;
        
        DWORD ReadSize = (DWORD)MIN((uint64_t)RemainingSize, ClustersRemainingByteSize); // safe to truncate, since remainingsize's max value is uint32_t_MAX, and its MIN macro
        // we expect max value of DWORD.
        
        ULONGLONG FilePtrTarget = (ULONGLONG)VolInf->ClusterSize * ((ULONGLONG)VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].StartPos + (ULONGLONG)VolInf->Stream.ClusterIndex);
        if (NarSetFilePointer(VolInf->Stream.Handle, FilePtrTarget)) {
            
            BOOL OperationResult = ReadFile(VolInf->Stream.Handle, CurrentBufferOffset, ReadSize, &BytesReadAfterOperation, 0);
            Result += BytesReadAfterOperation;
            if (!OperationResult || BytesReadAfterOperation != ReadSize) {
                printf("STREAM ERROR: Couldnt read %lu bytes, instead read %lu, error code %i\n", ReadSize, BytesReadAfterOperation, OperationResult);
                printf("rec_index % i rec_count % i, remaining bytes %I64u, offset at disk %I64u\n", VolInf->Stream.RecIndex, VolInf->Stream.Records.Count, ClustersRemainingByteSize, FilePtrTarget);
                printf("Total bytes read for buffer %u\n", Result);
                
                NarDumpToFile("STREAM_OVERFLOW_ERROR_LOGS", VolInf->Stream.Records.Data, VolInf->Stream.Records.Count*sizeof(nar_record));
                VolInf->Stream.Error = stream::Error_Read;
                
                goto ERR_BAIL_OUT;
            }
            
            
        }
        else {
            printf("Couldnt set file pointer to %I64d\n", FilePtrTarget);
            VolInf->Stream.Error = stream::Error_SetFP;
            goto ERR_BAIL_OUT;
        }
        
        INT32 ClusterToIterate = (INT32)(BytesReadAfterOperation / 4096);
        VolInf->Stream.ClusterIndex += ClusterToIterate;
        
        if ((uint32_t)VolInf->Stream.ClusterIndex == VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].Len) {
            VolInf->Stream.ClusterIndex = 0;
            VolInf->Stream.RecIndex++;
        }
        if ((uint32_t)VolInf->Stream.ClusterIndex > VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].Len) {
            printf("ClusterIndex exceeded region len, that MUST NOT happen at any circumstance\n");
            VolInf->Stream.Error = stream::Error_SizeOvershoot;
            goto ERR_BAIL_OUT;
        }
        
        if (BytesReadAfterOperation % VolInf->ClusterSize != 0) {
            printf("Couldnt read cluster aligned size, error, read %i bytes instead of %i\n", BytesReadAfterOperation, ReadSize);
        }
        
        RemainingSize -= BytesReadAfterOperation;
        CurrentBufferOffset = (char*)BufferToFill + (Result);
        
    }
    
    if(true == VolInf->Stream.ShouldCompress
       && Result > 0){
        
        ZSTD_outBuffer output = {0};
        ZSTD_inBuffer input = {0};
        
        {
            output.dst  = CallerBuffer;
            output.size = CallerBufferSize;
            output.pos  = 0;
            
            input.src   = BufferToFill;
            input.size  = Result;
            input.pos   = 0;
        }
        
        size_t RetCode = 0;
        RetCode = ZSTD_compressStream2(VolInf->Stream.CCtx, &output, &input, ZSTD_e_end);
        ASSERT(input.pos == input.size);
        ASSERT(!ZSTD_isError(RetCode));
        
        if(!ZSTD_isError(RetCode)){
            Result = output.pos;
        }
        else{
            VolInf->Stream.Error = stream::Error_Compression;
            goto ERR_BAIL_OUT;
        }
        
    }
    
    return Result;
    
    ERR_BAIL_OUT:
    
    Result = 0;
    return Result;
    
}

BOOLEAN
TerminateBackup(volume_backup_inf* V, BOOLEAN Succeeded) {
    
    BOOLEAN Return = FALSE;
    if (!V) return FALSE;
    
    printf("Succeeded %i for volume %c\n", Succeeded, V->Letter);
    
    if (!V->FullBackupExists) {
        
        //Termination of fullbackup
        printf("Fullbackup operation will be terminated\n");
        if (Succeeded) {
            
            if (SaveMetadata((char)V->Letter, NAR_FULLBACKUP_VERSION, V->ClusterSize, V->BT, V->Stream.Records, V->Stream.Handle, V->MFTLCN, V->MFTLCNCount, V->BackupID, V->Stream.ShouldCompress)) {
                Return = TRUE;
                V->Version = 0;
                V->FullBackupExists = TRUE;
            }
            else{
                printf("Coulndt save metadata\n");
            }
            
        }
        else {
            //Somehow operation failed.
            V->FilterFlags.IsActive = FALSE;
            V->Version = -1;
            V->FullBackupExists = FALSE;
        }
        
        //Termination of fullbackup
        //neccecary for incremental backup
        if(V->BT == BackupType::Inc){
            V->IncLogMark.LastBackupRegionOffset = 0;
            V->PossibleNewBackupRegionOffsetMark = 0;
        }
        
        
    }
    else {
        //Termination of diff-inc backup
        printf("Termination of diff-inc backup\n");
        
        if(Succeeded) {
            
            // NOTE(Batuhan):
            printf("Will save metadata to working directory, Version : %i\n", V->Version);
            SaveMetadata((char)V->Letter, V->Version, V->ClusterSize, V->BT, V->Stream.Records, V->Stream.Handle, V->MFTLCN, V->MFTLCNCount, V->BackupID, V->Stream.ShouldCompress);
            
            /*
         Since merge algorithm may have change size of the record buffer,
            we should overwrite and truncate it
         */
            Return = TRUE;
            V->Version++;
            
            
            if(V->BT == BackupType::Inc){
                V->IncLogMark.LastBackupRegionOffset = V->PossibleNewBackupRegionOffsetMark;
            }
            
            V->PossibleNewBackupRegionOffsetMark = 0;
            
            //backup operation succeeded condition end (for diff and inc)
        }
        else{
            printf("Backup failed for inc-diff operation\n");
        }
        
    }
    
    if(NULL != V->Stream.CompressionBuffer){
        V->Stream.BufferSize = 0;
        free(V->Stream.CompressionBuffer);
        V->Stream.CompressionBuffer = NULL;
    }
    
    if(V->Stream.CStream)  ZSTD_freeCStream(V->Stream.CStream);
    if(V->Stream.CCtx)     ZSTD_freeCCtx(V->Stream.CCtx);
    
    
    if(V->Stream.Handle != INVALID_HANDLE_VALUE) {
        CloseHandle(V->Stream.Handle);
    }
    if(V->Stream.Records.Data != NULL){
        FreeDataArray(&V->Stream.Records);
    }
    
    NarFreeMFTRegionsByCommandLine(V->MFTLCN);
    
    V->Stream.Records.Count = 0;
    V->Stream.RecIndex = 0;
    V->Stream.ClusterIndex = 0;
    {
        LONG Deleted=0;
        VSS_ID NonDeleted;
        HRESULT hr;
        CComPtr<IVssAsync> async;
        hr = V->VSSPTR->BackupComplete(&async);
        if(hr == S_OK){
            async->Wait();
        }
        hr = V->VSSPTR->DeleteSnapshots(V->SnapshotID, VSS_OBJECT_SNAPSHOT, TRUE, &Deleted, &NonDeleted);
        V->VSSPTR.Release();
    }
    
    
    return Return;
}

/*
*/
BOOLEAN
SetupStream(PLOG_CONTEXT C, wchar_t L, BackupType Type, DotNetStreamInf* SI, bool ShouldCompress) {
    
    TIMED_BLOCK();
    
    BOOLEAN Return = FALSE;
    int ID = GetVolumeID(C, L);
    
    if (ID < 0) {
        //If volume not found, try to add it
        printf("Couldnt find volume %c in list, adding it for stream setup\n", L);
        AddVolumeToTrack(C, L, Type);
        ID = GetVolumeID(C, L);
        if (ID < 0) {
            return FALSE;
        }
    }
    
    volume_backup_inf* VolInf = &C->Volumes.Data[ID];
    
    printf("Entered setup stream for volume %c, version %i\n", (char)L, VolInf->Version);
    
    VolInf->VSSPTR.Release();
    VolInf->Stream.ClusterIndex = 0;
    VolInf->Stream.RecIndex = 0;
    VolInf->Stream.Records.Count = 0;
    
    auto AppendINDXnMFTLCNToStream = [&]() {
        
        TIMED_NAMED_BLOCK("AppendINDXMFTLCN");
        data_array<nar_record> MFTandINDXRegions = GetMFTandINDXLCN((char)L, VolInf->Stream.Handle);
        if (MFTandINDXRegions.Data != 0) {
            
            printf("Parsed MFTLCN for volume %c for version %i, count %u", (wchar_t)VolInf->Letter, VolInf->Version, VolInf->MFTLCNCount);
            VolInf->Stream.Records.Data = (nar_record*)realloc(VolInf->Stream.Records.Data, (VolInf->Stream.Records.Count + MFTandINDXRegions.Count) * sizeof(nar_record));
            memcpy(&VolInf->Stream.Records.Data[VolInf->Stream.Records.Count], MFTandINDXRegions.Data, MFTandINDXRegions.Count * sizeof(nar_record));
            
            VolInf->Stream.Records.Count += MFTandINDXRegions.Count;
            
            qsort(VolInf->Stream.Records.Data, VolInf->Stream.Records.Count, sizeof(nar_record), CompareNarRecords);
            MergeRegions(&VolInf->Stream.Records);
            
        }
        else {
            printf("Couldnt parse MFT at setupstream function for volume %c, version %i\n", L, VolInf->Version);
        }
        
        FreeDataArray(&MFTandINDXRegions);
    };
    
    
    WCHAR Temp[] = L"!:\\";
    Temp[0] = VolInf->Letter;
    wchar_t ShadowPath[256];
    VolInf->SnapshotID = GetShadowPath(Temp, VolInf->VSSPTR, ShadowPath, sizeof(ShadowPath)/sizeof(ShadowPath[0]));
    
    if (ShadowPath == NULL) {
        printf("Can't get shadowpath from VSS\n");
        return FALSE;
    }
    
    // no overheat for attaching volume again and again
    if(FALSE == AttachVolume(VolInf->Letter)){
        printf("Cant attach volume\n");
        return FALSE;
    }
    
    VolInf->Stream.Handle = CreateFileW(ShadowPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
    if (VolInf->Stream.Handle == INVALID_HANDLE_VALUE) {
        printf("Can not open shadow path %S..\n", ShadowPath);
        DisplayError(GetLastError());
        return FALSE;
    }
    printf("Setup stream handle successfully\n");
    
    // NOTE(Batuhan): Experimental feature, from now on (06.10.2020), every binary data MUST contain MFTLCN with extended INDEX_ALLOCATION data.
    // that helps file explorer to search that volume 
    
    if (!VolInf->FullBackupExists) {
        printf("Fullbackup stream is preparing\n");
        
        //Fullbackup stream
        if (SetFullRecords(VolInf)) {
            Return = TRUE;
        }
        else {
            printf("SetFullRecords failed!\n");
        }
        
        if (SI && Return) {
            //SI->MetadataFileName;
            GenerateMetadataName(VolInf->BackupID, NAR_FULLBACKUP_VERSION, SI->MetadataFileName);
            //SI->FileName;
            GenerateBinaryFileName(VolInf->BackupID, NAR_FULLBACKUP_VERSION, SI->FileName);
        }
    }
    else {
        //Incremental or diff stream
        BackupType T = VolInf->BT;
        
        if (T == BackupType::Diff) {
            if (SetDiffRecords(C->Port,VolInf)) {
                Return = TRUE;
            }
            else {
                printf("Couldn't setup diff records for streaming\n");
            }
            
        }
        else if (T == BackupType::Inc) {
            if (SetIncRecords(C->Port, VolInf)) {
                Return = TRUE;
            }
            else {
                printf("Couldn't setup inc records for streaming\n");
            }
            
        }
        
        if (SI && Return) {
            GenerateBinaryFileName(VolInf->BackupID, VolInf->Version, SI->FileName);
            GenerateMetadataName(VolInf->BackupID, VolInf->Version, SI->MetadataFileName);
        }
        
    }
    
    if(Return != FALSE){
        volume_backup_inf* V = VolInf;
        AppendINDXnMFTLCNToStream();
        V->MFTLCN = NarGetMFTRegionsByCommandLine(V->Letter, &V->MFTLCNCount);
        
        int TruncateIndex = -1;
        
        SI->ClusterCount = 0;
        SI->ClusterSize = V->ClusterSize;
        
        for(unsigned int RecordIndex = 0; RecordIndex < V->Stream.Records.Count; RecordIndex++){
            if((INT64)V->Stream.Records.Data[RecordIndex].StartPos + (INT64)V->Stream.Records.Data[RecordIndex].Len > (INT64)V->VolumeTotalClusterCount){
                TruncateIndex = RecordIndex;
                break;
            }
            else{
                SI->ClusterCount += V->Stream.Records.Data[RecordIndex].Len;
            }
        }
        
        if (TruncateIndex == 0) {
            printf("Error occured, trimming all stream record information\n");
            Return = FALSE;
            FreeDataArray(&V->Stream.Records);
        }
        if(TruncateIndex > 0){
            printf("Found regions that exceeds volume size, truncating stream record array from %i to %i\n", V->Stream.Records.Count, TruncateIndex);
            V->Stream.Records.Data = 
                (nar_record*)realloc(V->Stream.Records.Data, TruncateIndex*sizeof(nar_record));            
            V->Stream.Records.Count = TruncateIndex;
        }
        
    }
    
    // NOTE(Batuhan): if failed, revert stream info
    if(Return == FALSE){
        SI->ClusterCount = 0;
        SI->ClusterSize = 0;
    }
    else{
        
        // NOTE(Batuhan): Compression stuff
        if(ShouldCompress){
            
            VolInf->Stream.CompressionBuffer = malloc(NAR_COMPRESSION_FRAME_SIZE);
            VolInf->Stream.BufferSize = NAR_COMPRESSION_FRAME_SIZE;
            
            if(NULL != VolInf->Stream.CompressionBuffer){        
                VolInf->Stream.ShouldCompress = true;
                
                VolInf->Stream.CCtx = ZSTD_createCCtx();
                VolInf->Stream.CStream = ZSTD_createCStream();
                
                ASSERT(VolInf->Stream.CCtx);
                ASSERT(VolInf->Stream.CStream);
                
                ZSTD_bounds ThreadBounds = ZSTD_cParam_getBounds(ZSTD_c_nbWorkers);
                if(!ZSTD_isError(ThreadBounds.error)){
                    ZSTD_CCtx_setParameter(VolInf->Stream.CCtx, ZSTD_c_nbWorkers, ThreadBounds.upperBound);
                }
                else{
                    NAR_BREAK;
                    printf("Couldn't query worker thread bounds for compression, zstd error code : %X\n", ThreadBounds.error);
                    ZSTD_CCtx_setParameter(VolInf->Stream.CCtx, ZSTD_c_nbWorkers, 0);
                }
            }
            else{
                VolInf->Stream.ShouldCompress = false;        
                VolInf->Stream.BufferSize = 0;
            }
            
        }
        
    }
    
    printf("Totalbytes should be backed up %I64u\n", (uint64_t)SI->ClusterCount*(uint64_t)SI->ClusterSize);
    
    return Return;
}


/*
  From given volume handle, extracts region information from volume
  Handle can be VSS handle or normal volume handle. VSS is preferred since volume information might change over time
  returns NULL if any error occurs
*/
nar_record*
GetVolumeRegionsFromBitmap(HANDLE VolumeHandle, uint32_t* OutRecordCount) {
    
    if(OutRecordCount == NULL) return NULL;
    
    nar_record* Records = NULL;
    uint32_t RecordCount = 0;
    
    
    STARTING_LCN_INPUT_BUFFER StartingLCN;
    StartingLCN.StartingLcn.QuadPart = 0;
    ULONGLONG MaxClusterCount = 0;
    DWORD BufferSize = 1024 * 1024 * 64; //  megabytes
    
    VOLUME_BITMAP_BUFFER* Bitmap = (VOLUME_BITMAP_BUFFER*)malloc(BufferSize);
    
    if (Bitmap != NULL) {
        
        HRESULT R = DeviceIoControl(VolumeHandle, FSCTL_GET_VOLUME_BITMAP, &StartingLCN, sizeof(StartingLCN), Bitmap, BufferSize, 0, 0);
        if (SUCCEEDED(R)) {
            
            MaxClusterCount = (ULONGLONG)Bitmap->BitmapSize.QuadPart;
            DWORD ClustersRead = 0;
            UCHAR* BitmapIndex = Bitmap->Buffer;
            UCHAR BitmapMask = 1;
            //uint32_t CurrentIndex = 0;
            uint32_t LastActiveCluster = 0;
            
            uint32_t RecordBufferSize = 128 * 1024 * 1024;
            Records = (nar_record*)malloc(RecordBufferSize);
            
            memset(Records, 0, RecordBufferSize);
            
            Records[0] = { 0,1 };
            RecordCount++; 
            
            ClustersRead++;
            BitmapMask <<= 1;
            
            while (ClustersRead < MaxClusterCount) {
                
                if ((*BitmapIndex & BitmapMask) == BitmapMask) {
                    
                    if (LastActiveCluster == ClustersRead - 1) {
                        Records[RecordCount].Len++;
                    }
                    else {
                        Records[++RecordCount] = { ClustersRead, 1 };
                    }
                    
                    LastActiveCluster = ClustersRead;
                    
                }
                BitmapMask <<= 1;
                if (BitmapMask == 0) {
                    BitmapMask = 1;
                    BitmapIndex++;
                }
                ClustersRead++;
            }
            
            printf("Successfully set fullbackup records\n");
            
            
        }
        else {
            // NOTE(Batuhan): DeviceIOControl failed
            printf("Get volume bitmap failed [DeviceIoControl].\n");
            printf("Result was -> %d\n", R);
        }
        
    }
    else {
        printf("Couldn't allocate memory for bitmap buffer, tried to allocate %u bytes!\n", BufferSize);
    }
    
    
    free(Bitmap);
    
    if(Records != NULL){
        Records = (nar_record*)realloc(Records, RecordCount*sizeof(nar_record));
        (*OutRecordCount) = RecordCount;
    }
    else{
        (*OutRecordCount) = 0;
    }
    
    return Records;
}


BOOLEAN
SetFullRecords(volume_backup_inf* V) {
    
    //UINT* ClusterIndices = 0;
    
    uint32_t Count = 0;
    V->Stream.Records.Data  = GetVolumeRegionsFromBitmap(V->Stream.Handle, &Count);
    V->Stream.Records.Count = Count;
    
    return (V->Stream.Records.Data != NULL);
}

BOOLEAN
SetIncRecords(HANDLE CommPort, volume_backup_inf* V) {
    
    TIMED_BLOCK();
    
    BOOLEAN Result = FALSE;
    if(V == NULL){
        printf("SetIncRecords: VolInf was null\n");
        return FALSE;
    }
    
    V->PossibleNewBackupRegionOffsetMark = NarGetLogFileSizeFromKernel(CommPort, V->Letter);
    DWORD TargetReadSize = (DWORD)(V->PossibleNewBackupRegionOffsetMark - V->IncLogMark.LastBackupRegionOffset);
    
    V->Stream.Records.Data = 0;
    V->Stream.Records.Count = 0;
    
    std::wstring logfilepath = GenerateLogFilePath(V->Letter);
    HANDLE LogHandle = CreateFileW(logfilepath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
    
    // calling malloc with 0 is undefined behaviour, but not having logs is possible thing that might happen and no need to worry about it since
    // setupstream will include mft to stream, stream will not be empty.
    if(TargetReadSize != 0){
        
        if(NarSetFilePointer(LogHandle, (uint64_t)V->IncLogMark.LastBackupRegionOffset)){
            
            V->Stream.Records.Data = (nar_record*)malloc(TargetReadSize);
            V->Stream.Records.Count = TargetReadSize/sizeof(nar_record);
            DWORD BytesRead = 0;
            
            if(ReadFile(LogHandle, V->Stream.Records.Data, TargetReadSize, &BytesRead, 0) && BytesRead == TargetReadSize){
                Result = TRUE;
            }
            else{
                printf("SetIncRecords Couldnt read %lu, instead read %lu\n", TargetReadSize, BytesRead);
                DisplayError(GetLastError());
                
                free(V->Stream.Records.Data);
                V->Stream.Records.Count = 0;
            }
            
        }
        else{
            printf("Couldnt set file pointer\n");
        }
        
        if(Result == TRUE){
            qsort(V->Stream.Records.Data, V->Stream.Records.Count, sizeof(nar_record), CompareNarRecords);
            MergeRegions(&V->Stream.Records);
        }
        
    }
    else{
        V->Stream.Records.Data = 0;
        V->Stream.Records.Count = 0;
        Result = TRUE;
    }
    
    return Result;
}



ULONGLONG
NarGetLogFileSizeFromKernel(HANDLE CommPort, char Letter){
    
    ULONGLONG Result = 0;
    
    NAR_COMMAND Cmd = {};
    Cmd.Letter = Letter;
    Cmd.Type = NarCommandType_FlushLog;
    
    if(NarGetVolumeGUIDKernelCompatible(Letter, &Cmd.VolumeGUIDStr[0])){
        
        DWORD BR = 0;
        NAR_LOG_INF Respond = {0};
        
        HRESULT hResult = FilterSendMessage(CommPort, &Cmd, sizeof(NAR_COMMAND), &Respond, sizeof(Respond), &BR);
        if(SUCCEEDED(hResult) && FALSE == Respond.ErrorOccured){
            Result = Respond.CurrentSize;
        }
        else{
            printf("FilterSendMessage failed, couldnt remove volume from kernel side, err %i\n", hResult);
            DisplayError(GetLastError());
        }
        
    }
    else{
        
    }
    
    return Result;
}




BOOLEAN
SetDiffRecords(HANDLE CommPort ,volume_backup_inf* V) {
    
    TIMED_BLOCK();
    
    if(V == NULL) return FALSE;
    
    printf("Entered SetDiffRecords\n");
    BOOLEAN Result = FALSE;
    
    V->PossibleNewBackupRegionOffsetMark = NarGetLogFileSizeFromKernel(CommPort, V->Letter);
    std::wstring logfilepath = GenerateLogFilePath(V->Letter);
    HANDLE LogHandle = CreateFileW(logfilepath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
    
    
    if(LogHandle == INVALID_HANDLE_VALUE){
        DisplayError(GetLastError());
    }
    
    printf("Logname : %S\n", logfilepath.c_str());
    
    // NOTE (Batuhan): IMPORTANT, so if total log size exceeds 4GB, we cant backup succcessfully, since reading file larger than 4GB is 
    // problem by itself, one can do partially load file and continuously compress it via qsort & mergeregions. then final log size drastically
    // becames lower.
    
    // not so safe to truncate
    DWORD TargetReadSize = (DWORD)(V->PossibleNewBackupRegionOffsetMark - V->DiffLogMark.BackupStartOffset); 
    
    // calling malloc with 0 is not a good idea
    if(TargetReadSize != 0){
        
        if(NarSetFilePointer(LogHandle, V->DiffLogMark.BackupStartOffset)){
            
            V->Stream.Records.Data = (nar_record*)malloc(TargetReadSize);
            V->Stream.Records.Count = TargetReadSize / sizeof(nar_record);
            DWORD BytesRead = 0;
            if(ReadFile(LogHandle, V->Stream.Records.Data, TargetReadSize, &BytesRead,0) && BytesRead == TargetReadSize){
                printf("Succ read diff records, will sort and merge regions to save in space\n");
                Result = TRUE;
            }
            else{
                printf("SetDiffRecords Couldnt read %lu, instead read %lu\n", TargetReadSize, BytesRead);
                DisplayError(GetLastError());
                
                free(V->Stream.Records.Data);
                V->Stream.Records.Count = 0;
            }
            
        }
        else{
            printf("Couldnt set file pointer to beginning of the file\n");
        }
        
        if(Result == TRUE){
            qsort(V->Stream.Records.Data, V->Stream.Records.Count, sizeof(nar_record), CompareNarRecords);
            MergeRegions(&V->Stream.Records);
        }
        
    }
    else{
        V->Stream.Records.Data = 0;
        V->Stream.Records.Count = 0;
        Result = TRUE;
    }
    
    CloseHandle(LogHandle);
    
    return Result;
}




BOOLEAN
SetupVSS() {
    /* 
        NOTE(Batuhan): in managed code we dont need to initialize these stuff. since i am shipping code as textual to wrapper, i can detect clr compilation and switch to correct way to initialize
        vss stuff
     */
    
#if 1
#if (_MANAGED == 1) || (_M_CEE == 1)
    return TRUE;
#else
    BOOLEAN Return = TRUE;
    HRESULT hResult = 0;
    
    // TODO (Batuhan): Remove that thing if 
    hResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (!SUCCEEDED(hResult)) {
        printf("Failed CoInitialize function %d\n", hResult);
        DisplayError(GetLastError());
        Return = FALSE;
    }
    
    hResult = CoInitializeSecurity(
                                   NULL,                           //  Allow *all* VSS writers to communicate back!
                                   -1,                             //  Default COM authentication service
                                   NULL,                           //  Default COM authorization service
                                   NULL,                           //  reserved parameter
                                   RPC_C_AUTHN_LEVEL_PKT_PRIVACY,  //  Strongest COM authentication level
                                   RPC_C_IMP_LEVEL_IMPERSONATE,    //  Minimal impersonation abilities
                                   NULL,                           //  Default COM authentication settings
                                   EOAC_DYNAMIC_CLOAKING,          //  Cloaking
                                   NULL                            //  Reserved parameter
                                   );
    
    if (!SUCCEEDED(hResult)) {
        printf("Failed CoInitializeSecurity function %d\n", hResult);
        DisplayError(GetLastError());
        Return = FALSE;
    }
    return Return;
#endif
#endif
    
}


BOOLEAN
ConnectDriver(PLOG_CONTEXT Ctx) {
    BOOLEAN Result = FALSE;
    HRESULT hResult = FALSE;
    
    DWORD PID = GetCurrentProcessId();
    NAR_CONNECTION_CONTEXT CTX = { 0 };
    
    char Buffer[1024];
    GetWindowsDirectoryA(Buffer, 1024);
    char VolumePath[] = " :";
    VolumePath[0] = Buffer[0];
    char DeviceName[128];
    QueryDosDeviceA(VolumePath, DeviceName, 128);
    char* IntPtr = DeviceName;
    while (IsNumeric(*IntPtr) && *(++IntPtr) != '\0');
    CTX.OsDeviceID = atoi(IntPtr);
    CTX.PID = PID;
    CTX.OsDeviceID = Buffer[0];
    DWORD BytesWritten = 256;
    //GetUserNameW(&CTX.UserName[0], &BytesWritten) && BytesWritten != 0
    
    {
        hResult = FilterConnectCommunicationPort(MINISPY_PORT_NAME,
                                                 0,
                                                 &CTX, sizeof(CTX),
                                                 NULL, &Ctx->Port);
        
        if (!IS_ERROR(hResult)) {
            Result = TRUE;
        }
        else {
            printf("Could not connect to filter: 0x%08x\n", hResult);
            printf("Program PID is %d\n", PID);
            DisplayError((DWORD)hResult);
        }
        
    }
    
    
    return Result;
}



inline BOOLEAN
NarRemoveLetter(char Letter){
    char Buffer[128];
    
    snprintf(Buffer, sizeof(128), 
             "select volume %c\n"
             "remove letter %c\n"
             "exit\n", Letter, Letter);
    
    
    char InputFN[] = "NARDPRLINPUT";
    if(NarDumpToFile(InputFN, Buffer, (unsigned int)strlen(Buffer))){
        snprintf(Buffer, sizeof(Buffer), "diskpart /s %s", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    
    printf("COULDNT DUMP CONTENTS TO FILE, RETURNING FALSE CODE (NARREMOVELETTER)\n");
    return FALSE;
}

inline BOOLEAN
NarFormatVolume(char Letter) {
    
    char Buffer[2096];
    memset(Buffer, 0, 2096);
    
    snprintf(Buffer, sizeof(Buffer), 
             ""
             "select volume %c\n"
             "format fs = ntfs quick override\n"
             "exit\n", Letter);
    
    char InputFN[] = "NARDPFORMATVOLUME";
    if (NarDumpToFile(InputFN, Buffer, (unsigned int)strlen(Buffer))) {
        snprintf(Buffer, sizeof(Buffer), "diskpart /s %s", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    
    return FALSE;
}

inline void
NarRepairBoot(char OSVolumeLetter, char BootPartitionLetter) {
    char Buffer[128];
    snprintf(Buffer, sizeof(Buffer), 
             "bcdboot %c:\\Windows /s %c: /f ALL", OSVolumeLetter, BootPartitionLetter);
    system(Buffer);
}




BOOLEAN
NarSetVolumeSize(char Letter, int TargetSizeMB) {
    BOOLEAN Result = 0;
    int VolSizeMB = 0;
    char Buffer[1024];
    char FNAME[] = "NARDPSCRPT";
    snprintf(Buffer, sizeof(Buffer), "%c:\\", Letter);
    
    ULARGE_INTEGER TOTAL_SIZE = { 0 };
    GetDiskFreeSpaceExA(Buffer, 0, &TOTAL_SIZE, 0);
    VolSizeMB = (int)(TOTAL_SIZE.QuadPart / (1024ull * 1024ull)); // byte to MB
    
    printf("Volume %c's size is %i, target size %i\n", Letter, VolSizeMB, TargetSizeMB);
    
    if (VolSizeMB == TargetSizeMB) {
        return TRUE;
    }
    
    /*
  extend size = X // extends volume by X. Doesnt resize, just adds X
  shrink desired = X // shrink volume to X, if X is bigger than current size, operation fails
  */
    if (VolSizeMB > TargetSizeMB) {
        // shrink volume
        ULONGLONG Diff = (unsigned int)(VolSizeMB - TargetSizeMB);
        snprintf(Buffer, sizeof(Buffer), "select volume %c\nshrink desired = %I64u\nexit\n", Letter, Diff);
    }
    else if(VolSizeMB < TargetSizeMB){
        // extend volume
        ULONGLONG Diff = (unsigned int)(TargetSizeMB - VolSizeMB);
        snprintf(Buffer, sizeof(Buffer), "select volume %c\nextend size = %I64u\nexit\n", Letter, Diff);
    }
    else {
        // VolSizeMB == TargetSizeMB
        // do nothing
        return TRUE;
    }
    
    //NarDumpToFile(const char *FileName, void* Data, int Size)
    if (NarDumpToFile(FNAME, Buffer, (uint32_t)strlen(Buffer))) {
        char CMDBuffer[128];
        snprintf(CMDBuffer, sizeof(CMDBuffer), "diskpart /s %s", FNAME);
        system(CMDBuffer);
        // TODO(Batuhan): maybe check output
        Result = TRUE;
    }
    
    return Result;
}

BOOLEAN
CreatePartition(int Disk, char Letter, unsigned size) {
    BOOLEAN Result = FALSE;
    char Buffer[1024];
    
    if (Letter >= 'a' && Letter <= 'z') Letter += ('A' - 'a'); //convert to uppercase
    
    DWORD Drives = GetLogicalDrives();
    if (Drives & (1 << (Letter - 'A'))) {
        printf("Volume exists in system\n");
        return FALSE;
    }
    snprintf(Buffer, sizeof(Buffer), "select disk %i\n"
             "create partition primary size = %u\n"
             "format fs = \"ntfs\" quick"
             "assign letter = \"%c\"\n", Disk, size, Letter);
    char FileName[] = "NARDPSCRPT";
    
    if (NarDumpToFile(FileName, Buffer, (unsigned int)strlen(Buffer))) {
        char CMDBuffer[128];
        snprintf(CMDBuffer, sizeof(CMDBuffer), "diskpart /s %s", FileName);
        system(CMDBuffer);
        // TODO(Batuhan): maybe check output
        Result = TRUE;
    }
    
    return Result;
}


// returns # of disks, returns 0 if information doesnt fit in array
data_array<disk_information>
NarGetDisks() {
    
    data_array<disk_information> Result = { 0, 0 };
    Result.Data = (disk_information*)malloc(sizeof(disk_information) * 32);
    Result.Count = 32;
    memset(Result.Data, 0, sizeof(disk_information) * 32);
    
    DWORD DriveLayoutSize = 1024 * 4;
    int DGEXSize = 1024 * 4;
    
    DWORD MemorySize = DriveLayoutSize + DGEXSize;
    void* Memory = VirtualAlloc(0, MemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    
    DRIVE_LAYOUT_INFORMATION_EX* DriveLayout = (DRIVE_LAYOUT_INFORMATION_EX*)(Memory);
    DISK_GEOMETRY_EX* DGEX = (DISK_GEOMETRY_EX*)((char*)Memory + DriveLayoutSize);    
    
    
    char StrBuf[255];
    int Found = 0;
    DWORD HELL;
    for (unsigned int DiskID = 0; DiskID < 32; DiskID++){
        
        snprintf(StrBuf, sizeof(StrBuf), "\\\\?\\PhysicalDrive%i", DiskID);
        HANDLE Disk = CreateFileA(StrBuf, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0,0);
        if(Disk == INVALID_HANDLE_VALUE){
            continue;
        }
        
        Result.Data[Found].ID = (int)DiskID;
        
        if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DriveLayout, DriveLayoutSize, &HELL, 0)) {
            
            if (DriveLayout->PartitionStyle == PARTITION_STYLE_MBR) {
                Result.Data[Found].Type = NAR_DISKTYPE_MBR;
            }
            if (DriveLayout->PartitionStyle == PARTITION_STYLE_GPT) {
                Result.Data[Found].Type = NAR_DISKTYPE_GPT;
            }
            if (DriveLayout->PartitionStyle == PARTITION_STYLE_RAW) {
                Result.Data[Found].Type = NAR_DISKTYPE_RAW;
            }
            
            if(DriveLayout->PartitionStyle != PARTITION_STYLE_RAW
               && DriveLayout->PartitionStyle != PARTITION_STYLE_GPT
               && DriveLayout->PartitionStyle != PARTITION_STYLE_MBR){
                printf("Drive layout style was undefined, %s\n", StrBuf);
                continue;
            }
            
        }
        else {
            printf("Can't get drive layout for disk %s\n", StrBuf);
        }
        
        if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, 0, 0, DGEX, (DWORD)DGEXSize, &HELL, 0)) {
            Result.Data[Found].Size = (ULONGLONG)DGEX->DiskSize.QuadPart;
        }
        else {
            printf("Can't get drive layout for disk %s\n", StrBuf);
        }
        
        CloseHandle(Disk);
        memset(Memory, 0, MemorySize);
        Found++;
        
    }
    
    
    VirtualFree(Memory, 0, MEM_RELEASE);
    Result.Count = Found;
    if (Found) {
        Result.Data = (disk_information*)realloc(Result.Data, sizeof(Result.Data[0]) * Result.Count);
    }
    else {
        free(Result.Data);
        Result.Data = 0;
    }
    
    return Result;
    
}


//Returns # of volumes detected
data_array<volume_information>
NarGetVolumes() {
    
    data_array<volume_information> Result = { 0,0 };
    wchar_t VolumeString[] = L"!:\\";
    char WindowsLetter = 'C';
    {
        char WindowsDir[512];
        GetWindowsDirectoryA(WindowsDir, 512);
        WindowsLetter = WindowsDir[0];
    }
    
    DWORD Drives = GetLogicalDrives();
    
    for (int CurrentDriveIndex = 0; CurrentDriveIndex < 26; CurrentDriveIndex++) {
        
        if (Drives & (1 << CurrentDriveIndex)) {
            
            VolumeString[0] = (wchar_t)('A' + (char)CurrentDriveIndex);
            ULARGE_INTEGER TotalSize = { 0 };
            ULARGE_INTEGER FreeSize = { 0 };
            
            volume_information T = { 0 };
            
            if (GetDiskFreeSpaceExW(VolumeString, 0, &TotalSize, &FreeSize)) {
                T.Letter = 'A' + (char)CurrentDriveIndex;
                T.TotalSize = TotalSize.QuadPart;
                T.FreeSize = FreeSize.QuadPart;
                
                T.Bootable = (WindowsLetter == T.Letter);
                T.DiskType = (char)NarGetVolumeDiskType(T.Letter);
                T.DiskID = NarGetVolumeDiskID(T.Letter);
                
                {
                    WCHAR fileSystemName[MAX_PATH + 1] = { 0 };
                    DWORD serialNumber = 0;
                    DWORD maxComponentLen = 0;
                    DWORD fileSystemFlags = 0;
                    GetVolumeInformationW(VolumeString, T.VolumeName, sizeof(T.VolumeName), &serialNumber, &maxComponentLen, &fileSystemFlags, fileSystemName, sizeof(fileSystemName));
                }
                
                Result.Insert(T);
            }
            
            
        }
        
    }
    
    return Result;
    
}



ULONGLONG
NarGetDiskTotalSize(int DiskID) {
    
    ULONGLONG Result = 0;
    DWORD Temp = 0;
    char StrBuffer[128];
    snprintf(StrBuffer, sizeof(StrBuffer), "\\\\?\\PhysicalDrive%i", DiskID);
    
    unsigned int DGEXSize = 1024 * 1; // 1 KB
    DISK_GEOMETRY_EX* DGEX = (DISK_GEOMETRY_EX*)malloc(DGEXSize);
    if (DGEX != NULL) {
        HANDLE Disk = CreateFileA(StrBuffer,
                                  GENERIC_WRITE | GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        
        if (Disk != INVALID_HANDLE_VALUE) {
            if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, 0, 0, DGEX, DGEXSize, &Temp, 0)) {
                Result = (ULONGLONG)DGEX->DiskSize.QuadPart;
            }
            else {
                printf("Failed to call DeviceIoControl with IOCTL_DISK_GET_DRIVE_GEOMETRY_EX parameter for disk %i\n", DiskID);
            }
        }
        else {
            printf("Can't open %s\n", StrBuffer);
        }
        
        CloseHandle(Disk);
        free(DGEX);    	
    }
    else{
    	printf("Unable to allocate memory for DISK_GEOMETYRY_EX, for disk %d\n", DiskID);
    }
    
    
    return Result;
}


inline BOOLEAN
NarCreateCleanGPTBootablePartition(int DiskID, int VolumeSizeMB, int EFISizeMB, int RecoverySizeMB, char VolumeLetter, char BootVolumeLetter) {
    
    char Buffer[2096];
    int BufferIndex = 0;
    memset(Buffer, 0, 2096);
    
    BufferIndex += snprintf(Buffer, sizeof(Buffer), 
                            "select disk %i\n" // ARG0 DiskID
                            "clean\n"
                            "convert gpt\n" // format disk as gpt
                            "select partition 1\n"
                            "delete partition override\n", DiskID);
    
    if(0 != RecoverySizeMB){
        BufferIndex += snprintf(&Buffer[BufferIndex], sizeof(Buffer) - BufferIndex,
                                "create partition primary size = %i\n"
                                "assign letter %c\n"
                                "format fs = ntfs quick\n"
                                "remove letter %c\n"
                                "set id = \"de94bba4-06d1-4d40-a16a-bfd50179d6ac\"\n"
                                "gpt attributes = 0x8000000000000001\n", 
                                RecoverySizeMB, BootVolumeLetter, BootVolumeLetter);
    }
    else{
        printf("Recovery partition's size was zero, skipping it\n");
    }
    
    BufferIndex += snprintf(&Buffer[BufferIndex], sizeof(Buffer) - BufferIndex,
                            "create partition efi size = %i\n"
                            "format fs = fat32 quick\n"
                            "assign letter %c\n"
                            "create partition msr size = 16\n" // win10
                            "create partition primary size = %i\n"
                            "assign letter = %c\n"
                            "format fs = \"ntfs\" quick\n"
                            "exit\n", 
                            EFISizeMB, 
                            BootVolumeLetter,
                            VolumeSizeMB,
                            VolumeLetter);
    
    
    char InputFN[] = "NARDPINPUT";
    // NOTE(Batuhan): safe conversion
    if (NarDumpToFile(InputFN, Buffer, (INT32)strlen(Buffer))) {
        snprintf(Buffer, sizeof(Buffer), "diskpart /s %s > NARDPOUTPUT.txt", InputFN);
        system(Buffer);
        printf("%s\n", Buffer);
        return TRUE;
    }
    
    return FALSE;
    
}

inline BOOLEAN
NarCreateCleanGPTPartition(int DiskID, int VolumeSizeMB, char Letter) {
    char Buffer[2096];
    memset(Buffer, 0, 2096);
    
    
    // Convert MB to byte
    if (NarGetDiskTotalSize(DiskID) < (ULONGLONG)VolumeSizeMB * 1024 * 1024) {
        printf("Not enough space to create partition\n");
        return FALSE;
    }
    
    
    snprintf(Buffer, sizeof(Buffer),
             ""
             "select disk %i\n"
             "clean\n"
             "convert gpt\n" // format disk as gpt
             "create partition primary size = %i\n"
             "assign letter = %c\n"
             "format fs = \"ntfs\" quick\n"
             "exit\n", DiskID, VolumeSizeMB, Letter);
    
    
    char InputFN[] = "NARDPINPUT";
    if (NarDumpToFile(InputFN, Buffer, (unsigned int)strlen(Buffer))) {
        snprintf(Buffer, sizeof(Buffer),"diskpart /s %s > NARDPOUTPUT.txt", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    
    return FALSE;
}


BOOLEAN
NarCreateCleanMBRPartition(int DiskID, char VolumeLetter, int VolumeSize) {
    
    char Buffer[2048];
    snprintf(Buffer,sizeof(Buffer), 
             "select disk %i\n"
             "clean\n"
             "convert mbr\n"
             "create partition primary size %i\n"
             "format fs = ntfs quick\n"
             "assign letter %c\n"
             "exit\n",
             DiskID, VolumeSize, VolumeLetter
             );
    
    char InputFN[] = "NARDPINPUT";
    if (NarDumpToFile(InputFN, Buffer, (unsigned int)strlen(Buffer))) {
        snprintf(Buffer, sizeof(Buffer), "diskpart /s %s > NARDPOUTPUT.txt", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    
    
    return FALSE;
    
}


inline BOOLEAN
NarCreateCleanMBRBootPartition(int DiskID, char VolumeLetter, int VolumeSizeMB, int SystemPartitionSizeMB, int RecoveryPartitionSizeMB,
                               char BootPartitionLetter) {
    
    char Buffer[2048];
    memset(Buffer, 0, 2048);
    int indx = 0;
    indx = snprintf(Buffer, sizeof(Buffer), 
                    "select disk %i\n"
                    "clean\n"
                    "convert mbr\n"
                    // SYSTEM PARTITION
                    "create partition primary size = %i\n"
                    "assign letter %c\n"
                    "format quick fs = ntfs label = System\n"
                    "active\n"
                    // WINDOWS PARTITION
                    "create partition primary size %i\n" // in MB
                    "format quick fs = ntfs label = Windows\n"
                    "assign letter %c\n", 
                    DiskID, SystemPartitionSizeMB, BootPartitionLetter, VolumeSizeMB, VolumeLetter);
    
    if(0 != RecoveryPartitionSizeMB){
        indx += snprintf(Buffer, sizeof(Buffer)-indx,
                         "create partition primary size %i\n"
                         "set id = 27\n", 
                         RecoveryPartitionSizeMB);
    }
    
    snprintf(&Buffer[indx], sizeof(Buffer) - indx,
             "exit\n");
    
    char InputFN[] = "NARDPINPUT";
    if (NarDumpToFile(InputFN, Buffer, (unsigned int)strlen(Buffer))) {
        snprintf(Buffer, sizeof(Buffer), "diskpart /s %s > NARDPOUTPUT.txt", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    
    
    return FALSE;
    
}


inline BOOLEAN
NarIsOSVolume(char Letter) {
    char windir[256];
    GetWindowsDirectoryA(windir, 256);
    return windir[0] == Letter;
}

/*
Returns:
'R'aw
'M'BR
'G'PT
*/
inline int
NarGetVolumeDiskType(char Letter) {
    char VolPath[512];
    char Vol[] = "!:\\";
    Vol[0] = Letter;
    
    int Result = NAR_DISKTYPE_RAW;
    DWORD BS = 1024 * 8; //8 KB
    DWORD T = 0;
    
    void* Buf = VirtualAlloc(0, 2 * BS, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    VOLUME_DISK_EXTENTS* Ext = (VOLUME_DISK_EXTENTS*)(Buf);
    DRIVE_LAYOUT_INFORMATION_EX* DL = (DRIVE_LAYOUT_INFORMATION_EX*)((char*)Buf + BS);
    
    
    
    GetVolumeNameForVolumeMountPointA(Vol, VolPath, 512);
    VolPath[strlen(VolPath) - 1] = '\0'; //Remove trailing slash
    HANDLE Drive = CreateFileA(VolPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    HANDLE Disk = INVALID_HANDLE_VALUE;
    
    if (Drive != INVALID_HANDLE_VALUE) {
        
        if (DeviceIoControl(Drive, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0, Ext, BS, &T, 0)) {
            wchar_t DiskPath[512];
            // L"\\\\?\\PhysicalDrive%i";
            wsprintfW(DiskPath, L"\\\\?\\PhysicalDrive%i", Ext->Extents[0].DiskNumber);
            Disk = CreateFileW(DiskPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
            if (Disk != INVALID_HANDLE_VALUE) {
                
                if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DL, BS, &T, 0)) {
                    if (DL->PartitionStyle == PARTITION_STYLE_MBR) {
                        Result = NAR_DISKTYPE_MBR;
                    }
                    if (DL->PartitionStyle == PARTITION_STYLE_GPT) {
                        Result = NAR_DISKTYPE_GPT;
                    }
                    if (DL->PartitionStyle == PARTITION_STYLE_RAW) {
                        Result = NAR_DISKTYPE_RAW;
                    }
                }
                else {
                    printf("DeviceIOControl GET_DRIVE_LAYOUT_EX failed\n");
                    DisplayError(GetLastError());
                }
                
            }
            else {
                printf("Can't open disk as file\n");
            }
            
        }
        
        
    }
    else {
        printf("Cant open drive %c as file\n", Letter);
    }
    
    VirtualFree(Buf, 0, MEM_RELEASE);
    CloseHandle(Drive);
    CloseHandle(Disk);
    return Result;
}


inline unsigned char
NarGetVolumeDiskID(char Letter) {
    
    wchar_t VolPath[512];
    wchar_t Vol[] = L"!:\\";
    Vol[0] = Letter;
    
    unsigned char Result = (unsigned char)NAR_INVALID_DISK_ID;
    DWORD BS = 1024 * 2; //1 KB
    DWORD T = 0;
    
    VOLUME_DISK_EXTENTS* Ext = (VOLUME_DISK_EXTENTS*)VirtualAlloc(0, BS, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    
    if(NarGetVolumeGUIDKernelCompatible((wchar_t)Letter,VolPath)){
        
        HANDLE Drive = CreateFileW(VolPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        
        if (Drive != INVALID_HANDLE_VALUE) {
            
            if (DeviceIoControl(Drive, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0, Ext, BS, &T, 0)) {
                //wchar_t DiskPath[512];
                // L"\\\\?\\PhysicalDrive%i";
                Result = (unsigned char)Ext->Extents[0].DiskNumber;
            }
            else {
                printf("DeviceIoControl failed with argument VOLUME_GET_VOLUME_DISK_EXTENTS for volume %c\n", Letter);
            }
            
        }
        else {
            printf("Cant open drive %c as file\n", Letter);
        }
        
        CloseHandle(Drive);
        
    }
    else{
        printf("Couldnt get kernel compatible volume GUID\n");
        DisplayError(GetLastError());
    }
    
    VirtualFree(Ext, 0, MEM_RELEASE);
    
    return Result;
}

inline BOOLEAN
NarIsFullBackup(int Version) {
    return Version < 0;
}



inline BOOLEAN
NarSetFilePointer(HANDLE File, ULONGLONG V) {
    LARGE_INTEGER MoveTo = { 0 };
    MoveTo.QuadPart = V;
    LARGE_INTEGER NewFilePointer = { 0 };
    SetFilePointerEx(File, MoveTo, &NewFilePointer, FILE_BEGIN);
    return MoveTo.QuadPart == NewFilePointer.QuadPart;
}



HANDLE
NarOpenVolume(char Letter) {
    char VolumePath[64];
    snprintf(VolumePath, 64, "\\\\.\\%c:", Letter);
    
    HANDLE Volume = CreateFileA(VolumePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
    if (Volume != INVALID_HANDLE_VALUE) {
        
        
#if 0        
        if (DeviceIoControl(Volume, FSCTL_LOCK_VOLUME, 0, 0, 0, 0, 0, 0)) {
            
        }
        else {
            // NOTE(Batuhan): this isnt an error, tho prohibiting volume access for other processes would be great.
            printf("Couldn't lock volume %c\n", Letter);
        }
        
        
        if (DeviceIoControl(Volume, FSCTL_DISMOUNT_VOLUME, 0, 0, 0, 0, 0, 0)) {
            
        }
        else {
            // printf("Couldnt dismount volume\n");
        }
        
#endif
        
        
    }
    else {
        printf("Couldn't open volume %c\n", Letter);
        DisplayError(GetLastError());
    }
    
    return Volume;
}

void
NarCloseVolume(HANDLE V) {
    DeviceIoControl(V, FSCTL_UNLOCK_VOLUME, 0, 0, 0, 0, 0, 0);
    CloseHandle(V);
}


inline nar_backup_id
NarGenerateBackupID(char Letter){
    
    nar_backup_id Result = {0};
    
    SYSTEMTIME T;
    GetLocalTime(&T);
    Result.Year = T.wYear;
    Result.Month = T.wMonth;
    Result.Day = T.wDay;
    Result.Hour = T.wHour;
    Result.Min = T.wMinute;
    Result.Letter = Letter;
    
    return Result;
}




/*
Unlike generatemetadata, binary functions, this one generates absolute path of the log file. Which is 
under windows folder
C:\Windows\Log....
*/
inline std::wstring
GenerateLogFilePath(char Letter) {
    wchar_t NameTemp[]= L"NAR_LOG_FILE__.nlfx";
    NameTemp[13] = (wchar_t)Letter;
    
    std::wstring Result = L"C:\\Windows\\";
    Result += std::wstring(NameTemp);
    
    return Result;
}




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
    
    HANDLE F = CreateFile(FileName.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
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


ULONGLONG
NarGetVolumeTotalSize(char Letter) {
    char Temp[] = "!:\\";
    Temp[0] = Letter;
    ULARGE_INTEGER L = { 0 };
    GetDiskFreeSpaceExA(Temp, 0, &L, 0);
    return L.QuadPart;
}

ULONGLONG
NarGetVolumeUsedSize(char Letter){
    char Temp[] = "!:\\";
    Temp[0] = Letter;
    ULARGE_INTEGER total = { 0 };
    ULARGE_INTEGER free = { 0 };
    GetDiskFreeSpaceExA(Temp, 0, &total, &free);
    return (total.QuadPart - free.QuadPart);
}

/*
Version: indicates full backup if <0
ClusterSize: default 4096
BackupRegions: Must have, this data determines how i must map binary data to the volume at restore operation

// NOTE(Batuhan): file that contains this struct contains:
-- RegionMetadata:
-- MFTMetadata: (optional)
-- MFT: (optional)
-- Recovery: (optional)
*/
BOOLEAN
SaveMetadata(char Letter, int Version, int ClusterSize, BackupType BT,
             data_array<nar_record> BackupRegions, HANDLE VSSHandle, nar_record* MFTLCN, unsigned int MFTLCNCount, nar_backup_id ID, bool IsCompressed) {
    
    // TODO(Batuhan): convert letter to uppercase
    //Letter += ('A' - 'a');
    
    BOOLEAN IsMFTLCNInternal = FALSE;
    if (ClusterSize <= 0 || ClusterSize % 512 != 0) {
        return FALSE;
    }
    if (Letter < 'A' || Letter > 'Z') {
        return FALSE;
    }
    
    {
        DWORD Len = 128;
        wchar_t bf[128];
        memset(bf, 0, 128);
        GetCurrentDirectoryW(Len, &bf[0]);
        printf("Current working directory is %S\n", bf);
    }
    
    DWORD BytesWritten = 0;
    backup_metadata BM = { 0 };
    ULONGLONG BaseOffset = sizeof(BM);
    
    BOOLEAN Result = FALSE;
    char StringBuffer[1024];
    
    std::wstring MetadataFilePath;
    GenerateMetadataName(ID, Version, MetadataFilePath);
    
    HANDLE MetadataFile = CreateFileW(MetadataFilePath.c_str(), GENERIC_WRITE | GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0);
    if (MetadataFile == INVALID_HANDLE_VALUE) {
        printf("Couldn't create metadata file : %s\n", MetadataFilePath.c_str());
        goto Exit;
    }
    
    printf("Metadata name generated %S\n", MetadataFilePath.c_str());
    
    if(FALSE == NarSetFilePointer(MetadataFile, sizeof(BM))){
        printf("Couldnt reserve bytes for metadata struct!\n");
        goto Exit;
    }
    
    BM.MetadataInf.Size = sizeof(BM);
    BM.MetadataInf.Version = GlobalBackupMetadataVersion;
    
    BM.Version = Version;
    BM.ClusterSize = ClusterSize;
    BM.BT = BT;
    BM.Letter = Letter;
    BM.DiskType = (unsigned char)NarGetVolumeDiskType(BM.Letter);
    
    BM.IsOSVolume = NarIsOSVolume(Letter);
    BM.VolumeTotalSize = NarGetVolumeTotalSize(BM.Letter);
    BM.VolumeUsedSize = NarGetVolumeUsedSize(BM.Letter);
    BM.ID = ID;
    BM.IsCompressed = IsCompressed;
    BM.FrameSize = NAR_COMPRESSION_FRAME_SIZE;
    
    NarGetProductName(BM.ProductName);
    DWORD Len = sizeof(BM.ComputerName);
    if(GetComputerNameA(&BM.ComputerName[0], &Len) == FALSE){
        printf("Unable to query computer name\n");
    }
    
    {
        GetLocalTime(&BM.BackupDate);
        memset(&BM.Size, 0, sizeof(BM.Size));
        memset(&BM.Offset, 0, sizeof(BM.Offset));
        memset(&BM.Errors, 0, sizeof(BM.Errors));
    }
    
    // NOTE(Batuhan): fill BM.Size struct
    // NOTE(Batuhan): Backup regions and it's metadata sizes
    {
        BM.Size.RegionsMetadata = BackupRegions.Count * sizeof(nar_record);
        for (size_t i = 0; i < BackupRegions.Count; i++) {
            BM.Size.Regions += (ULONGLONG)BackupRegions.Data[i].Len * BM.ClusterSize;
        }
        BM.VersionMaxWriteOffset = ((ULONGLONG)BackupRegions.Data[BackupRegions.Count - 1].StartPos + BackupRegions.Data[BackupRegions.Count - 1].Len)*BM.ClusterSize;
    }
    
    /*
    // NOTE(Batuhan): MFT metadata and it's binary sizes, checks if non-fullbackup, otherwise skips it
    Since it metadata file must contain MFT for non-full backups, we forwardly declare it's size here, then try to
    write it to the file, if any error occurs during this operation, BM.Size.MFT wil be corrected and marked as corrupt
    @BM.Errors.MFT
      */
    
    if(MFTLCN == NULL){
        // TODO(Batuhan): null check
        MFTLCN = NarGetMFTRegionsByCommandLine(BM.Letter, &MFTLCNCount);
        IsMFTLCNInternal = TRUE;
    }
    
    
    if (MFTLCN != 0) {
        BM.Size.MFTMetadata = MFTLCNCount * sizeof(nar_record);
        for (size_t i = 0; i < MFTLCNCount; i++) {
            BM.Size.MFT += (ULONGLONG)MFTLCN[i].Len * (ULONGLONG)BM.ClusterSize;
        }
    }
    else {
        // NOTE(Batuhan): Couldn't fetch MFT LCN's
        BM.Errors.MFTMetadata = TRUE;
        BM.Errors.MFT = TRUE;
        printf("Couldn't fetch MFT LCN's\n");
    }
    
    
    {
        // TODO(Batuhan): same problem mentioned in the codebase, we have to figure out reading more than 4gb at once(easy, but gotta replace lots of code probably)
        WriteFile(MetadataFile, BackupRegions.Data, BM.Size.RegionsMetadata, &BytesWritten, 0);
        if (BytesWritten != BM.Size.RegionsMetadata) {
            printf("Couldn't save regionsmetata to file\n");
            BM.Errors.RegionsMetadata = TRUE;
            BM.Size.RegionsMetadata = BytesWritten;
        }
    }
    
    
    // NOTE(Batuhan): since MFT size is > 0 if only non-full backup, we dont need to check Version < 0
    if (BM.Size.MFT && !BM.Errors.MFT && MFTLCN != NULL) {
        
        //Save current file pointer in case of failure, so we can roll back to old pointer.
        ULONGLONG OldFilePointer = NarGetFilePointer(MetadataFile);
        if (WriteFile(MetadataFile, MFTLCN, BM.Size.MFTMetadata, &BytesWritten, 0) && BytesWritten == BM.Size.MFTMetadata) {
            OldFilePointer = NarGetFilePointer(MetadataFile);
            
            data_array<nar_record> temp = {MFTLCN, MFTLCNCount};
            if (AppendMFTFile(MetadataFile, VSSHandle, temp, ClusterSize)) {
                printf("Successfully appended MFT to metadata\n");
                // NOTE(Batuhan): successfully appended mft to the metadata file
            }
            else {
                BM.Errors.MFT = TRUE;
                if (NarTruncateFile(MetadataFile, OldFilePointer)) {
                    // NOTE(Batuhan): successfully truncated file, we are safe to set MFT size to 0
                    BM.Size.MFT = 0;
                }
                else {
                    // NOTE(Batuhan): couldnt even truncate file, so set size to how many bytes has written @AppendMFTFile
                    ULONGLONG NewFilePointer = NarGetFilePointer(MetadataFile);
                    BM.Size.MFT = OldFilePointer - NewFilePointer;
                }
            }
        }
        else {
            printf("Couldn't append MFTMetadata to file\n");
            // NOTE(Batuhan) : Couldnt append MFTMetadata to file
            BM.Errors.MFTMetadata = TRUE;
            BM.Errors.MFT = TRUE;
            
            // Try to truncate file to old position,
            if (NarTruncateFile(MetadataFile, OldFilePointer)) {
                BM.Size.MFTMetadata = 0;
            }
            else {
                ULONGLONG NewFilePointer = NarGetFilePointer(MetadataFile);
                BM.Size.MFTMetadata = OldFilePointer - NewFilePointer;
            }
            
        }
        
    }
    else {
        // NOTE(Batuhan): that's not an error
    }
    
    
    /*
  layout of disks (fundamental level, gpt might contain extra data partitions)
  for extra information visit:
  MBR https://docs.microsoft.com/en-us/windows-hardware/manufacture/desktop/configure-biosmbr-based-hard-drive-partitions  
  GPT https://docs.microsoft.com/en-us/windows-hardware/manufacture/desktop/configure-uefigpt-based-hard-drive-partitions
  
    GPT
  System
  Msr
  windows
  recovery
  
  
  MBR
  system
  windows
  recovery
  
    */
    // NOTE(Batuhan): Recovery partition's data is stored on metadata only if it's both full and OS volume backup
    
    
    
#if 1
    
    if (BM.IsOSVolume && BM.Version == NAR_FULLBACKUP_VERSION) {
        
        // recovery partition exists for both GPT and MBR, AppendRecoveryToFile can detect disk type and append it accordingly
        ULONGLONG OldFilePointer = NarGetFilePointer(MetadataFile);
        if (AppendRecoveryToFile(MetadataFile, BM.Letter)) {
            ULONGLONG CurrentFilePointer = NarGetFilePointer(MetadataFile);
            BM.Size.Recovery = CurrentFilePointer - OldFilePointer;
            printf("Appended recovery partition to metadata, size %I64d\n", BM.Size.Recovery);
        }
        else {
            printf("Error occured while appending recovery partition to backup metadata file for volume %c\n", BM.Letter);
            ULONGLONG CurrentFilePointer = NarGetFilePointer(MetadataFile);
            BM.Size.Recovery = CurrentFilePointer - OldFilePointer;
            BM.Errors.Recovery = TRUE;
            Result = FALSE;
        }
        
    }
    
    if(BM.IsOSVolume){
        
        GUID GUIDMSRPartition = { 0 }; // microsoft reserved partition guid
        GUID GUIDSystemPartition = { 0 }; // efi-system partition guid
        GUID GUIDRecoveryPartition = { 0 }; // recovery partition guid
        
        StrToGUID("{e3c9e316-0b5c-4db8-817d-f92df00215ae}", &GUIDMSRPartition);
        StrToGUID("{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}", &GUIDSystemPartition);
        StrToGUID("{de94bba4-06d1-4d40-a16a-bfd50179d6ac}", &GUIDRecoveryPartition);
        
        
        int DiskID = NarGetVolumeDiskID(BM.Letter);
        char DiskPath[128];
        snprintf(DiskPath, sizeof(DiskPath), "\\\\?\\PhysicalDrive%i", DiskID);
        HANDLE DiskHandle = CreateFileA(DiskPath, GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        if(DiskHandle != INVALID_HANDLE_VALUE){
            
            DWORD DLSize = 1024*2;
            DRIVE_LAYOUT_INFORMATION_EX *DL = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(DLSize);
            DWORD Hell = 0;
            if (DeviceIoControl(DiskHandle, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DL, DLSize, &Hell, 0)) {
                
                if (DL->PartitionStyle == PARTITION_STYLE_GPT) {
                    
                    for (unsigned int PartitionIndex = 0; PartitionIndex < DL->PartitionCount; PartitionIndex++) {
                        
                        PARTITION_INFORMATION_EX* PI = &DL->PartitionEntry[PartitionIndex];
                        // NOTE(Batuhan): Finding recovery partition via GUID
                        if (IsEqualGUID(PI->Gpt.PartitionType, GUIDRecoveryPartition)) {
                            BM.Size.Recovery = PI->PartitionLength.QuadPart;
                        }
                        if(IsEqualGUID(PI->Gpt.PartitionType, GUIDSystemPartition)){
                            BM.GPT_EFIPartitionSize = PI->PartitionLength.QuadPart;
                        }
                        
                    }
                    
                }
                else if(DL->PartitionStyle == PARTITION_STYLE_MBR){
                    
                    for (unsigned int PartitionIndex = 0; PartitionIndex < DL->PartitionCount; ++PartitionIndex){
                        
                        PARTITION_INFORMATION_EX* PI = &DL->PartitionEntry[PartitionIndex];
                        // System partition
                        if (PI->Mbr.BootIndicator == TRUE) {
                            BM.MBR_SystemPartitionSize = PI->PartitionLength.QuadPart;
                            break;
                        }
                        
                    }
                    
                }
                
                
            }
            else {
                printf("DeviceIoControl with argument IOCTL_DISK_GET_DRIVE_LAYOUT_EX failed for drive %i, cant append recovery file to backup metadata\n", DiskID, Letter);
                DisplayError(GetLastError());
            }
            
            free(DL);
            CloseHandle(DiskHandle);
            
        }
        else{
            printf("Couldn't open disk %i as file, for volume %c's metadata @ version %i\n", DiskID, BM.Letter, BM.Version);
        }
        
    }
    
#endif
    
    /*
    // NOTE(Batuhan): fill BM.Offset struct
    offset[n] = offset[n-1] + size[n-1]
  offset[0] = baseoffset = struct size
  */
    
    BM.Offset.RegionsMetadata = BaseOffset;
    BM.Offset.MFTMetadata = BM.Offset.RegionsMetadata + BM.Size.RegionsMetadata;
    BM.Offset.MFT = BM.Offset.MFTMetadata + BM.Size.MFTMetadata;
    BM.Offset.Recovery = BM.Offset.MFT + BM.Size.MFT;
    
    SetFilePointer(MetadataFile, 0, 0, FILE_BEGIN);
    WriteFile(MetadataFile, &BM, sizeof(BM), &BytesWritten, 0);
    if (BytesWritten == sizeof(BM)) {
        Result = TRUE;
    }
    else{
        printf("Couldnt write %i bytes to metadata, instead written %i bytes\n", sizeof(BM), BytesWritten);
        DisplayError(GetLastError());
    }
    
    Exit:
    
    CloseHandle(MetadataFile);
    if(IsMFTLCNInternal){
        NarFreeMFTRegionsByCommandLine(MFTLCN);
    }
    
    
    printf("SaveMetadata returns %i\n", Result);
    
    return Result;
    
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


inline ULONGLONG
NarGetFilePointer(HANDLE F) {
    LARGE_INTEGER M = { 0 };
    LARGE_INTEGER N = { 0 };
    SetFilePointerEx(F, M, &N, FILE_CURRENT);
    return N.QuadPart;
}


BOOLEAN
AppendRecoveryToFile(HANDLE File, char Letter) {
    
    GUID GREC = { 0 }; // recovery partition guid
    StrToGUID("{de94bba4-06d1-4d40-a16a-bfd50179d6ac}", &GREC);
    
    BOOLEAN Result = FALSE;
    DWORD BufferSize = 1024 * 2; // 64KB
    
    
    DRIVE_LAYOUT_INFORMATION_EX* DL = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(BufferSize);
    HANDLE Disk = INVALID_HANDLE_VALUE;
    int DiskID = NarGetVolumeDiskID(Letter);
    
    if (DiskID != NAR_INVALID_DISK_ID) {
        char DiskPath[64];
        snprintf(DiskPath, sizeof(DiskPath), "\\\\?\\PhysicalDrive%i", DiskID);
        
        Disk = CreateFileA(DiskPath, GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        if (Disk != INVALID_HANDLE_VALUE) {
            
            DWORD Hell = 0;
            if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DL, BufferSize, &Hell, 0)) {
                
                if (DL->PartitionStyle == PARTITION_STYLE_GPT) {
                    
                    for (unsigned int PartitionIndex = 0; PartitionIndex < DL->PartitionCount; PartitionIndex++) {
                        PARTITION_INFORMATION_EX* PI = &DL->PartitionEntry[PartitionIndex];
                        
                        // NOTE(Batuhan): Finding recovery partition via GUID
                        if (IsEqualGUID(PI->Gpt.PartitionType, GREC)) {
                            
                            NarSetFilePointer(Disk, (ULONGLONG)PI->StartingOffset.QuadPart);
                            if (CopyData(Disk, File, (ULONGLONG)PI->PartitionLength.QuadPart)) {
                                Result = TRUE;
                                printf("[AppendRecoveryToFile]: Successfully appended recovery partition to given handle\n");
                                //NOTE(Batuhan): Success
                            }
                            else {
                                printf("Couldnt restore recovery partition from backup metadata file\n");
                            }
                            
                        }
                        
                    }
                    
                }
                else if(DL->PartitionStyle == PARTITION_STYLE_MBR){
                    // TODO
                    
                    
                    for (unsigned int PartitionIndex = 0; PartitionIndex < DL->PartitionCount; ++PartitionIndex){
                        
                        // PARTITION_INFORMATION_EX* PI = &DL->PartitionEntry[PartitionIndex];
                        //
                        
                    }
                    
                }
                
                
            }
            else {
                printf("DeviceIoControl with argument IOCTL_DISK_GET_DRIVE_LAYOUT_EX failed for drive %i, cant append recovery file to backup metadata\n", Letter);
                DisplayError(GetLastError());
            }
            
        }
        else {
            printf("Couldnt open disk %s as file, cant append recovery file to backup metadata\n", DiskPath);
        }
        
    }
    else {
        printf("Couldnt find valid Disk ID for volume letter %c\n", Letter);
    }
    
    free(DL);
    CloseHandle(Disk);
    return Result;
}

inline BOOLEAN
NarFileNameExtensionCheck(const wchar_t *Path, const wchar_t *Extension){
    TIMED_BLOCK();
    size_t pl = wcslen(Path);
    size_t el = wcslen(Extension);
    if(pl <= el) return FALSE;
    return (wcscmp(&Path[pl - el], Extension) == 0);
}



BOOLEAN
NarGetBackupsInDirectory(const wchar_t* Directory, backup_metadata* B, int BufferSize, int* FoundCount) {
    
    //#error "IMPLEMENT THAT SHIT"
    
    BOOLEAN Result = TRUE;
    
    wchar_t WildcardDir[280];
    wchar_t wstrbuffer[512];
    memset(wstrbuffer, 0, 512 * sizeof(wchar_t));
    
    wcscpy(WildcardDir, Directory);
    //So for some reason, directory string MUST end with * to be valid for iteration.
    wcscat(WildcardDir, L"\\*");
    
    WIN32_FIND_DATAW FDATA;
    HANDLE FileIterator = FindFirstFileW(WildcardDir, &FDATA);
    
    int BackupFound = 0;
    int MaxBackupCount = BufferSize / (int)sizeof(*B);
    
    if (FileIterator != INVALID_HANDLE_VALUE) {
        
        while (FindNextFileW(FileIterator, &FDATA) != 0) {
            memset(wstrbuffer, 0, 512 * sizeof(wchar_t));
            
            //NOTE(Batuhan): Do not search for sub-directories, skip folders.
            if (FDATA.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue;
            }
            
            if (BackupFound == MaxBackupCount) {
                Result = FALSE;
            }
            
            //printf("Found file %S\n", FDATA.cFileName);
            std::wstring MDExtension;
            NarGetMetadataExtension(MDExtension);
            
            //NOTE(Batuhan): Compare file name for metadata draft prefix NAR_ , if prefix found, try to read it
            if (NarFileNameExtensionCheck(FDATA.cFileName, MDExtension.c_str())) {
                
                wcscpy(wstrbuffer, Directory);
                wcscat(wstrbuffer, L"\\");
                wcscat(wstrbuffer, FDATA.cFileName);
                
                HANDLE F = CreateFileW(wstrbuffer, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
                if (F != INVALID_HANDLE_VALUE) {
                    DWORD BR = 0;
                    if (ReadFile(F, &B[BackupFound], sizeof(*B), &BR, 0) && BR == sizeof(*B)) {
                        //NOTE(Batuhan): Since we just compared metadata file name draft, now, just compare full name, to determine its filename, since it might be corrupted
                        
                        std::wstring TempName;
                        GenerateMetadataName(B[BackupFound].ID, B[BackupFound].Version, TempName);
                        
                        if (wcscmp(FDATA.cFileName, TempName.c_str()) == 0) {
                            //NOTE(Batuhan): File name indicated from metadata and actual file name matches
                            //Even though metadatas match, actual binary data may not exist at all or even, metadata itself might be corrupted too, or missing. Check it
                            
                            printf("Backup found %S, ver %i\n", FDATA.cFileName, B[BackupFound].Version);
                            BackupFound++;
                            
#if 0
                            //NOTE(Batuhan): check if actual binary data exists in path and valid in size
                            wcscpy(wstrbuffer, Directory);
                            wcscat(wstrbuffer, L"\\");
                            wcscat(wstrbuffer, GenerateBinaryFileName(B[BackupFound].Letter, B[BackupFound].Version).c_str());
                            if (PathFileExistsW(wstrbuffer)) {
                                if (NarGetFileSize(wstrbuffer) == B[BackupFound].Size.Regions) {
                                }
                            }
#endif
                            
                            
                            
                        }
                        else {
                            //NOTE(Batuhan): File name indicated from metadata and actual file name does NOT match.
                            memset(&B[BackupFound], 0, sizeof(*B));
                        }
                        
                    }
                    
                }
                
                CloseHandle(F);
            }
            
        }
        
    }
    else {
        printf("Cant iterate directory\n");
        Result = FALSE;
    }
    
    if (FoundCount) {
        *FoundCount = BackupFound;
    }
    
    FindClose(FileIterator);
    return TRUE;
    
    
}



inline BOOLEAN
AppendMFTFile(HANDLE File, HANDLE VSSHandle, data_array<nar_record> MFTLCN, int ClusterSize) {
    
    BOOLEAN Return = FALSE;
    
    if (File != INVALID_HANDLE_VALUE && VSSHandle != INVALID_HANDLE_VALUE) {
        
        LARGE_INTEGER MoveTo = { 0 };
        LARGE_INTEGER NewFilePointer = { 0 };
        BOOL Result = 0;
        for (unsigned int i = 0; i < MFTLCN.Count; i++) {
            MoveTo.QuadPart = (LONGLONG)MFTLCN.Data[i].StartPos * (LONGLONG)ClusterSize;
            
            Result = SetFilePointerEx(VSSHandle, MoveTo, &NewFilePointer, FILE_BEGIN);
            if (!SUCCEEDED(Result) || MoveTo.QuadPart != NewFilePointer.QuadPart) {
                printf("Set file pointer failed\n");
                printf("Tried to set pointer -> %I64d, instead moved to -> %I64d\n", MoveTo.QuadPart, NewFilePointer.QuadPart);
                printf("Failed at LCN : %i\n", i);
                DisplayError(GetLastError());
                goto Exit;
            }
            
            if (!CopyData(VSSHandle, File, (ULONGLONG)MFTLCN.Data[i].Len * (ULONGLONG)ClusterSize)) {
                printf("Error occured while copying MFT file\n");
                goto Exit;
            }
            
        }
        
        return TRUE;
    }
    else {
        if (File == INVALID_HANDLE_VALUE) {
            printf("File to append MFT isnt valid\n");
        }
        if (VSSHandle == INVALID_HANDLE_VALUE) {
            printf("VSSHandle was invalid [AppendMFTToFile]\n");
        }
    }
    
    Exit:
    return Return;
    
}


/*
Its not best way to initialize a struct
*/
LOG_CONTEXT*
NarLoadBootState() {
    
    printf("Entered NarLoadBootState\n");
    LOG_CONTEXT* Result;
    BOOLEAN bResult = FALSE;
    Result = (LOG_CONTEXT*)malloc(sizeof(LOG_CONTEXT));
    memset(Result, 0, sizeof(LOG_CONTEXT));
    
    Result->Port = INVALID_HANDLE_VALUE;
    Result->Volumes = { 0,0 };
    
    char FileNameBuffer[64];
    if (GetWindowsDirectoryA(FileNameBuffer, 64)) {
        
        strcat(FileNameBuffer, "\\");
        strcat(FileNameBuffer, NAR_BOOTFILE_NAME);
        HANDLE File = CreateFileA(FileNameBuffer, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
        if (File != INVALID_HANDLE_VALUE) {
            
            // Check aligment, not a great way to check integrity of file
            DWORD Size = GetFileSize(File, 0);
            if (Size > sizeof(nar_boot_track_data) * NAR_MAX_VOLUME_COUNT) {
                printf("Bootfile too large, might be corrupted\n");
            }
            else if (Size % sizeof(nar_boot_track_data) != 0) {
                printf("Bootfile is not aligned\n");
            }
            else {
                
                printf("Found previous backup files, will initialize CONTEXT accordingly\n");
                
                DWORD BR = 0;
                nar_boot_track_data* BootTrackData = (nar_boot_track_data*)malloc(Size);
                if (BootTrackData != NULL) {
                    
                    if (ReadFile(File, BootTrackData, Size, &BR, 0) && BR == Size) {
                        
                        volume_backup_inf VolInf;
                        memset(&VolInf, 0, sizeof(VolInf));
                        int BootTrackDataCount = (int)(Size / (DWORD)sizeof(BootTrackData[0]));
                        
                        for (int i = 0; i < BootTrackDataCount; i++) {
                            
                            bResult = InitVolumeInf(&VolInf, (char)BootTrackData[i].Letter, (BackupType)BootTrackData[i].BackupType);
                            
                            if (bResult) {
                                
                                VolInf.BackupID = {0};
                                VolInf.BackupID = BootTrackData[i].BackupID;
                                
                                VolInf.FilterFlags.IsActive = TRUE;
                                VolInf.FullBackupExists = TRUE;
                                VolInf.Version = BootTrackData[i].Version;
                                
                                // NOTE(Batuhan): Diff backups only need to know file pointer position they started backing up. For single-tracking system we use now, that's 0
                                if((BackupType)BootTrackData[i].BackupType == BackupType::Diff){
                                    // NOTE(Batuhan): We don't support different volume track units yet, 
                                    // for multi-tracked system this value may be different for each unit
                                    VolInf.DiffLogMark.BackupStartOffset = 0;
                                }
                                else{
                                    // Incremental backups needs to know where they left off
                                    VolInf.IncLogMark.LastBackupRegionOffset = BootTrackData[i].LastBackupOffset;
                                }
                                
                                Result->Volumes.Insert(VolInf);
                                
                            }
                            else{
                                printf("Couldnt initialize volume backup inf for volume %c\n", BootTrackData[i].Letter);
                            }
                            
                        }
                        
                        
                    }
                    else {
                        printf("Couldnt read boot file, read %i, file size was %i\n", BR, Size);
                        DisplayError(GetLastError());
                    }
                    
                }
                else {
                    printf("Couldnt allocate memory for nar_boot_track_data \n");
                    BootTrackData = 0;
                }
                
                free(BootTrackData);
                
            }
            
        }
        else {
            printf("File %s doesnt exist\n", FileNameBuffer);
            // File doesnt exist
        }
        
        CloseHandle(File);
        
    }
    else {
        printf("Couldnt get windows directory, error %i\n", GetLastError());
    }
    
    if (!Result) {
        free(Result);
        Result = NULL;
    }
    
    return Result;
}


/*
    Saves the current program state into a file, so next time computer boots driver can recognize it 
    and setup itself accordingly.

    saved states:
        - Letter (user-kernel)
        - Version (user)
        - BackupType (user)
        - LastBackupRegionOffset (user)
*/

BOOLEAN
NarSaveBootState(LOG_CONTEXT* CTX) {
    
    if (CTX == NULL || CTX->Volumes.Data == NULL){
        printf("Either CTX or its volume data was NULL, terminating NarSaveBootState\n");
        return FALSE;
    }
    
    BOOLEAN Result = TRUE;
    nar_boot_track_data Pack;
    
    char FileNameBuffer[64];
    if (GetWindowsDirectoryA(FileNameBuffer, 64)) {
        strcat(FileNameBuffer, "\\");
        strcat(FileNameBuffer, NAR_BOOTFILE_NAME);
        
        HANDLE File = CreateFileA(FileNameBuffer, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
        if (File != INVALID_HANDLE_VALUE) {
            printf("Created file to write boot information\n");
            for (unsigned int i = 0; i < CTX->Volumes.Count; i++) {
                Pack.Letter = 0;
                Pack.Version = -2;
                
                if (!CTX->Volumes.Data[i].INVALIDATEDENTRY) {
                    
                    Pack.Letter = (char)CTX->Volumes.Data[i].Letter;
                    Pack.Version = (char)CTX->Volumes.Data[i].Version; // for testing purposes, 128 version looks like fine
                    Pack.BackupType = (char)CTX->Volumes.Data[i].BT;
                    Pack.LastBackupOffset = (uint64_t)CTX->Volumes.Data[i].IncLogMark.LastBackupRegionOffset;
                    
                    DWORD BR = 0;
                    
                    if (WriteFile(File, &Pack, sizeof(Pack), &BR, 0) && BR == sizeof(Pack)) {
                        printf("Successfully saved volume [%c]'s [%i]th version information to boot file\n", Pack.Letter, Pack.Version);
                    }
                    else {
                        printf("Coulndt save volume [%c]'s [%i]th version information to boot file\n", Pack.Letter, Pack.Version);
                        Result = FALSE;
                    }
                    
                }
                else{
                    continue;    
                }
                
                
            }
            
            
        }
        else {
            printf("Couldnt open file %s, can not save boot information\n", FileNameBuffer);
            Result = FALSE;
        }
        
        CloseHandle(File);
        
    }
    
    
    return Result;
    
}





#if 1

// Intented usage: strings, not optimized for big buffers, used only in kernel mode to detect some special directory activity
BOOLEAN
NarSubMemoryExists(void *mem1, void* mem2, int mem1len, int mem2len){
    
    char *S1 = (char*)mem1;
    char *S2 = (char*)mem2;
    
    
    int k = 0;
    int j = 0;
    
    while (k < mem1len && j < mem2len){
        
        if(S1[k] == S2[j]){
            
            int temp = k;
            int found = FALSE;
            
            while(k < mem1len && j < mem2len){
                if(S1[k] == S2[j]){
                    j++;
                    k++;
                }
                else {
                    break;
                }
            }
            
            if(j == mem2len){
                return TRUE;
            }
            
            k = temp;
            
        }
        else{
            k++;
            j = 0;
        }
        
    }
    
    
    
    return FALSE;
}





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



/*
    Gets the intersection of the r1 and r2 arrays, writes new array to dynamicall allocated r3 array, and sets intersections pointer to that pointers adress
    after caller has done with that r3 array, caller MUST call NarFreeRegionIntersections with intersections parameter that has passed here. Otherwise memory will be leaked
    ASSUMES R1 AND R2 ARE SORTED
*/
inline void
NarGetRegionIntersection(nar_record* r1, nar_record* r2, nar_record** intersections, INT32 len1, INT32 len2, INT32 *IntersectionLen) {
    
    if (!r1 || !r2 || !intersections) return;
    
    int i1, i2, i3;
    i1 = i2 = i3 = 0;
    
    //memset(r3, 0, len3 * sizeof(*r3));
    uint32_t r3cap = len2;
    nar_record* r3 = (nar_record*)malloc(r3cap * sizeof(nar_record));
    
    // logic behind iteration, ALWAYS iterate item that has LOWER END.
    while (TRUE) {
        
        if (i1 >= len1 || i2 >= len2) {
            break;
        }
        
        if (i3 >= r3cap) {
            r3cap *= 2;
            r3 = (nar_record*)realloc(r3, r3cap * sizeof(nar_record));
        }
        
        uint32_t r1end = r1[i1].StartPos + r1[i1].Len;
        uint32_t r2end = r2[i2].StartPos + r2[i2].Len;
        
        /*
       r2          -----------------
       r1 -----
       not touching, iterate r1
        */
        if (r1end < r2[i2].StartPos) {
            i1++;
            continue;
        }
        /*
      r1          -----------------
      r2 -----
      not touching, iterate r2
       */
        
        if (r2end < r1[i1].StartPos) {
            i2++;
            continue;
        }
        
        /*
        
        condition 1:
            --------------
        -------                             MUST ITERATE THAT ONE
    
        condition 2:
            -----------------               MUST ITERATE THAT ONE
                        ------------
    
        condition 3:
            ------------------
                --------                    MUST ITERATE THAT ONE
    
        condition 4:
            -----------------
            -----------------
                doesnt really matter which one you iterate, fits in algorithm below
    
        as you can see, if we would like to spot continuous colliding blocks, we must ALWAYS ITERATE THAT HAS LOWER END POINT, and their intersection WILL ALWAYS BE REPRESENTED AS
        HIGH_START - LOW_END of both of them.
        */
        uint32_t IntersectionEnd   = MIN(r1end, r2end);
        uint32_t IntersectionStart = MAX(r1[i1].StartPos, r2[i2].StartPos);
        
        r3[i3].StartPos = IntersectionStart;
        r3[i3].Len = IntersectionEnd - IntersectionStart;
        i3++;
        
        if (r1end < r2end) i1++;
        else               i2++;
        
    }
    
    r3 = (nar_record*)realloc(r3, i3 * sizeof(nar_record));
    *IntersectionLen = i3;
    *intersections = r3;
    
}


inline void
NarFreeRegionIntersection(nar_record* intersections) {
    if (intersections != NULL) free(intersections);
}



inline INT32
NarGetVolumeClusterSize(char Letter){
    
    char V[] = "!:\\";
    V[0] = Letter;
    
    INT32 Result = 0;
    DWORD SectorsPerCluster = 0;
    DWORD BytesPerSector = 0;
    
    if (GetDiskFreeSpaceA(V, &SectorsPerCluster, &BytesPerSector, 0, 0)){
        Result = SectorsPerCluster * BytesPerSector;
    }
    else{
        printf("Couldnt get disk free space for volume %c\n", Letter);
    }
    
    return Result;
}

// asserts Buffer is large enough to hold all data needed, since caller has information about metadata this isnt problem at all
inline BOOLEAN
ReadMFTLCNFromMetadata(HANDLE FHandle, backup_metadata Metadata, void *Buffer){
    
    BOOLEAN Result = FALSE;
    if(FHandle != INVALID_HANDLE_VALUE){
        
        // Caller must validate metadata integrity, but it's likely that something invalid may be passed here, check it anyway there isnt a cost
        if(NarSetFilePointer(FHandle, Metadata.Offset.MFTMetadata)){
            
            DWORD BR = 0;        
            if(ReadFile(FHandle, Buffer, Metadata.Size.MFTMetadata, &BR, 0) && BR == Metadata.Size.MFTMetadata){
                Result = TRUE;
            }
            else{
                // readfile failed
                printf("Readfile failed for ReadMFTLCNFromMetadata, tried to read %i bytes, instead read %i\n", Metadata.Size.MFTMetadata, BR);
                DisplayError(GetLastError());
            }
            
        }
        else{
            printf("Couldnt set file pointer to %I64d to read mftlcn from metadata\n", Metadata.Offset.MFTMetadata);
            // setfileptr failed
        }
        
    }
    else{
        printf("Metadata handle was invalid, terminating ReadMFTLCNFromMetadata\n");
        // handle was invalid
    }
    
    return Result;
}



/*
    Returns first available volume letter that is not used by system
*/
inline char
NarGetAvailableVolumeLetter() {
    
    DWORD Drives = GetLogicalDrives();
    
    for (int CurrentDriveIndex = 0; CurrentDriveIndex < 26; CurrentDriveIndex++) {
        if (Drives & (1 << CurrentDriveIndex)) {
            
        }
        else {
            return ('A' + (char)CurrentDriveIndex);
        }
    }
    
    return 0;
}


/*
Expects Letter to be uppercase
*/
inline BOOLEAN
NarIsVolumeAvailable(char Letter){
    DWORD Drives = GetLogicalDrives();
    return !(Drives & (1 << (Letter - 'A')));
}




// input MUST be sorted
// Finds point Offset in relative to Records structure, useful when converting absolue volume offsets to our binary backup data offsets.
// returns NAR_POINT_OFFSET_FAILED if fails to find given offset, 
inline INT64 
FindPointOffsetInRecords(nar_record *Records, INT32 Len, INT64 Offset){
    
    if(!Records) return NAR_POINT_OFFSET_FAILED;
    
    INT64 Result = 0;
    BOOLEAN Found = FALSE;
    
    for(int i = 0; i < Len; i++){
        
        if(Offset <= (INT64)Records[i].StartPos + (INT64)Records[i].Len){
            
            INT64 Diff = (Offset - (INT64)Records[i].StartPos);
            if (Diff < 0) {
                // Exceeded offset, this means we cant relate our Offset and Records data, return failcode
                Found = FALSE;
            }
            else {
                Result += Diff;
                Found = TRUE;
            }
            
            break;
            
        }
        
        
        Result += Records[i].Len;
        
    }
    
    
    return (Found ? Result : NAR_POINT_OFFSET_FAILED);
}

#if 1

inline BOOLEAN
NarEditTaskNameAndDescription(const wchar_t* FileName, const wchar_t* TaskName, const wchar_t* TaskDescription){
    
    if(FileName == NULL) return FALSE;
    
    size_t TaskNameLen = wcslen(TaskName);
    size_t TaskDescriptionLen = wcslen(TaskDescription);
    
    if(TaskNameLen > NAR_MAX_TASK_NAME_LEN/sizeof(wchar_t)){
        printf("Input Task name can't fit metadata, len was %I64u\n", TaskNameLen);
        return FALSE;
    }
    if(TaskDescriptionLen > NAR_MAX_TASK_DESCRIPTION_LEN/sizeof(wchar_t)){
        printf("Input Task description can't fit metadata, len was %I64u\n", TaskDescriptionLen);
        return FALSE;
    }
    if(wcslen(FileName) == 0){
        return FALSE;
    }
    
    BOOLEAN Result = 0;
    
    HANDLE FileHandle = CreateFileW(FileName, 
                                    GENERIC_READ | GENERIC_WRITE, 
                                    FILE_SHARE_READ, 0, 
                                    OPEN_EXISTING, 0, 0);
    if(FileHandle != INVALID_HANDLE_VALUE){
        // TODO(Batuhan): validate if it is really metadata file
        DWORD BytesOperated = 0;
        backup_metadata Metadata;
        if(ReadFile(FileHandle, &Metadata, sizeof(Metadata), &BytesOperated, 0) && BytesOperated == sizeof(Metadata)){
            wcsncpy(Metadata.TaskName, TaskName, NAR_MAX_TASK_NAME_LEN);
            wcsncpy(Metadata.TaskDescription, TaskDescription, NAR_MAX_TASK_DESCRIPTION_LEN);
            if(NarSetFilePointer(FileHandle, FILE_BEGIN)){
                if(WriteFile(FileHandle, &Metadata, sizeof(Metadata), &BytesOperated, 0) && BytesOperated == sizeof(Metadata)){
                    printf("Successfully updated metadata task name and task description info\n");
                    Result = TRUE;
                }
                else{
                    printf("Couldnt update metadata task name and task description(%S)\n", FileName);
                    DisplayError(GetLastError());
                }
            }
        }
        else{
            printf("Couldn't read %i bytes from file %S, task was NarEditTaskNameAndDescription\n", &BytesOperated, FileName);
            DisplayError(GetLastError());
        }
    }
    else{
        printf("Couldnt open file %S to edit TaskName and Description\n", FileName);
    }
    
    CloseHandle(FileHandle);
    
    return Result;
}

#endif


void
TestFindPointOffsetInRecords(){
    
    nar_record* Recs = new nar_record[100];
    memset(Recs, 0, sizeof(nar_record) * 100);
    
    Recs[0] = {0, 100};
    Recs[1] = {150, 200};
    Recs[2] = {520, 150};
    Recs[3] = {1200, 55};
    Recs[4] = {1300, 100};
    Recs[5] = {1500, 75};
    Recs[6] = {1700, 420};
    Recs[7] = {5200, 500};
    
    INT64 Answer = FindPointOffsetInRecords(Recs, 8, 1350);
    printf("HELL ==== %I64d\n", Answer);
    
    Answer = FindPointOffsetInRecords(Recs, 8, 1000);
    printf("HELL ==== %I64d\n", Answer);
    
    Answer = FindPointOffsetInRecords(Recs, 8, 70);
    printf("HELL ==== %I64d\n", Answer);
    
    Answer = FindPointOffsetInRecords(Recs, 8, 5400);
    printf("HELL ==== %I64d\n", Answer);
    
    Answer = FindPointOffsetInRecords(Recs, 8, 2000);
    printf("HELL ==== %I64d\n", Answer);
    
    Answer = FindPointOffsetInRecords(Recs, 8, 3000);
    printf("HELL ==== %I64d\n", Answer);
    
    
    return;
    
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


inline void
NarGetProductName(char* OutName) {
    
    HKEY Key;
    if (RegOpenKeyA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", &Key) == ERROR_SUCCESS) {
        DWORD H = 0;
        if(RegGetValueA(Key, 0, "ProductName", RRF_RT_ANY, 0, OutName, &H) != ERROR_SUCCESS){
            DisplayError(GetLastError());
        }
        //RegCloseKey(Key);
    }
    
}



struct nar_pool_entry{
    nar_pool_entry *Next;
};

struct nar_memory_pool{
    void *Memory;
    nar_pool_entry *Entries;
    int PoolSize;
    int EntryCount;
};


nar_memory_pool
NarInitPool(void *Memory, int MemorySize, int PoolSize){
    
    if(Memory == NULL) return { 0 };
    
    nar_memory_pool Result = {0};
    Result.Memory = Memory;
    Result.PoolSize = PoolSize;
    Result.EntryCount = MemorySize / PoolSize;
    
    for(size_t i = 0; i < Result.EntryCount - 1; i++){
        nar_pool_entry *entry = (nar_pool_entry*)((char*)Memory + (PoolSize * i));
        entry->Next = (nar_pool_entry*)((char*)entry + PoolSize);
    }
    
    nar_pool_entry *entry = (nar_pool_entry*)((char*)Memory + (PoolSize * (Result.EntryCount - 1)));
    entry->Next = 0;
    Result.Entries = (nar_pool_entry*)Memory;
    
    for(nar_pool_entry *e = Result.Entries; e != NULL; e = e->Next){
        printf("%X\n", e);
    }
    
    return Result;
}


#define NAR_FAILED 0
#define NAR_SUCC   1

#define loop for(;;)


#include<iostream>


void
DEBUG_Restore(){
    
    size_t MemLen = Megabyte(250);
    std::wstring MetadataPath = L"";
    int DiskID;
    wchar_t TargetLetter;
    
    std::cout<<"Enter metadata path\n";
    std::wcin>>MetadataPath;
    
    std::cout<<"Enter disk id\n";
    std::cin>>DiskID;
    
    std::cout<<"Enter target letter\n";
    std::wcin>>TargetLetter;
    
    backup_metadata bm;
    NarReadMetadata(MetadataPath, &bm);
    
    if(false == NarReadMetadata(MetadataPath, &bm)){
        std::wcout<<L"Unable to read metadata "<<MetadataPath<<"\n";
    }
    
    //SetDiskRestore(DiskID, TargetLetter, &bm);
    
    void* Mem = VirtualAlloc(0, MemLen, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    nar_arena Arena = ArenaInit(Mem, MemLen);
    
#if 1    
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
        if(Stream->Error != RestoreStream_Errors::Error_NoError){
            NAR_BREAK;
        }
    }
    
    FreeRestoreStream(Stream);
    
}

void
DEBUG_FileExplorer(){
	ASSERT(0);    
}


void
DEBUG_FileRestore(){
	ASSERT(0);    
}




void
DEBUG_FileExplorerQuery(){
    ASSERT(0);
    
}

void
DEBUG_Parser(){
    
    file_read r = NarReadFile("C:\\tmp\\mftdump");
    
    uint64_t MaxOutputLen = 1024ull*1024ull*512ull;
    
    nar_record *ClustersExtracted = (nar_record*)VirtualAlloc(0, MaxOutputLen*sizeof(nar_record), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    unsigned int ClusterExtractedCount = 0;
    
    unsigned int TotalFC = r.Len/1024u;
    
    for(unsigned int FileRecordIndex = 0; FileRecordIndex < TotalFC; FileRecordIndex++){
        void* FileRecord = (BYTE*)r.Data + (uint64_t)1024 * (uint64_t)FileRecordIndex;
        
        if (*(INT32*)FileRecord != 'ELIF') {
            // block doesnt start with 'FILE0', skip
            continue;
        }
        
        void *IndxOffset = NarFindFileAttributeFromFileRecord(FileRecord, NAR_INDEX_ALLOCATION_FLAG);
        
        
        if(IndxOffset != NULL){
            
            void *BitmapAttr = NarFindFileAttributeFromFileRecord(FileRecord, NAR_BITMAP_FLAG);
            
            if(BitmapAttr == NULL){
                uint32_t RegFound = 0;
                NarParseIndexAllocationAttribute(IndxOffset, &ClustersExtracted[ClusterExtractedCount], MaxOutputLen - ClusterExtractedCount, &RegFound);
                ClusterExtractedCount += RegFound;
            }
            else{
                // NOTE(Batuhan): parses indx allocation to one-cluster granuality blocks, to make bitmap parsing trivial. increases memory usage(at parsing stage, not for the final outcome), but makes parsing much easier.
                
                uint32_t RegFound = 0;
                NarParseIndexAllocationAttributeSingular(IndxOffset, &ClustersExtracted[ClusterExtractedCount], MaxOutputLen - ClusterExtractedCount, &RegFound);
                
                
                int BLen = NarGetBitmapAttributeDataLen(BitmapAttr);
                unsigned char* Bitmap = (unsigned char*)NarGetBitmapAttributeData(BitmapAttr);
                
                size_t ceil = MIN(RegFound, BLen);
                for(size_t ci = 0; ci<BLen; ci++){
                    TIMED_NAMED_BLOCK("Bitmap merge stuff");
                    size_t ByteIndex = ci/8;
                    size_t BitIndex = ci%8;
                    if(Bitmap[ByteIndex] & (1 << BitIndex)){
                        // everything is ok
                    }
                    else{
                        ClustersExtracted[ci + ClusterExtractedCount].StartPos = 0;
                        ClustersExtracted[ci + ClusterExtractedCount].Len = 0;
                    }
                }
                ClusterExtractedCount += RegFound;
                
            }
            
            
        }
    }
    
    
}

int
main(int argc, char* argv[]) {
    
    
    return 0;
    
    if(argc != 2){
        printf("invalid argument, pass restore or backup\n");
    }
    
    
    if(std::string(argv[1]) == "restore"){
        DEBUG_Restore();
        return 0;
    }
    
    if(std::string(argv[1]) != "backup"){
        printf("invalid argument\n");
    }
    
#if 0    
    HANDLE H = NarOpenVolume('C');
    GetMFTandINDXLCN('C', H);
    
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
            
            BackupType bt = (BackupType)Type;
            
            if(SetupStream(&C, (wchar_t)Volume, bt, &inf, true)){
                
                int id = GetVolumeID(&C, (wchar_t)Volume);
                volume_backup_inf *v = &C.Volumes.Data[id];
                size_t TotalRead = 0;
                size_t TotalWritten = 0;
                size_t TargetWrite = (size_t)inf.ClusterSize * (size_t)inf.ClusterCount;
                
                printf("filename : %S\n", inf.FileName.c_str());
                
                HANDLE file = CreateFileW(inf.FileName.c_str(), 
                                          GENERIC_WRITE | GENERIC_READ, 
                                          0, 0, 
                                          CREATE_ALWAYS, 
                                          0, 0);
                if(file != INVALID_HANDLE_VALUE){
                    
#if 1                    
                    loop{
                        int Read = ReadStream(v, MemBuf, bsize);
                        TotalRead += Read;
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
                                DisplayError(GetLastError());
                            }
                        }
                    }
#else
                    
                    TotalRead = TargetWrite;
                    TotalWritten = TargetWrite;
#endif
                    
                    
                    NAR_BREAK;
                    if(CheckStreamCompletedSuccessfully(v)){
                        TerminateBackup(v, NAR_SUCC);
                    }
                    else{
                        NAR_BREAK;
                        TerminateBackup(v, NAR_FAILED);
                    }
                    
                    PrintDebugRecords();
                    
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

#endif





VOID
DisplayError(DWORD Code) {
    
    WCHAR buffer[MAX_PATH] = { 0 };
    DWORD count;
    HMODULE module = NULL;
    HRESULT status;
    
    count = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                          NULL,
                          Code,
                          0,
                          buffer,
                          sizeof(buffer) / sizeof(WCHAR),
                          NULL);
    
    
    if (count == 0) {
        
        count = GetSystemDirectory(buffer,
                                   sizeof(buffer) / sizeof(WCHAR));
        
        if (count == 0 || count > sizeof(buffer) / sizeof(WCHAR)) {
            
            //
            //  In practice we expect buffer to be large enough to hold the
            //  system directory path.
            //
            
            printf("    Could not translate error: %d\n", Code);
            return;
        }
        
        
        status = StringCchCat(buffer,
                              sizeof(buffer) / sizeof(WCHAR),
                              L"\\fltlib.dll");
        
        if (status != S_OK) {
            
            printf("    Could not translate error: %d\n", Code);
            return;
        }
        
        module = LoadLibraryExW(buffer, NULL, LOAD_LIBRARY_AS_DATAFILE);
        
        //
        //  Translate the Win32 error code into a useful message.
        //
        
        count = FormatMessage(FORMAT_MESSAGE_FROM_HMODULE,
                              module,
                              Code,
                              0,
                              buffer,
                              sizeof(buffer) / sizeof(WCHAR),
                              NULL);
        
        if (module != NULL) {
            
            FreeLibrary(module);
        }
        
        //
        //  If we still couldn't resolve the message, generate a string
        //
        
        if (count == 0) {
            
            printf("    Could not translate error: %d\n", Code);
            return;
        }
    }
    
    //
    //  Display the translated error.
    //
    
    printf("    %ws\n", buffer);
}

//debug_record GlobalDebugRecordArray[__COUNTER__];

inline void
PrintDebugRecords() {
    
    
#if 0    
    SYSTEMTIME Time;
    char tbuf[80];
    
    snprintf(tbuf, 80, "NarPerformanceDump [%02d/%02d/%04d | %02d:%02d:%02d]", );
    
    GetLocalTime(&Time);
    
    FILE *F = fopen("NarPerformanceDump", "rb");
#endif
    
    
    int len = sizeof(GlobalDebugRecordArray) / sizeof(debug_record);
    for (int i = 0; i < len; i++) {
        
        if(GlobalDebugRecordArray[i].FunctionName == NULL){
            continue;
        }
        
        printf("FunctionName: %s\tdescription: %s\tavclocks: %.3f\tat line %i\thit count %i\n", GlobalDebugRecordArray[i].FunctionName, GlobalDebugRecordArray[i].Description, (double)GlobalDebugRecordArray[i].Clocks / GlobalDebugRecordArray[i].HitCount, GlobalDebugRecordArray[i].LineNumber, GlobalDebugRecordArray[i].HitCount);
        
    }
    
}
