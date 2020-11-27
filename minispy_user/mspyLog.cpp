#include <DriverSpecs.h>
_Analysis_mode_(_Analysis_code_type_user_code_)



#define TIME_BUFFER_LENGTH 20
#define TIME_ERROR         "time error"

#define POLL_INTERVAL   (10) // ms

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif


DWORD
WINAPI
RetrieveLogRecords(
                   _In_ LPVOID lpParameter
                   )
/*++

Routine Description:

    This runs as a separate thread.  Its job is to retrieve log records
    from the filter and then output them

Arguments:

    lpParameter - Contains context structure for synchronizing with the
        main program thread.

Return Value:

    The thread successfully terminated

--*/
{
    printf("Msging thread started\n");
    
    PLOG_CONTEXT context = (PLOG_CONTEXT)lpParameter;
    HRESULT hResult;
    
    
    //printf("Log: Starting up\n");
    std::wstring LogDOSName = L"";
#pragma warning(push)
#pragma warning(disable:4127) // conditional expression is constant
    Sleep(POLL_INTERVAL);
    void* OutBuffer = malloc(NAR_MEMORYBUFFER_SIZE);
    while (TRUE) {
        
#pragma warning(pop)
        
        //
        //  Check to see if we should shut down.
        //
        
        if (context->CleaningUp) {
            break;
        }
        
        static constexpr int USERGUIDSTRLEN = 50;
        static constexpr int WSTRBUFFERLEN = 256;
        static WCHAR wStrBuffer[256];
        memset(wStrBuffer, 0, WSTRBUFFERLEN * sizeof(WCHAR));
        
        for (unsigned int VolumeIndex = 0; VolumeIndex < context->Volumes.Count; VolumeIndex++) {
            
            volume_backup_inf* V = &context->Volumes.Data[VolumeIndex];
            
            // TODO(BATUHAN): CHECK IF V LETTER IS ACTUALLY A LETTER
            if (V == NULL || V->INVALIDATEDENTRY || V->Letter == 0)
            {
                continue;
            }
            
            NAR_COMMAND Command;
            
            wsprintfW(wStrBuffer, L"%c:\\", V->Letter);
            
            
            if (NarGetVolumeGUIDKernelCompatible(V->Letter, Command.VolumeGUIDStr)) {
                
                DWORD Hell = 0;
                
                memset(OutBuffer, 0, NAR_MEMORYBUFFER_SIZE);
                Command.Type = NarCommandType_GetVolumeLog;
                
                hResult = FilterSendMessage(context->Port, &Command, sizeof(NAR_COMMAND), OutBuffer, NAR_MEMORYBUFFER_SIZE, &Hell);
                
                if (SUCCEEDED(hResult)) {
                    
                    if (!NAR_MB_ERROR_OCCURED(OutBuffer)) {
                        
                        // if volume isnt active, just fetch all data and dump it, thats it
                        if (!V->FilterFlags.IsActive) {
                            continue;
                        }
                        
                        
                        INT32 DataUsed = (INT32)NAR_MB_DATA_USED(OutBuffer);
                        
                        if (DataUsed > 0 && DataUsed < NAR_MEMORYBUFFER_SIZE) {
                            
                            UINT32 RecCount = NAR_MB_DATA_USED(OutBuffer)/sizeof(nar_record);
                            nar_record *Recs = (nar_record*)NAR_MB_DATA(OutBuffer);
                            DWORD MSDNRetVAL = 0;
                            
                            MSDNRetVAL = WaitForSingleObject(V->FileWriteMutex, 250);
                            if(MSDNRetVAL != WAIT_OBJECT_0){
                                printf("Couldnt lock mutex to write records to file\n");
                                continue;
                            }
                            
                            FlushFileBuffers(V->LogHandle);
                            SetFilePointer(V->LogHandle, 0, 0, FILE_END); // set it to the append mode
                            
                            for (unsigned int TempIndex = 0; TempIndex < RecCount; TempIndex++) {
                                
                                if (Recs[TempIndex].StartPos + Recs[TempIndex].Len < V->VolumeTotalClusterCount) {
                                    FileDump(&Recs[TempIndex], sizeof(nar_record), V->LogHandle);        
                                }
                                
                            }
                            
                            ReleaseMutex(V->FileWriteMutex);
                            
                        }
                        else {
                            
                            if (DataUsed < 0) {
                                printf("Data used was lower than zero %i, GUID : %S\n", DataUsed, Command.VolumeGUIDStr);
                            }
                            if (DataUsed > NAR_MEMORYBUFFER_SIZE) {
                                printf("Data size exceeded buffer size itself, this is a fatal error(size = %i), GUID : %S\n", DataUsed, Command.VolumeGUIDStr);
                            }
                            
                        }
                        
                        
                        
                    }
                    else {
                        printf("Error flag detected in memory buffer, volume %c buffer size %i\n", (char)V->Letter, NAR_MB_USED(OutBuffer));
                    }
                    
                }
                else {
                    
                    if (IS_ERROR(hResult)) {
                        
                        if (HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE) == hResult) {
                            
                            printf("The kernel component of minispy has unloaded. Exiting\n");
                            ExitProcess(0);
                        }
                        else {
                            
                            if (hResult != HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS)) {
                                printf("UNEXPECTED ERROR received: %x\n", hResult);
                            }
                            
                        }
                        
                        continue;
                    }
                    
                }
                
                
            }
            else {
                printf("Couldnt get volume GUID for volume %c\n", V->Letter);
            }
            
            
            
        }
        
        Sleep(POLL_INTERVAL);
        
    }
    
    free(OutBuffer);
    printf("Log: Shutting down\n");
    ReleaseSemaphore(context->ShutDown, 1, NULL);
    printf("Log: All done\n");
    return 0;
}




BOOL
FileDump(
         _In_ PVOID Data,
         _In_ INT32 DataSize,
         _In_ HANDLE File
         )
/*++
Routine Description:

    Prints a Data log record to the specified file.  The output is in a tab
    delimited format with the fields in the following order:

    SequenceNumber, OriginatingTime, CompletionTime, CallbackMajorId, CallbackMinorId,
    Flags, NoCache, Paging I/O, Synchronous, Synchronous paging, FileName,
    ReturnStatus, FileName

Arguments:

    RecordData - the Data record to print
    File - the file to print to

Return Value:

    None.

--*/
{
    DWORD BytesWritten = 0;
    DWORD Result = TRUE;
    
    Result = WriteFile(File, Data, DataSize, &BytesWritten, 0);
    if (!SUCCEEDED(Result) || BytesWritten != (DWORD)DataSize) {
        printf("Error occured!\n");
        printf("Bytes written -> %d, BytesToWrite -> %d\n", BytesWritten, DataSize);
        DisplayError(GetLastError());
        Result = FALSE;
    }
    
    return Result;
}
