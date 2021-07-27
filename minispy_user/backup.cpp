#include "nar.h"
#include "backup.h"
#include "performance.h"
#include "nar_win32.h"



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


inline bool
IsRegionsCollide(nar_record R1, nar_record R2) {
    
    uint32_t R1EndPoint = R1.StartPos + R1.Len;
    uint32_t R2EndPoint = R2.StartPos + R2.Len;
    
    if (R1.StartPos == R2.StartPos && R1.Len == R2.Len) {
        return true;
    }
    
    
    if ((R1EndPoint <= R2EndPoint
         && R1EndPoint >= R2.StartPos)
        || (R2EndPoint <= R1EndPoint
            && R2EndPoint >= R1.StartPos)
        ) {
        return true;
    }
    
    return false;
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


BOOLEAN
SetupVSS() {
    /* 
        NOTE(Batuhan): in managed code we dont need to initialize these stuff. since i am shipping code as textual to wrapper, i can detect clr compilation and switch to correct way to initialize
        vss stuff
     */
    
#if 1
    
    
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
    
    printf("PNBRO : %I64u\n", V->PossibleNewBackupRegionOffsetMark);
    printf("SIR: Volume %c, lko %I64u\n", V->Letter, V->IncLogMark.LastBackupRegionOffset);
    
    ASSERT((uint64_t)V->IncLogMark.LastBackupRegionOffset < (uint64_t)V->PossibleNewBackupRegionOffsetMark);
    
    DWORD TargetReadSize = (DWORD)(V->PossibleNewBackupRegionOffsetMark - V->IncLogMark.LastBackupRegionOffset);
    
    V->Stream.Records.Data = 0;
    V->Stream.Records.Count = 0;
    
    std::wstring logfilepath = GenerateLogFilePath(V->Letter);
    HANDLE LogHandle = CreateFileW(logfilepath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
    
    // calling malloc with 0 is implementation defined behaviour, but not having logs is possible thing that might happen and no need to worry about it since
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
                if(BytesRead == 0){
                    free(V->Stream.Records.Data);
                    V->Stream.Records.Count = 0;
                }
                
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
            printf("KERNEL RESPOND: Log size of volume %c is %I64u\n", Letter, Result);
        }
        else{
            printf("FilterSendMessage failed, couldnt remove volume from kernel side, err %i\n", hResult);
            DisplayError(GetLastError());
        }
        
    }
    else{
        printf("Unable to generate kernel GUID of volume %c\n", Letter);
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
SetFullRecords(volume_backup_inf* V) {
    
    //UINT* ClusterIndices = 0;
    
    uint32_t Count = 0;
    V->Stream.Records.Data  = GetVolumeRegionsFromBitmap(V->Stream.Handle, &Count);
    V->Stream.Records.Count = Count;
    
    return (V->Stream.Records.Data != NULL);
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
    
    // temporary allocation, will extend after determining exact size of the bitmap
    DWORD BufferSize = Megabyte(32); 
    
    VOLUME_BITMAP_BUFFER* Bitmap = (VOLUME_BITMAP_BUFFER*)malloc(BufferSize);
    
    if (Bitmap != NULL) {
        
        int HitC = 0;
        p:;
        HitC++;
        
        HRESULT R = DeviceIoControl(VolumeHandle, FSCTL_GET_VOLUME_BITMAP, &StartingLCN, sizeof(StartingLCN), Bitmap, BufferSize, 0, 0);
        if (SUCCEEDED(R)) {
            
            if(HitC == 1){
                if(BufferSize < Bitmap->BitmapSize.QuadPart + sizeof(VOLUME_BITMAP_BUFFER)){
                    Bitmap = (VOLUME_BITMAP_BUFFER*)realloc(Bitmap, Bitmap->BitmapSize.QuadPart + sizeof(VOLUME_BITMAP_BUFFER) + 5);
                    goto p;
                }
            }
            
            MaxClusterCount = (ULONGLONG)Bitmap->BitmapSize.QuadPart;
            uint32_t ClusterIndex = 0;
            UCHAR* BitmapIndex = Bitmap->Buffer;
            UCHAR BitmapMask = 1;
            //uint32_t CurrentIndex = 0;
            uint32_t LastActiveCluster = 0;
            
            uint32_t RecordBufferCount = 1 * 1024 * 1024;
            Records = (nar_record*)calloc(RecordBufferCount, sizeof(nar_record));
            
            Records[0] = { 0,1 };
            RecordCount++; 
            
            ClusterIndex++;
            BitmapMask <<= 1;
            
            while (BitmapIndex != (Bitmap->Buffer + Bitmap->BitmapSize.QuadPart)) {
                
                if ((*BitmapIndex & BitmapMask) == BitmapMask) {
                    
                    if (LastActiveCluster == ClusterIndex - 1) {
                        Records[RecordCount - 1].Len++;
                    }
                    else {
                        nar_record dbg = {0};
                        dbg.StartPos = ClusterIndex;
                        dbg.Len      = 1;
                        Records[RecordCount++] = dbg;
                    }
                    
                    LastActiveCluster = ClusterIndex;
                }
                BitmapMask <<= 1;
                if (BitmapMask == 0) {
                    BitmapMask = 1;
                    BitmapIndex++;
                }
                ClusterIndex++;
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



/*
*/
BOOLEAN
SetupStream(PLOG_CONTEXT C, wchar_t L, BackupType Type, DotNetStreamInf* SI, bool ShouldCompress) {
    
    TIMED_BLOCK();
    
#if _MANAGED
    // #error remove this stuff
#endif
    
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
            
            printf("Parsed MFTLCN for volume %c for version %i\n", (wchar_t)VolInf->Letter, VolInf->Version);
            
            VolInf->Stream.Records.Data = (nar_record*)realloc(VolInf->Stream.Records.Data, (VolInf->Stream.Records.Count + MFTandINDXRegions.Count) * sizeof(nar_record));
            memcpy(&VolInf->Stream.Records.Data[VolInf->Stream.Records.Count], MFTandINDXRegions.Data, MFTandINDXRegions.Count * sizeof(nar_record));
            
            VolInf->Stream.Records.Count += MFTandINDXRegions.Count;
            
            qsort(VolInf->Stream.Records.Data, VolInf->Stream.Records.Count, sizeof(nar_record), CompareNarRecords);
            MergeRegions(&VolInf->Stream.Records);
            
#if 0            
            for(size_t i =0; i<VolInf->Stream.Records.Count; i++){
                printf("MERGED REGIONS OVERALL : %9u %9u\n", VolInf->Stream.Records.Data[i].StartPos, VolInf->Stream.Records.Data[i].Len);
            }
#endif
            
        }
        else {
            printf("Couldnt parse MFT at setupstream function for volume %c, version %i\n", L, VolInf->Version);
        }
        
        FreeDataArray(&MFTandINDXRegions);
    };
    
    
    WCHAR Temp[] = L"!:\\";
    Temp[0] = VolInf->Letter;
    wchar_t ShadowPath[256];
    
    HANDLE TmpVolHandle = NarOpenVolume(VolInf->Letter);
    BOOL WapiResult = FlushFileBuffers(TmpVolHandle);
    CloseHandle(TmpVolHandle);
    
    VolInf->SnapshotID = GetShadowPath(Temp, VolInf->VSSPTR, ShadowPath, sizeof(ShadowPath)/sizeof(ShadowPath[0]));
    
    if (ShadowPath == NULL) {
        printf("Can't get shadowpath from VSS\n");
        Return = FALSE;
    }
    
    // no overheat for attaching volume again and again
    if(FALSE == AttachVolume(VolInf->Letter)){
        printf("Cant attach volume\n");
        Return = FALSE;
    }
    
    VolInf->Stream.Handle = CreateFileW(ShadowPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
    if (VolInf->Stream.Handle == INVALID_HANDLE_VALUE) {
        printf("Can not open shadow path %S..\n", ShadowPath);
        DisplayError(GetLastError());
        Return = FALSE;
    }
    printf("Setup stream handle successfully\n");
    
    if (VolInf->Version == NAR_FULLBACKUP_VERSION) {
        printf("Fullbackup stream is preparing\n");
        
        //Fullbackup stream
        if (SetFullRecords(VolInf)) {
            Return = TRUE;
        }
        else {
            printf("SetFullRecords failed!\n");
        }
        
        if (SI && Return) {
            SI->MetadataFileName = L"";
            GenerateMetadataName(VolInf->BackupID, NAR_FULLBACKUP_VERSION, SI->MetadataFileName);
            SI->FileName = L"";
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
        
#if 0        
        printf("DRIVER REGIONS : \n");
        for(size_t i =0; i<V->Stream.Records.Count; i++){
            printf("Start : %9u Len : %9u\n", V->Stream.Records.Data[i].StartPos,  V->Stream.Records.Data[i].Len);
        }
#endif
        
        AppendINDXnMFTLCNToStream();
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
            
            uint32_t NewEnd = V->VolumeTotalClusterCount;
            V->Stream.Records.Data[TruncateIndex].Len = NewEnd - V->Stream.Records.Data[TruncateIndex].StartPos;
            
            ASSERT(V->Stream.Records.Data[TruncateIndex].StartPos + V->Stream.Records.Data[TruncateIndex].Len <= V->VolumeTotalClusterCount);
            
            V->Stream.Records.Data = 
                (nar_record*)realloc(V->Stream.Records.Data, (TruncateIndex + 1)*sizeof(nar_record));            
            V->Stream.Records.Count = TruncateIndex + 1;
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
            VolInf->Stream.BufferSize        = NAR_COMPRESSION_FRAME_SIZE;
            
            if(NULL != VolInf->Stream.CompressionBuffer){        
                VolInf->Stream.ShouldCompress = true;
                
                VolInf->Stream.CCtx = ZSTD_createCCtx();
                ASSERT(VolInf->Stream.CCtx);
                
                ZSTD_bounds ThreadBounds = ZSTD_cParam_getBounds(ZSTD_c_nbWorkers);
                if(!ZSTD_isError(ThreadBounds.error)){
                    ZSTD_CCtx_setParameter(VolInf->Stream.CCtx, ZSTD_c_nbWorkers, ThreadBounds.upperBound);
                }
                else{
                    NAR_BREAK;
                    printf("Couldn't query worker thread bounds for compression, zstd error code : %X\n", ThreadBounds.error);
                    ZSTD_CCtx_setParameter(VolInf->Stream.CCtx, ZSTD_c_nbWorkers, 0);
                }
                
                size_t RetCode = ZSTD_CCtx_setParameter(VolInf->Stream.CCtx, ZSTD_c_compressionLevel, ZSTD_strategy::ZSTD_lazy);
                ASSERT(!ZSTD_isError(RetCode));
                
                size_t BackupSize = (uint64_t)SI->ClusterCount*(uint64_t)SI->ClusterSize;
                
                VolInf->MaxCBI  = BackupSize/(NAR_COMPRESSION_FRAME_SIZE);
                
                VolInf->CompInf  = (nar_record*)calloc(VolInf->MaxCBI, sizeof(nar_record));
                
                VolInf->CBII   = 0;
                
            }
            else{
                VolInf->Stream.ShouldCompress = false;        
                VolInf->Stream.BufferSize = 0;
            }
            
        }
        
    }
    
    printf("Number of backup bytes : %I64u\n", (uint64_t)SI->ClusterCount*(uint64_t)SI->ClusterSize);
    
    return Return;
}



BOOLEAN
TerminateBackup(volume_backup_inf* V, BOOLEAN Succeeded) {
    
    BOOLEAN Return = FALSE;
    if (!V) return FALSE;
    
    printf("Volume %c version %i backup operation will be terminated\n", V->Letter, V->Version);
    
    if(Succeeded){
        if(!!SaveMetadata((char)V->Letter, V->Version, V->ClusterSize, V->BT, V->Stream.Records, V->BackupID, V->Stream.ShouldCompress, V->Stream.Handle, V->CompInf, V->CBII)){
            
            if(V->BT == BackupType::Inc){
                V->IncLogMark.LastBackupRegionOffset = V->PossibleNewBackupRegionOffsetMark;
            }
            V->PossibleNewBackupRegionOffsetMark = 0;
            
            V->Version++;
        }
        else{
            printf("Couldn't save metadata. Version %i volume %c\n", V->Version, V->Letter);
        }
    }
    else{
        
    }
    
#if 0    
    //Termination of fullbackup
    //neccecary for incremental backup
    if(V->BT == BackupType::Inc){
        V->IncLogMark.LastBackupRegionOffset = 0;
        V->PossibleNewBackupRegionOffsetMark = 0;
    }
#endif
    
    
    if(NULL != V->Stream.CompressionBuffer){
        V->Stream.BufferSize = 0;
        free(V->Stream.CompressionBuffer);
        V->Stream.CompressionBuffer = NULL;
    }
    
    if(NULL != V->Stream.CCtx) {
        ZSTD_freeCCtx(V->Stream.CCtx);
    }
    if(V->Stream.Handle != INVALID_HANDLE_VALUE) {
        CloseHandle(V->Stream.Handle);
        V->Stream.Handle = INVALID_HANDLE_VALUE;
    }
    if(V->Stream.Records.Data != NULL){
        FreeDataArray(&V->Stream.Records);
    }
    if(V->CompInf != 0){
        free(V->CompInf);
        V->CompInf = 0;
        V->CBII   = 0;
        V->MaxCBI = 0;
    }
    
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




// Assumes CallerBufferSize >= NAR_COMPRESSION_FRAME_SIZE
uint32_t
ReadStream(volume_backup_inf* VolInf, void* CallerBuffer, uint32_t CallerBufferSize) {
    
    //TotalSize MUST be multiple of cluster size
    uint32_t Result = 0;
    
    void* BufferToFill = CallerBuffer;
    unsigned int TotalSize = CallerBufferSize;
    if(true == VolInf->Stream.ShouldCompress){
        BufferToFill    = VolInf->Stream.CompressionBuffer;
        TotalSize       = VolInf->Stream.BufferSize;
    }
    
    if (TotalSize == 0) {
        printf("Passed totalsize as 0, terminating now\n");
        return TRUE;
    }
    
    unsigned int RemainingSize = TotalSize;
    void* CurrentBufferOffset = BufferToFill;
    
    if ((uint32_t)VolInf->Stream.RecIndex >= VolInf->Stream.Records.Count) {
        printf("End of the stream\n", VolInf->Stream.RecIndex, VolInf->Stream.Records.Count);
        return Result;
    }
    
    
    while (RemainingSize) {
        
        if ((uint32_t)VolInf->Stream.RecIndex >= VolInf->Stream.Records.Count) {
            printf("Rec index was higher than record's count, result %u, rec_index %i rec_count %i\n", Result, VolInf->Stream.RecIndex, VolInf->Stream.Records.Count);
            break;
        }
        
        DWORD BytesReadAfterOperation = 0;
        
        uint64_t ClustersRemainingByteSize = (uint64_t)VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].Len - (uint64_t)VolInf->Stream.ClusterIndex;
        ClustersRemainingByteSize *= VolInf->ClusterSize;
        
        
        DWORD ReadSize = (DWORD)MIN((uint64_t)RemainingSize, ClustersRemainingByteSize); 
        
        ULONGLONG FilePtrTarget = (ULONGLONG)VolInf->ClusterSize * ((ULONGLONG)VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].StartPos + (ULONGLONG)VolInf->Stream.ClusterIndex);
        if (NarSetFilePointer(VolInf->Stream.Handle, FilePtrTarget)) {
            
#if 0
            ASSERT(ReadSize % 4096 == 0);
            ASSERT(FilePtrTarget % 4096 == 0);
            ASSERT(((char*)CurrentBufferOffset - (char*)BufferToFill) % 4096 == 0);
            
            if(VolInf->Version != NAR_FULLBACKUP_VERSION){
                printf("BackupRead : Reading %9I64u clusters from volume offset %9I64u, writing it to buffer offset of %9I64u\n", ReadSize/4096ull, FilePtrTarget/4096, (char*)CurrentBufferOffset - (char*)BufferToFill);
            }
#endif
            
            BOOL OperationResult = ReadFile(VolInf->Stream.Handle, CurrentBufferOffset, ReadSize, &BytesReadAfterOperation, 0);
            Result += BytesReadAfterOperation;
            ASSERT(BytesReadAfterOperation == ReadSize);
            
            
            if (!OperationResult || BytesReadAfterOperation != ReadSize) {
                printf("STREAM ERROR: Couldnt read %lu bytes, instead read %lu, error code %i\n", ReadSize, BytesReadAfterOperation, OperationResult);
                printf("rec_index % i rec_count % i, remaining bytes %I64u, offset at disk %I64u\n", VolInf->Stream.RecIndex, VolInf->Stream.Records.Count, ClustersRemainingByteSize, FilePtrTarget);
                printf("Total bytes read for buffer %u\n", Result);
                
                NarDumpToFile("STREAM_OVERFLOW_ERROR_LOGS", VolInf->Stream.Records.Data, VolInf->Stream.Records.Count*sizeof(nar_record));
                VolInf->Stream.Error = BackupStream_Errors::Error_Read;
                
                goto ERR_BAIL_OUT;
            }
            
            
        }
        else {
            printf("Couldnt set file pointer to %I64d\n", FilePtrTarget);
            VolInf->Stream.Error = BackupStream_Errors::Error_SetFP;
            goto ERR_BAIL_OUT;
        }
        
        INT32 ClusterToIterate       = (INT32)(BytesReadAfterOperation / VolInf->ClusterSize);
        VolInf->Stream.ClusterIndex += ClusterToIterate;
        
        if ((uint32_t)VolInf->Stream.ClusterIndex > VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].Len) {
            printf("ClusterIndex exceeded region len, that MUST NOT happen at any circumstance\n");
            VolInf->Stream.Error = BackupStream_Errors::Error_SizeOvershoot;
            goto ERR_BAIL_OUT;
        }
        if ((uint32_t)VolInf->Stream.ClusterIndex == VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].Len) {
            VolInf->Stream.ClusterIndex = 0;
            VolInf->Stream.RecIndex++;
        }
        
        
        if (BytesReadAfterOperation % VolInf->ClusterSize != 0) {
            printf("Couldnt read cluster aligned size, error, read %i bytes instead of %i\n", BytesReadAfterOperation, ReadSize);
        }
        
        ASSERT(Result % 4096 == 0);
        if(Result % 4096 != 0){
            NAR_BREAK;
        }
        
        RemainingSize      -= BytesReadAfterOperation;
        CurrentBufferOffset = (char*)BufferToFill + (Result);
        
    }
    
    if(true == VolInf->Stream.ShouldCompress
       && Result > 0){
        
        size_t RetCode = 0;
        
        RetCode = ZSTD_compress2(VolInf->Stream.CCtx, CallerBuffer, CallerBufferSize, BufferToFill, Result);
        
        ASSERT(!ZSTD_isError(RetCode));
        
        if(!ZSTD_isError(RetCode)){
            nar_record CompInfo;
            CompInfo.CompressedSize   = RetCode;
            CompInfo.DecompressedSize = Result;
            
            if(VolInf->MaxCBI > VolInf->CBII){
                VolInf->CompInf[VolInf->CBII++] = CompInfo;
            }
            
            ASSERT(VolInf->MaxCBI > VolInf->CBII);
            
            VolInf->Stream.BytesProcessed = Result;
            Result = RetCode;
        }
        else{
            
#if 0
            if(input.pos != input.size){
                printf("Input buffer size %u, input pos %u\n", input.size, input.pos);
                printf("output buffer size %u, output pos %u\n", output.size, output.pos);
            }
#endif
            
            if (ZSTD_isError(RetCode)) {
                printf("ZSTD Error description : %s\n", ZSTD_getErrorName(RetCode));
            }
            printf("ZSTD RetCode : %I64u\n", RetCode);
            
            VolInf->Stream.Error = BackupStream_Errors::Error_Compression;
            goto ERR_BAIL_OUT;
        }
        
    }
    
    return Result;
    
    ERR_BAIL_OUT:
    
    printf("ReadStream error : %s\n", VolInf->Stream.GetErrorDescription());
    
    Result = 0;
    return Result;
    
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


BOOLEAN
DetachVolume(volume_backup_inf* VolInf) {
    BOOLEAN Return = TRUE;
    HRESULT Result = 0;
    std::wstring Temp = L"!:\\";
    Temp[0] = VolInf->Letter;
    
    Result = FilterDetach(MINISPY_NAME, Temp.c_str(), 0);
    
    if (SUCCEEDED(Result)) {
        
    }
    else {
        printf("Can't detach filter\n");
        printf("Result was -> %d\n", Result);
        DisplayError((DWORD)Result);
        Return = FALSE;
    }
    
    return Return;
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
            // TODO(Batuhan): 
        }
        
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


data_array<nar_record>
GetMFTandINDXLCN(char VolumeLetter, HANDLE VolumeHandle) {
    
    TIMED_BLOCK();
    
    BOOLEAN JustExtractMFTRegions = FALSE;
    
    uint32_t MEMORY_BUFFER_SIZE = 1024LL * 1024LL * 32;
    uint32_t ClusterExtractedBufferSize = 1024 * 1024 * 8;
    uint32_t MaxOutputLen  = ClusterExtractedBufferSize/sizeof(nar_record);
    uint32_t ClusterSize   = NarGetVolumeClusterSize(VolumeLetter);
    
    nar_record* ClustersExtracted      = (nar_record*)malloc(ClusterExtractedBufferSize);
    unsigned int ClusterExtractedCount = 0;
    uint32_t MFTRegionCount            = 0;
    
    if (ClustersExtracted != 0)
        memset(ClustersExtracted, 0, ClusterExtractedBufferSize);
    
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
        printf("Unable to allocate memory for cluster&file buffer\n");
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
            if(NarGetMFTRegionsFromBootSector(VolumeHandle, ClustersExtracted, &MFTRegionCount, MaxOutputLen)){
                ClusterExtractedCount += MFTRegionCount;
            }
            else{
                free(ClustersExtracted);
                ClustersExtracted = 0;
                ClusterExtractedCount = 0;
                goto EARLY_TERMINATION;
            }
            
            
            // TODO (Batuhan): remove this after testing on windows server, looks like rest of the code finds some invalid regions on volume.
            if (JustExtractMFTRegions) {
                printf("Found %i regions\n", ClusterExtractedCount);
                for(uint32_t indx = 0; indx < ClusterExtractedCount; indx++){
                    printf("0x%X\t0x%X\n", ClustersExtracted[indx].StartPos, ClustersExtracted[indx].Len);
                }
                goto EARLY_TERMINATION;
            }
            
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
                        FileRemaining -= TargetFileCount;
                        
                        BOOL RFResult = ReadFile(VolumeHandle, FileBuffer, TargetFileCount * 1024ul, &BR, 0);
                        
                        if (RFResult && BR == (TargetFileCount * 1024ul)) {
#if 1                          
                            
                            int64_t start = NarGetPerfCounter();
                            
                            for (uint64_t FileRecordIndex = 0; FileRecordIndex < TargetFileCount; FileRecordIndex++) {
                                
                                TIMED_NAMED_BLOCK("File record parser");
                                
                                void* FileRecord = (BYTE*)FileBuffer + (uint64_t)FileRecordSize * (uint64_t)FileRecordIndex;
                                
                                if (*(int32_t*)FileRecord != 'ELIF') {
                                    continue;
                                }
                                
                                // lsn, lsa swap to not confuse further parsing stages.
                                ((uint8_t*)FileRecord)[510] = *(uint8_t*)NAR_OFFSET(FileRecord, 50);
                                ((uint8_t*)FileRecord)[511] = *(uint8_t*)NAR_OFFSET(FileRecord, 51);
                                
                                uint32_t FileID = 0;
                                FileID = NarGetFileID(FileRecord);
                                
                                // manually insert $BITMAP & $LogFile data regions to stream.
                                if(FileID == 0||
                                   FileID == 2||
                                   FileID == 6){
                                    void *attr = NarFindFileAttributeFromFileRecord(FileRecord, 0x80);
                                    if(attr){
                                        if(!!(*(uint8_t*)NAR_OFFSET(attr, 8))){
                                            int16_t DataRunOffset = *(int16_t*)NAR_OFFSET(attr, 32);
                                            void* DataRun = NAR_OFFSET(attr, DataRunOffset);
                                            uint32_t RegFound = 0;
                                            if(false == NarParseDataRun(DataRun, &ClustersExtracted[ClusterExtractedCount], MaxOutputLen - ClusterExtractedCount, &RegFound, false)){
                                                printf("Error occured while parsing data run of special file case\n");
                                            }
                                            ClusterExtractedCount += RegFound;
                                        }
                                        else{
                                            printf("Skipped data run parsing for file 0x%X\n", FileID);
                                        }
                                    }
                                    else{
                                        if(FileID == 2)
                                            printf("Unable to find data attribute of $LogFile\n");
                                        if(FileID == 6)
                                            printf("Unable to find data attribute of $BITMAP\n");
                                        if(FileID == 0)
                                            printf("Unable to find data attribute of $MFT\n");
                                    }
                                }
                                
                                if(FileID == 0){
                                    void* MFTBitmap = NarFindFileAttributeFromFileRecord(FileRecord, 0xB0);
                                    if(NULL != MFTBitmap){
                                        uint32_t RegFound = 0;
                                        uint8_t *DataRun = (uint8_t*)NAR_OFFSET(MFTBitmap, 64);
                                        if(false == NarParseDataRun(DataRun, &ClustersExtracted[ClusterExtractedCount], MaxOutputLen - ClusterExtractedCount, &RegFound, false)){
                                            printf("Error occured while parsing data run of special file case\n");
                                        }
                                        ClusterExtractedCount += RegFound;
                                    }
                                    else{
                                        NAR_BREAK;
                                        printf("Unable to find bitmap of mft!\n");
                                    }
                                }
                                
                                void *IndxOffset = NarFindFileAttributeFromFileRecord(FileRecord, NAR_INDEX_ALLOCATION_FLAG);
                                
                                
                                if(IndxOffset != NULL){
                                    TIMED_NAMED_BLOCK("File record parser(valid ones)");
                                    
                                    AutoCompressAndResizeOutput();
                                    
                                    uint32_t RegFound = 0;
                                    NarParseIndexAllocationAttribute(IndxOffset, &ClustersExtracted[ClusterExtractedCount], MaxOutputLen - ClusterExtractedCount, &RegFound,
                                                                     false);
                                    ClusterExtractedCount += RegFound;
                                }
                                
                                // NOTE(Batuhan): EDGE CASE: Attributes that don't fit file record are stored in other file records, and attribute list 
                                // attribute holds which attribute is in which file etc.
                                // But, even attribute list may not fit original file record at all, meaning, it's data may be non-resident
                                // Without this information, some of the files probably may not be accessible at all, or be corrupted.
                                void* ATL = NarFindFileAttributeFromFileRecord(FileRecord, NAR_ATTRIBUTE_LIST);
                                if(NULL != ATL){
                                    uint8_t ATLNonResident = *(uint8_t*)NAR_OFFSET(ATL, 8);
                                    if(ATLNonResident){
                                        TIMED_NAMED_BLOCK("Non resident attrlist!");
                                        void* DataRunStart     = NAR_OFFSET(ATL, 64);;
                                        
                                        uint32_t DataRunFound  = 0;
                                        bool ParseResult       = NarParseDataRun(DataRunStart, 
                                                                                 &ClustersExtracted[ClusterExtractedCount], 
                                                                                 MaxOutputLen - ClusterExtractedCount, 
                                                                                 &DataRunFound, 
                                                                                 false);
                                        
                                        ClusterExtractedCount += DataRunFound;
                                    }
                                }
                                
                                
                            }
                            
#endif
                            
                            double time_sec = NarTimeElapsed(start);
                            printf("Processed %8u files in %.5f sec, file per ms %.5f\n", TargetFileCount, time_sec, (double)TargetFileCount/time_sec/1000.0);
                            
                        }
                        else {
                            printf("Couldnt read file records, read %u bytes instead of %u\n", BR, TargetFileCount*1024);
                            printf("Error code is %X\n", RFResult);
                            DisplayError(RFResult);
                        }
                        
                        
                    }// END OF WHILE(FILE REMAINING)
                    
                }
                else {
                    printf("Couldnt set file pointer to %I64d\n", Offset);
                }
                
            }
        }
        else {
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
    
    size_t DebugRegionCount = 0;
    ULONGLONG VolumeSize = NarGetVolumeTotalSize(VolumeLetter);
    uint32_t TruncateIndex = 0;
    for (uint32_t i = 0; i < Result.Count; i++) {
        if ((ULONGLONG)Result.Data[i].StartPos * (ULONGLONG)ClusterSize + (ULONGLONG)Result.Data[i].Len * ClusterSize > VolumeSize) {
            TruncateIndex = i;
            printf("MFT PARSER : truncation index found %u, out of %u\n", i, Result.Count);
            break;
        }
        else{
            // printf("MFT PARSER region : %9u, length %9u\n", Result.Data[i].StartPos, Result.Data[i].Len);
        }
        DebugRegionCount += (size_t)Result.Data[i].Len;
    }
    
    
    if (TruncateIndex > 0) {
        printf("INDEX_ALLOCATION blocks exceeds volume size, truncating from index %i\n", TruncateIndex);
        Result.Data = (nar_record*)realloc(Result.Data, TruncateIndex * sizeof(nar_record));
        Result.Count = TruncateIndex;
    }
    else{
        
    }
    
    printf("Number of regions found %I64u, mb : %I64u\n", DebugRegionCount, DebugRegionCount*4096ull/(1024ull*1024ull));
    
    return Result;
}




VSS_ID
GetShadowPath(std::wstring Drive, CComPtr<IVssBackupComponents>& ptr, wchar_t* OutShadowPath, size_t MaxOutCh) {
    
    BOOLEAN Error = TRUE;
    VSS_ID sid = { 0 };
    HRESULT res;
    
    res = CreateVssBackupComponents(&ptr);
    
    if(nullptr == ptr){
        printf("createbackup components returned %X\n", res);
        return sid;
        // early termination
    }
    
    if (S_OK == ptr->InitializeForBackup()) {
        if (S_OK == ptr->SetContext(VSS_CTX_BACKUP)) {
            if(S_OK == ptr->SetBackupState(false, false, VSS_BACKUP_TYPE::VSS_BT_COPY, false)){
                CComPtr<IVssAsync> async3;
                if(S_OK == ptr->GatherWriterMetadata(&async3)){
                    async3->Wait();
                    if (S_OK == ptr->StartSnapshotSet(&sid)) {
                        if (S_OK == ptr->AddToSnapshotSet((LPWSTR)Drive.c_str(), GUID_NULL, &sid)) {
                            CComPtr<IVssAsync> Async;
                            if (S_OK == ptr->PrepareForBackup(&Async)) {
                                Async->Wait();
                                CComPtr<IVssAsync> Async2;
                                if (S_OK == ptr->DoSnapshotSet(&Async2)) {
                                    Async2->Wait();
                                    Error = FALSE;
                                }
                                else{
                                    printf("DoSnapshotSet failed\n");
                                }
                            }
                            else{
                                printf("PrepareForBackup failed\n");
                            }
                        }
                        else{
                            printf("AddToSnapshotSet failed\n");
                        }
                    }
                    else{
                        printf("StartSnapshotSet failed\n");
                    }
                }
                else{
                    printf("GatherWriterMetadata failed\n");
                }
            }
            else{
                printf("SetBackupState failed\n");
            }
        }
        else{
            printf("SetContext failed\n");
        }
    }
    else{
        printf("InitializeForBackup failed\n");
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
InitVolumeInf(volume_backup_inf* VolInf, wchar_t Letter, BackupType Type) {
    
    if(VolInf == NULL) return FALSE;
    *VolInf = {0};
    
    BOOLEAN Return = FALSE;
    
    VolInf->Letter = Letter;
    VolInf->INVALIDATEDENTRY = FALSE;
    VolInf->BT = Type;
    
    VolInf->DiffLogMark.BackupStartOffset = 0;
    
    VolInf->Version = -1;
    VolInf->ClusterSize = 0;
    VolInf->VSSPTR = 0;
    
    
    VolInf->Stream.Records = { 0,0 };
    VolInf->Stream.Handle = 0;
    VolInf->Stream.RecIndex = 0;
    VolInf->Stream.ClusterIndex = 0;
    
    DWORD BytesPerSector = 0;
    DWORD SectorsPerCluster = 0;
    DWORD ClusterCount = 0;
    
    wchar_t DN[] = L"C:";
    DN[0] = Letter;
    
    VolInf->BackupID = NarGenerateBackupID((char)Letter);
    
    wchar_t V[] = L"!:\\";
    V[0] = Letter;
    if (GetDiskFreeSpaceW(V, &SectorsPerCluster, &BytesPerSector, 0, &ClusterCount)) {
        VolInf->ClusterSize = SectorsPerCluster * BytesPerSector;
        VolInf->VolumeTotalClusterCount = ClusterCount;
        WCHAR windir[MAX_PATH];
        GetWindowsDirectoryW(windir, MAX_PATH);
        if (windir[0] == Letter) {
            //Selected volume contains windows
            VolInf->IsOSVolume = TRUE;
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
             data_array<nar_record> BackupRegions, nar_backup_id ID, bool IsCompressed, HANDLE VSSHandle, nar_record *CompInfo, size_t CompInfoCount) {
    
    // TODO(Batuhan): convert letter to uppercase
    //Letter += ('A' - 'a');
    
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
    BM.VolumeUsedSize  = NarGetVolumeUsedSize(BM.Letter);
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
    
    
    
    
    {
        // TODO(Batuhan): same problem mentioned in the codebase, we have to figure out reading more than 4gb at once(easy, but gotta replace lots of code probably)
        WriteFile(MetadataFile, BackupRegions.Data, BM.Size.RegionsMetadata, &BytesWritten, 0);
        if (BytesWritten != BM.Size.RegionsMetadata) {
            printf("Couldn't save regionsmetata to file\n");
            BM.Errors.RegionsMetadata = TRUE;
            BM.Size.RegionsMetadata = BytesWritten;
        }
        
        
        uint32_t MFTRecordLen  = 0;
        uint32_t Cap = 1024;
        nar_record *MFTRecords = (nar_record*)calloc(Cap, sizeof(nar_record));
        uint32_t MFTSize = 0;
        bool MFTParseResult = NarGetMFTRegionsFromBootSector(VSSHandle, MFTRecords, &MFTRecordLen, Cap);
        ASSERT(MFTParseResult);
        ASSERT(MFTRecordLen > 0);
        for(uint64_t mi =0; mi<MFTRecordLen; mi++){
            BOOLEAN SFR = NarSetFilePointer(VSSHandle, (uint64_t)MFTRecords[mi].StartPos*(uint64_t)ClusterSize);
            ASSERT(!!SFR);
            CopyData(VSSHandle, MetadataFile, (uint64_t)MFTRecords[mi].Len*(uint64_t)ClusterSize);
            MFTSize += (MFTRecords[mi].Len*ClusterSize);
        }
        
        BM.Size.MFT = MFTSize;
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
    
    BM.CompressionInfoCount  = CompInfo ? CompInfoCount : 0;
    {
        if(CompInfo != NULL){
            ASSERT((uint64_t)CompInfoCount*8ull <= Gigabyte(4));
            DWORD BW = 0;
            
            if(!WriteFile(VSSHandle, CompInfo, CompInfoCount*8, &BW, 0)
               || BW != CompInfoCount*8){
                ASSERT(FALSE);
                printf("Error occured while saving compression information to backup metadata. Volume %c, version %d, id %I64u\n", BM.Letter, BM.Version, BM.ID.Q);
            }
            
            
        }
        
        LARGE_INTEGER liOfs={0};
        LARGE_INTEGER liNew={0};
        SetFilePointerEx(VSSHandle, liOfs, &liNew, FILE_CURRENT);
        BM.CompressionInfoOffset = liNew.QuadPart;
    }
    
    SetFilePointer(MetadataFile, 0, 0, FILE_BEGIN);
    /*
    // NOTE(Batuhan): fill BM.Offset struct
    offset[n] = offset[n-1] + size[n-1]
  offset[0] = baseoffset = struct size
  */
    
    BM.Offset.RegionsMetadata = BaseOffset;
    BM.Offset.MFTMetadata     = BM.Offset.RegionsMetadata + BM.Size.RegionsMetadata;
    BM.Offset.MFT             = BM.Offset.MFTMetadata + BM.Size.MFTMetadata;
    BM.Offset.Recovery        = BM.Offset.MFT + BM.Size.MFT;
    
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
    
    printf("SaveMetadata returns %i\n", Result);
    
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
NarFileNameExtensionCheck(const wchar_t *Path, const wchar_t *Extension){
    TIMED_BLOCK();
    size_t pl = wcslen(Path);
    size_t el = wcslen(Extension);
    if(pl <= el) return FALSE;
    return (wcscmp(&Path[pl - el], Extension) == 0);
}



VOID
DisplayError(DWORD Code) {
    
    WCHAR buffer[MAX_PATH] = { 0 };
    DWORD count;
    HMODULE module = NULL;
    HRESULT status;
    
    count = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM,
                           NULL,
                           Code,
                           0,
                           buffer,
                           sizeof(buffer) / sizeof(WCHAR),
                           NULL);
    
    
    if (count == 0) {
        
        count = GetSystemDirectoryW(buffer,
                                    sizeof(buffer) / sizeof(WCHAR));
        
        if (count == 0 || count > sizeof(buffer) / sizeof(WCHAR)) {
            
            //
            //  In practice we expect buffer to be large enough to hold the
            //  system directory path.
            //
            
            printf("Could not translate error: %d\n", Code);
            return;
        }
        
        
        status = StringCchCat(buffer,
                              sizeof(buffer) / sizeof(WCHAR),
                              L"\\fltlib.dll");
        
        if (status != S_OK) {
            
            printf("Could not translate error: %d\n", Code);
            return;
        }
        
        module = LoadLibraryExW(buffer, NULL, LOAD_LIBRARY_AS_DATAFILE);
        
        //
        //  Translate the Win32 error code into a useful message.
        //
        
        count = FormatMessageW(FORMAT_MESSAGE_FROM_HMODULE,
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


int
CompareNarRecords(const void* v1, const void* v2) {
    
    nar_record* n1 = (nar_record*)v1;
    nar_record* n2 = (nar_record*)v2;
    
#if 0    
    if(n1->StartPos == n2->StartPos){
        return (int)((int64_t)n1->Len - (int64_t)n2->Len);
    }
    else{
        return (int)((int64_t)n1->StartPos - (int64_t)n2->StartPos);
    }
#endif
    
#if 1    
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