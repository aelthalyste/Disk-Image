/*++

Copyright (c) 1989-2002  Microsoft Corporation

Module Name:

  MiniSpy.c

Abstract:

  This is the main module for the MiniSpy mini-filter.

Environment:

  Kernel mode

--*/

#define NAR_KERNEL 1

#include "mspyKern.h"
#include <stdio.h>
#include <Ntstrsafe.h>
#include <string.h>
#define DbgPrint

//
//  Global variables
//

nar_data NarData;
NTSTATUS StatusToBreakOn = 0;

//NAR

//---------------------------------------------------------------------------
//  Function prototypes
//---------------------------------------------------------------------------
DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry(
            _In_ PDRIVER_OBJECT DriverObject,
            _In_ PUNICODE_STRING RegistryPath
            );


NTSTATUS
SpyMessage(
           _In_ PVOID ConnectionCookie,
           _In_reads_bytes_opt_(InputBufferSize) PVOID InputBuffer,
           _In_ ULONG InputBufferSize,
           _Out_writes_bytes_to_opt_(OutputBufferSize, *ReturnOutputBufferLength) PVOID OutputBuffer,
           _In_ ULONG OutputBufferSize,
           _Out_ PULONG ReturnOutputBufferLength
           );

NTSTATUS
SpyConnect(
           _In_ PFLT_PORT ClientPort,
           _In_ PVOID ServerPortCookie,
           _In_reads_bytes_(SizeOfContext) PVOID ConnectionContext,
           _In_ ULONG SizeOfContext,
           _Flt_ConnectionCookie_Outptr_ PVOID* ConnectionCookie
           );

VOID
SpyDisconnect(
              _In_opt_ PVOID ConnectionCookie
              );


NTSTATUS
SpyVolumeInstanceSetup(
                       _In_ PCFLT_RELATED_OBJECTS FltObjects,
                       _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
                       _In_ DEVICE_TYPE VolumeDeviceType,
                       _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
                       );

//---------------------------------------------------------------------------
//  Assign text sections for each routine.
//---------------------------------------------------------------------------

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, SpyFilterUnload)
#pragma alloc_text(PAGE, SpyQueryTeardown)
#pragma alloc_text(PAGE, SpyConnect)
#pragma alloc_text(PAGE, SpyDisconnect)
#pragma alloc_text(PAGE, SpyMessage)
#pragma alloc_text(PAGE, SpyVolumeInstanceSetup)
#endif


#define SetFlagInterlocked(_ptrFlags,_flagToSet) \
((VOID)InterlockedOr(((volatile LONG *)(_ptrFlags)),_flagToSet))

//---------------------------------------------------------------------------
//                      ROUTINES
//---------------------------------------------------------------------------

NTSTATUS
DriverEntry(
            _In_ PDRIVER_OBJECT DriverObject,
            _In_ PUNICODE_STRING RegistryPath
            )
/*++

Routine Description:

  This routine is called when a driver first loads.  Its purpose is to
  initialize global state and then register with FltMgr to start filtering.

Arguments:

  DriverObject - Pointer to driver object created by the system to
    represent this driver.
  RegistryPath - Unicode string identifying where the parameters for this
    driver are located in the registry.

Return Value:

  Status of the operation.

--*/
{
    PSECURITY_DESCRIPTOR sd;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING uniString;
    NTSTATUS status = STATUS_SUCCESS;
    
    UNREFERENCED_PARAMETER(RegistryPath);
    
    try {
        
        memset(&NarData, 0, sizeof(NarData));
        
        //
        // Initialize global data structures.
        //
        
        NarData.NameQueryMethod = DEFAULT_NAME_QUERY_METHOD;
        
        NarData.DriverObject = DriverObject;
        
        
        //
        //  Now that our global configuration is complete, register with FltMgr.
        //
        
        status = FltRegisterFilter(DriverObject,
                                   &FilterRegistration,
                                   &NarData.Filter);
        
        if (!NT_SUCCESS(status)) {
            DbgPrint("Register filter failed\n");
            
            leave;
        }
        
        
        status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);
        
        if (!NT_SUCCESS(status)) {
            DbgPrint("Build default security descriptor failed\n");
            
            leave;
        }
        
        RtlInitUnicodeString(&uniString, MINISPY_PORT_NAME);
        
        InitializeObjectAttributes(&oa,
                                   &uniString,
                                   OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                                   NULL,
                                   sd);
        
        status = FltCreateCommunicationPort(NarData.Filter,
                                            &NarData.ServerPort,
                                            &oa,
                                            NULL,
                                            SpyConnect,
                                            SpyDisconnect,
                                            SpyMessage,
                                            1);
        
        FltFreeSecurityDescriptor(sd);
        
        if (!NT_SUCCESS(status)) {
            if (status == STATUS_OBJECT_NAME_COLLISION) {
                DbgPrint("NAME_COLLISION");
            }
            if (status == STATUS_INSUFFICIENT_RESOURCES) {
                DbgPrint("INSUFFICENT_RESOURCES");
                
            }
            if (status == STATUS_FLT_DELETING_OBJECT) {
                DbgPrint("FLT_DELETING_OBJECT");
            }
            DbgPrint("Create comm failed %i\n", status);
            
            __leave;
        }
        
        
        
        DbgPrint("Driver entry!\n");
        
        
#if 1
        nar_boot_track_data TrackedVolumes[NAR_MAX_VOLUME_COUNT];
        int TrackedVolumesCount = 0;
        memset(TrackedVolumes, 0, sizeof(TrackedVolumes));
        
        UNICODE_STRING     FileName;
        OBJECT_ATTRIBUTES  objAttr;
        
        void* UnicodeStringBuffer = ExAllocatePoolWithTag(PagedPool, 512, NAR_TAG);
        RtlInitEmptyUnicodeString(&FileName, UnicodeStringBuffer, 512);
        status = RtlUnicodeStringCatString(&FileName, L"\\SystemRoot\\");
        if (NT_SUCCESS(status)) {
            status = RtlUnicodeStringCatString(&FileName, NAR_BOOTFILE_W_NAME);
            if (NT_SUCCESS(status)) {
                DbgPrint("Succ created unicode string %wZ\n", &FileName);
            }
            else {
                DbgPrint("Couldnt append narbootifle name to unicode string, status : %i\n", status);
            }
        }
        else {
            DbgPrint("Couldnt append systemroot string to unicode string, status : %i\n", status);
        }
        
        
        if (NT_SUCCESS(status)) {
            
            InitializeObjectAttributes(&objAttr, &FileName,
                                       OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                                       NULL, NULL);
            
            IO_STATUS_BLOCK IOSB = { 0 };
            HANDLE FileHandle;
            
            status = ZwCreateFile(&FileHandle,
                                  GENERIC_READ,
                                  &objAttr, &IOSB, NULL,
                                  FILE_ATTRIBUTE_NORMAL,
                                  0,
                                  FILE_OPEN,
                                  FILE_SYNCHRONOUS_IO_NONALERT | FILE_RANDOM_ACCESS,
                                  NULL, 0);
            
            if (NT_SUCCESS(status)) {
                DbgPrint("Found metadata file\n");
                FILE_STANDARD_INFORMATION  FI;
                INT32 FileSize = 0;
                status = ZwQueryInformationFile(FileHandle, &IOSB, &FI, sizeof(FI), FileStandardInformation);
                if (NT_SUCCESS(status)) {
                    DbgPrint("Succ queried standard file information");
                    
                    if (FI.EndOfFile.QuadPart >= sizeof(TrackedVolumes)) {
                        DbgPrint("File is too big\n");
                    }
                    else if (FI.EndOfFile.QuadPart % sizeof(TrackedVolumes[0]) != 0) {
                        DbgPrint("File is not aligned\n");
                        DbgPrint("Struct size %i\n", sizeof(TrackedVolumes[0]));
                    }
                    else {
                        FileSize = (INT32)(FI.EndOfFile.QuadPart); // safe conversion
                    }
                    DbgPrint("File size %i\n", FI.EndOfFile.QuadPart);
                    
                    if (FileSize > 0) {
                        TrackedVolumesCount = FileSize / sizeof(TrackedVolumes[0]);
                        
                        status = ZwReadFile(FileHandle, 0, 0, 0, &IOSB, &TrackedVolumes[0], FileSize, 0, 0);
                        if (NT_SUCCESS(status)) {
                            DbgPrint("Succ read metadata at driver entry\n");
                        }
                        else {
                            DbgPrint("Couldnt read file, status : %i\n", status);
                        }
                    }
                    
                    
                }
                else {
                    DbgPrint("Couldnt query standard file information\n");
                }
                
                ZwClose(FileHandle);
                
            }
            else {
                DbgPrint("Metadata file doesnt exist\n");
                // metadatafile doesnt exist, thats not an error.
            }
            
        }
        
        ExFreePoolWithTag(UnicodeStringBuffer, NAR_TAG);
        
#endif
        
        
        
        memset(NarData.VolumeRegionBuffer, 0, NAR_VOLUMEREGIONBUFFERSIZE);
        
        //Initializing each volume entry
        for (int i = 0; i < NAR_MAX_VOLUME_COUNT; i++) {
            
            ExInitializeFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
            NarData.VolumeRegionBuffer[i].MemoryBuffer = 0;
            NarData.VolumeRegionBuffer[i].MemoryBuffer = ExAllocatePoolWithTag(NonPagedPool, NAR_MEMORYBUFFER_SIZE, NAR_TAG);
            
            NarData.VolumeRegionBuffer[i].GUIDStrVol.Length = 0;
            NarData.VolumeRegionBuffer[i].GUIDStrVol.MaximumLength = sizeof(NarData.VolumeRegionBuffer[i].Reserved);
            NarData.VolumeRegionBuffer[i].GUIDStrVol.Buffer = NarData.VolumeRegionBuffer[i].Reserved;
            memset(&NarData.VolumeRegionBuffer[i].Reserved[0], 0, sizeof(NarData.VolumeRegionBuffer[i].Reserved));
            
            if (NarData.VolumeRegionBuffer[i].MemoryBuffer == NULL) {
                status = STATUS_FAILED_DRIVER_ENTRY;
                leave;
            }
            
            memset(NarData.VolumeRegionBuffer[i].MemoryBuffer, 0, NAR_MEMORYBUFFER_SIZE);
            NAR_INIT_MEMORYBUFFER(NarData.VolumeRegionBuffer[i].MemoryBuffer);
            DbgPrint("Initialized volume entry %i\n", i);
            
        }
        
        
        
        //
        //  We are now ready to start filtering
        //
        
        status = FltStartFiltering(NarData.Filter);
        
        
        char UniBuffer[32];
        DbgPrint("Found %i volumes at boot file, will initialize accordingly\n", TrackedVolumesCount);
        
        for (int i = 0; i < TrackedVolumesCount; i++) {
            
            DbgPrint("Will try to add volume %c to filter\n", TrackedVolumes[i].Letter);
            
            memset(UniBuffer, 0, sizeof(UniBuffer));
            UNICODE_STRING VolumeNameUniStr = { 0 };
            RtlInitEmptyUnicodeString(&VolumeNameUniStr, UniBuffer, sizeof(UniBuffer));
            
            status = RtlUnicodeStringPrintf(&VolumeNameUniStr, L"%c:", TrackedVolumes[i].Letter);
            if (NT_SUCCESS(status)) {
                PFLT_VOLUME Volume;
                
                // For every succ call, we must deference PFLT_VOLUME
                status = FltGetVolumeFromName(NarData.Filter, &VolumeNameUniStr, &Volume);
                if (NT_SUCCESS(status)) {
                    
                    DbgPrint("Succ get volume name from volume letter (%c)\n", TrackedVolumes[i].Letter);
                    
                    ULONG BufferNeeded = 0;
                    status = FltGetVolumeGuidName(Volume, &NarData.VolumeRegionBuffer[i].GUIDStrVol, &BufferNeeded);
                    
                    if (NT_SUCCESS(status)) {
                        
                        DbgPrint("Before fltattach \n");
                        status = FltAttachVolume(NarData.Filter, Volume, NULL, NULL);
                        DbgPrint("After FltAttachVolume, status %X\n", status);
                        if (status == STATUS_SUCCESS) {
                            
                            DbgPrint("Successfully attached volume (%c) GUID : (%wZ)\n", TrackedVolumes[i].Letter, &NarData.VolumeRegionBuffer[i].GUIDStrVol);
                            DbgPrint("GUID len %i, max len %i\n", NarData.VolumeRegionBuffer[i].GUIDStrVol.Length, NarData.VolumeRegionBuffer[i].GUIDStrVol.MaximumLength);
                            NarData.VolumeRegionBuffer[i].VolFileID = NAR_KERNEL_GEN_FILE_ID(TrackedVolumes[i].Letter);
                            
                        }
                        else {
                            DbgPrint("Couldnt attach volume volume %c, GUID: %wZ, status %i\n", TrackedVolumes[i].Letter, &NarData.VolumeRegionBuffer[i].GUIDStrVol, status);
                            // revert guid str to null again
                            memset(NarData.VolumeRegionBuffer[i].Reserved, 0, NAR_GUID_STR_SIZE);
                            NarData.VolumeRegionBuffer[i].GUIDStrVol.Length = 0;
                        }
                        
                    }
                    else {
                        DbgPrint("GetVolumeGUIDName failed\n");
                    }
                    
                    FltObjectDereference(Volume);
                }
                else {
                    DbgPrint("Couldnt get volume from name\n");
                }
            }
            else {
                
                DbgPrint("Sprintf failed, volume letter %c, uni str %wZ", TrackedVolumes[i].Letter, &VolumeNameUniStr);
                
            }
        }
        
        
        
        NarData.FileHandles = ExAllocatePoolWithTag(NonPagedPool, sizeof(HANDLE) * NAR_KERNEL_MAX_FILE_ID, NAR_TAG);
        for (char c = 'A'; c <= 'Z'; c++) {
            
            wchar_t NameTemp[] = L"\\SystemRoot\\NAR_LOG_FILE__.nlfx";
            NameTemp[54 / 2 - 2] = (wchar_t)c;
            
            UNICODE_STRING Fn = { 0 };
            RtlInitUnicodeString(&Fn, NameTemp);
            
            IO_STATUS_BLOCK iosb;
            OBJECT_ATTRIBUTES tobjAttr;
            
            InitializeObjectAttributes(&tobjAttr, &Fn, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
            
            status = ZwCreateFile(&NarData.FileHandles[NAR_KERNEL_GEN_FILE_ID(c)], FILE_APPEND_DATA | SYNCHRONIZE, &tobjAttr, &iosb, NULL,
                                  FILE_ATTRIBUTE_NORMAL,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN_IF,
                                  FILE_SEQUENTIAL_ONLY | FILE_WRITE_THROUGH | FILE_SYNCHRONOUS_IO_NONALERT,
                                  NULL, 0);
            
            if (!NT_SUCCESS(status)) {
                DbgPrint("Unable to create file\n");
            }
            else {
                DbgPrint("Log file opened for volume %c", c);
            }
            
        }
        
        
    }
    finally {
        
        if (!NT_SUCCESS(status)) {
            
            DbgPrint("[FATAL]: Driver entry failed!\n");
            if (NULL != NarData.ServerPort) {
                FltCloseCommunicationPort(NarData.ServerPort);
            }
            
            if (NULL != NarData.Filter) {
                FltUnregisterFilter(NarData.Filter);
            }
            
            if (NarData.VolumeRegionBuffer != NULL) {
                
                for (int i = 0; i < NAR_MAX_VOLUME_COUNT; i++) {
                    if (NarData.VolumeRegionBuffer[i].MemoryBuffer != NULL) {
                        DbgPrint("Freeing volume memory buffer %i\n", i);
                        ExFreePoolWithTag(NarData.VolumeRegionBuffer[i].MemoryBuffer, NAR_TAG);
                    }
                    else {
                        break;
                    }
                }
                
                
            }// If nardata.volumeregionbuffer != NULL
            
            ExFreePoolWithTag(NarData.FileHandles, NAR_TAG);
            
            
        }
    }
    
    return status;
}

NTSTATUS
SpyConnect(
           _In_ PFLT_PORT ClientPort,
           _In_ PVOID ServerPortCookie,
           _In_reads_bytes_(SizeOfContext) PVOID ConnectionContext,
           _In_ ULONG SizeOfContext,
           _Flt_ConnectionCookie_Outptr_ PVOID* ConnectionCookie
           )
/*++

Routine Description

  This is called when user-mode connects to the server
  port - to establish a connection

Arguments

  ClientPort - This is the pointer to the client port that
    will be used to send messages from the filter.
  ServerPortCookie - unused
  ConnectionContext - unused
  SizeofContext   - unused
  ConnectionCookie - unused

Return Value

  STATUS_SUCCESS - to accept the connection
--*/
{
    
    PAGED_CODE();
    UNREFERENCED_PARAMETER(ClientPort);
    UNREFERENCED_PARAMETER(ServerPortCookie);
    UNREFERENCED_PARAMETER(ConnectionContext);
    UNREFERENCED_PARAMETER(SizeOfContext);
    UNREFERENCED_PARAMETER(ConnectionCookie);
    
    
    NAR_CONNECTION_CONTEXT* CTX = ConnectionContext;
    NarData.ClientPort = ClientPort;
    NarData.UserModePID = CTX->PID;
    
    
    
    //Copy user name
    
    return STATUS_SUCCESS;
}




VOID
SpyDisconnect(
              _In_opt_ PVOID ConnectionCookie
              )
/*++

Routine Description

  This is called when the connection is torn-down. We use it to close our handle to the connection

Arguments

  ConnectionCookie - unused

Return value

  None
--*/
{
    
    PAGED_CODE();
    
    UNREFERENCED_PARAMETER(ConnectionCookie);
    
    //
    //  Close our handle
    //
    
    FltCloseClientPort(NarData.Filter, &NarData.ClientPort);
}

NTSTATUS
SpyFilterUnload(
                _In_ FLT_FILTER_UNLOAD_FLAGS Flags
                )
/*++

Routine Description:

  This is called when a request has been made to unload the filter.  Unload
  requests from the Operation System (ex: "sc stop minispy" can not be
  failed.  Other unload requests may be failed.

  You can disallow OS unload request by setting the
  FLTREGFL_DO_NOT_SUPPORT_SERVICE_STOP flag in the FLT_REGISTARTION
  structure.

Arguments:

  Flags - Flags pertinent to this operation

Return Value:

  Always success

--*/
{
    UNREFERENCED_PARAMETER(Flags);
    
    PAGED_CODE();
    
    if(Flags == 0){
        DbgPrint("Unload called with NULL Flags parameter\n");
    }
    
    if((Flags & FLTFL_FILTER_UNLOAD_MANDATORY) == FLTFL_FILTER_UNLOAD_MANDATORY){
        DbgPrint("Unload called with FLTFL_FILTER_UNLOAD_MANDATORY Flags parameter\n");
    }
    
    //
    //  Close the server port. This will stop new connections.
    //
    
    DbgPrint("Filter unloading\n");
    
    FltCloseCommunicationPort(NarData.ServerPort);
    DbgPrint("Communication port closed\n");
    
    //NTSTATUS status;
    
    FltUnregisterFilter(NarData.Filter);
    DbgPrint("Filter unregistered\n");
    
    
    {
        for (int i = 0; i < NAR_MAX_VOLUME_COUNT; i++) {
            
            // since we just cancelled all pending operations, we dont have to acquire mutexes nor release them
            
            ExAcquireFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
            
            
            // If volume is active, flush it's logs
            /*
                        if (NarData.VolumeRegionBuffer[i].VolFileID != NAR_KERNEL_INVALID_FILE_ID) {
                            nar_log_thread_params* tp = (nar_log_thread_params*)ExAllocatePoolWithTag(NonPagedPool, sizeof(nar_log_thread_params), NAR_TAG);
                            
                            if (tp != NULL) {
                                memset(tp, 0, sizeof(*tp));
                                
                                tp->Data                = NAR_MB_DATA(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                                tp->DataLen             = NAR_MB_DATA_USED(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                                tp->FileID              = NarData.VolumeRegionBuffer[i].VolFileID;
                                tp->ShouldFlush         = TRUE;
                                tp->ShouldQueryFileSize = FALSE;
                                
                                DbgPrint("Flush at filterunload with data length %u, file id : %u", tp->DataLen, tp->FileID);
                                status = NarWriteLogsToFile(tp, 0);
                                
                                if (NT_SUCCESS(status)) {
                                    
                                }
                                else {
                                    
                                }
                                
                                NAR_INIT_MEMORYBUFFER(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                                ExFreePoolWithTag(tp, NAR_TAG);
                                
                            }
                            else {
                                DbgPrint("Unable to allocate memory to flush logs at filterunload routine\n");
                            }
                        }
                        
                        */
            
            // check if there are still logs that need to be flushed
            
            // invalidate volume name so no thread gains access to invalid memory adress
            memset(&NarData.VolumeRegionBuffer[i].Reserved[0], 0, sizeof(NarData.VolumeRegionBuffer[i].Reserved));
            
            // checking with NULL is dumb at kernel level
            if (NarData.VolumeRegionBuffer[i].MemoryBuffer != NULL) {
                ExFreePoolWithTag(NarData.VolumeRegionBuffer[i].MemoryBuffer, NAR_TAG);
                NarData.VolumeRegionBuffer[i].MemoryBuffer = 0;
            }
            ExReleaseFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
            
        }
        
        
    }// If nardata.volumeregionbuffer != NULL
    
    
    
    for (size_t i = 1; i < NAR_KERNEL_MAX_FILE_ID; i++) {
        ZwClose(NarData.FileHandles[i]);
    }
    
    ExFreePoolWithTag(NarData.FileHandles, NAR_TAG);
    
    
    return STATUS_SUCCESS;
}

NTSTATUS
SpyTeardownStart(
                 _In_ PCFLT_RELATED_OBJECTS FltObjects,
                 _In_ FLT_INSTANCE_TEARDOWN_FLAGS Reason
                 ) {
    
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Reason);
    DbgPrint("Teardownstart called\n");
    
    return STATUS_SUCCESS;
}


void
SpyTeardownComplete(
                    _In_ PCFLT_RELATED_OBJECTS FltObjects,
                    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Reason
                    ) {
    
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Reason);
    DbgPrint("Teardown complete\n");
    
    return;
}

NTSTATUS
SpyQueryTeardown(
                 _In_ PCFLT_RELATED_OBJECTS FltObjects,
                 _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
                 )
/*++

Routine Description:

  This allows our filter to be manually detached from a volume.

Arguments:

  FltObjects - Contains pointer to relevant objects for this operation.
    Note that the FileObject field will always be NULL.

  Flags - Flags pertinent to this operation

Return Value:

--*/
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    PAGED_CODE();
    
    DbgPrint("Query Teardown called\n");
    
    return STATUS_SUCCESS;
}


NTSTATUS
SpyMessage(
           _In_ PVOID ConnectionCookie,
           _In_reads_bytes_opt_(InputBufferSize) PVOID InputBuffer,
           _In_ ULONG InputBufferSize,
           _Out_writes_bytes_to_opt_(OutputBufferSize, *ReturnOutputBufferLength) PVOID OutputBuffer,
           _In_ ULONG OutputBufferSize,
           _Out_ PULONG ReturnOutputBufferLength
           )
/*++

Routine Description:

  This is called whenever a user mode application wishes to communicate
  with this minifilter.

Arguments:

  ConnectionCookie - unused

  OperationCode - An identifier describing what type of message this
    is.  These codes are defined by the MiniFilter.
  InputBuffer - A buffer containing input data, can be NULL if there
    is no input data.
  InputBufferSize - The size in bytes of the InputBuffer.
  OutputBuffer - A buffer provided by the application that originated
    the communication in which to store data to be returned to this
    application.
  OutputBufferSize - The size in bytes of the OutputBuffer.
  ReturnOutputBufferSize - The size in bytes of meaningful data
    returned in the OutputBuffer.

Return Value:

  Returns the status of processing the message.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    
    PAGED_CODE();
    
    UNREFERENCED_PARAMETER(ConnectionCookie);
    UNREFERENCED_PARAMETER(ReturnOutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferSize);
    UNREFERENCED_PARAMETER(InputBuffer);
    
    //
    //                      **** PLEASE READ ****
    //
    //  The INPUT and OUTPUT buffers are raw user mode addresses.  The filter
    //  manager has already done a ProbedForRead (on InputBuffer) and
    //  ProbedForWrite (on OutputBuffer) which guarentees they are valid
    //  addresses based on the access (user mode vs. kernel mode).  The
    //  minifilter does not need to do their own probe.
    //
    //  The filter manager is NOT doing any alignment checking on the pointers.
    //  The minifilter must do this themselves if they care (see below).
    //
    //  The minifilter MUST continue to use a try/except around any access to
    //  these buffers.
    //
    
    if ((InputBuffer != NULL) && InputBufferSize >= sizeof(NAR_COMMAND)) {
        
        
        
        try {
            
            if (!IS_ALIGNED(OutputBuffer, sizeof(PVOID))) {
                DbgPrint("Outputbuffer not aligned accordingly\n");
            }
            
            //
            //  Probe and capture input message: the message is raw user mode
            //  buffer, so need to protect with exception handler
            //
            
            
            NAR_COMMAND* Command = (NAR_COMMAND*)InputBuffer;
            if(Command->Type == NarCommandType_FlushLog){
                
                if(OutputBuffer != 0 && OutputBufferSize >= sizeof(NAR_LOG_INF) && InputBuffer != 0){
                    
                    NAR_LOG_INF* li = (NAR_LOG_INF*)OutputBuffer;
                    NAR_COMMAND* Cmd = (NAR_COMMAND*)InputBuffer;
                    
                    INT32 Found = FALSE;
                    
                    for(size_t i = 0; i < NAR_MAX_VOLUME_COUNT; i++){
                        
                        ExAcquireFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                        
                        if(NarData.VolumeRegionBuffer[i].Letter == Cmd->Letter){
                            Found = TRUE;
                            
                            // flush file contents
                            nar_log_thread_params* tp = (nar_log_thread_params*)ExAllocatePoolWithTag(NonPagedPool, sizeof(nar_log_thread_params), NAR_TAG);
                            
                            if (tp != NULL) {
                                memset(tp, 0, sizeof(*tp));
                                tp->Data = NAR_MB_DATA(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                                tp->DataLen = NAR_MB_DATA_USED(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                                tp->FileID = NarData.VolumeRegionBuffer[i].VolFileID;
                                tp->ShouldFlush = TRUE;
                                tp->ShouldQueryFileSize = TRUE;
                                
                                NarWriteLogsToFile(tp, 0);
                                
                                li->CurrentSize = tp->FileSize;
                                li->ErrorOccured = !NT_SUCCESS(tp->InternalError);
                                
                                NAR_INIT_MEMORYBUFFER(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                                
                                ExFreePoolWithTag(tp, NAR_TAG);
                            }
                            
                            
                            ExReleaseFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                            break;
                        }
                        
                        ExReleaseFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);    
                    }
                    
                    if (Found == FALSE) {
                        DbgPrint("Couldnt find volume %c in kernel list", Cmd->Letter);
                    }
                    
                    li->CurrentSize;
                    li->ErrorOccured;
                    
                }
                
            }
            if (Command->Type == NarCommandType_AddVolume) {
                
                DbgPrint("Command add volume received\n");
                
                INT32 VolumeAdded = FALSE;
                if (Command->VolumeGUIDStr != NULL) {
                    
                    void* PagedGUIDStrBuffer = ExAllocatePoolWithTag(PagedPool, NAR_GUID_STR_SIZE, NAR_TAG);
                    if (PagedGUIDStrBuffer != NULL) {
                        
                        memcpy(PagedGUIDStrBuffer, Command->VolumeGUIDStr, NAR_GUID_STR_SIZE);
                        
                        INT32 VolumeAlreadyExists = FALSE;
                        DbgPrint("Will iterate volume list\n");
                        
                        for (int i = 0; i < NAR_MAX_VOLUME_COUNT; i++) {
                            
                            ExAcquireFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                            
                            if (RtlCompareMemory(PagedGUIDStrBuffer, NarData.VolumeRegionBuffer[i].GUIDStrVol.Buffer, NAR_GUID_STR_SIZE) == NAR_GUID_STR_SIZE) {
                                VolumeAlreadyExists = TRUE;
                            }
                            
                            ExReleaseFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                            if (VolumeAlreadyExists) {
                                break;
                            }
                            
                        }
                        
                        int VolumeAddIndex = -1;
                        // If volume doesnt exist, insert it to first empty space
                        if (VolumeAlreadyExists == FALSE) {
                            
                            for (int i = 0; i < NAR_MAX_VOLUME_COUNT; i++) {
                                
                                ExAcquireFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                                
                                if (NarData.VolumeRegionBuffer[i].GUIDStrVol.Length == 0) {
                                    // first empty space
                                    NarData.VolumeRegionBuffer[i].GUIDStrVol.Buffer = (PWCH)NarData.VolumeRegionBuffer[i].Reserved;
                                    
                                    memset(NarData.VolumeRegionBuffer[i].GUIDStrVol.Buffer, 0, NAR_GUID_STR_SIZE);
                                    memcpy(NarData.VolumeRegionBuffer[i].GUIDStrVol.Buffer, PagedGUIDStrBuffer, NAR_GUID_STR_SIZE);
                                    memset(NarData.VolumeRegionBuffer[i].MemoryBuffer, 0, NAR_MEMORYBUFFER_SIZE);
                                    
                                    NAR_INIT_MEMORYBUFFER(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                                    NarData.VolumeRegionBuffer[i].GUIDStrVol.MaximumLength = NAR_GUID_STR_SIZE;
                                    NarData.VolumeRegionBuffer[i].GUIDStrVol.Length = NAR_GUID_STR_SIZE/2;
                                    
                                    VolumeAdded = TRUE;
                                    VolumeAddIndex = i;
                                    NarData.VolumeRegionBuffer[i].Letter = Command->Letter;
                                    NarData.VolumeRegionBuffer[i].VolFileID = NAR_KERNEL_GEN_FILE_ID(Command->Letter);
                                    DbgPrint("Generated file ID %u", NarData.VolumeRegionBuffer[i].VolFileID);
                                    
                                    ExReleaseFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                                    break;
                                    
                                }
                                
                                ExReleaseFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                                
                            }
                            
                        }
                        
                        
                        if (PagedGUIDStrBuffer != NULL) {
                            DbgPrint("VOLUME GUID : %S\n", PagedGUIDStrBuffer);
                            ExFreePoolWithTag(PagedGUIDStrBuffer, NAR_TAG);
                        }
                        
                        if (!VolumeAdded) {
                            DbgPrint("Couldnt add volume to list\n");
                        }
                        else {
                            DbgPrint("Successfully added volume to list at index %i\n", VolumeAddIndex);
                        }
                        if (VolumeAlreadyExists) {
                            DbgPrint("Volume already exist in the list\n");
                        }
                        
                    }
                    else {
                        DbgPrint("Couldnt allocate paged memory for GUID string\n");
                    }
                    
                    
                    
                }
                else {
                    
                }
                
            }
            if (Command->Type == NarCommandType_DeleteVolume) {
                
                BOOLEAN FoundVolume = FALSE;
                for (int i = 0; i < NAR_MAX_VOLUME_COUNT; i++) {
                    
                    ExAcquireFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                    
                    if (RtlCompareMemory(Command->VolumeGUIDStr, NarData.VolumeRegionBuffer[i].Reserved, NAR_GUID_STR_SIZE) == NAR_GUID_STR_SIZE) {
                        
                        
                        // Reset file pointer to beginning of the file
                        nar_log_thread_params* tp = (nar_log_thread_params*)ExAllocatePoolWithTag(NonPagedPool, sizeof(nar_log_thread_params), NAR_TAG);
                        if (tp != NULL) {
                            memset(tp, 0, sizeof(*tp));
                            tp->Data    = NULL;
                            tp->DataLen = 0;
                            tp->FileID  = NarData.VolumeRegionBuffer[i].VolFileID;
                            tp->ShouldFlush = FALSE;
                            tp->ShouldQueryFileSize = FALSE;
                            tp->ShouldDelete = TRUE;
                            
                            NarWriteLogsToFile(tp, 0);
                            
                            NAR_INIT_MEMORYBUFFER(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                            
                            ExFreePoolWithTag(tp, NAR_TAG);
                        }
                        
                        // found the volume
                        FoundVolume = TRUE;
                        memset(NarData.VolumeRegionBuffer[i].MemoryBuffer, 0, NAR_MEMORYBUFFER_SIZE);
                        NAR_MB_USED(NarData.VolumeRegionBuffer[i].MemoryBuffer) = 2 * sizeof(INT32);
                        memset(NarData.VolumeRegionBuffer[i].Reserved, 0, NAR_GUID_STR_SIZE);
                        NarData.VolumeRegionBuffer[i].GUIDStrVol.Length = 0;
                        
                        // early termination
                        ExReleaseFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                        break;
                    }
                    
                    ExReleaseFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                    
                }
                
                if (!FoundVolume) {
                    DbgPrint("Couldnt find volume %S\n", Command->VolumeGUIDStr);
                }
                
            }
            
            
        }
        except(SpyExceptionFilter(GetExceptionInformation(), TRUE)) {
            DbgPrint("exception throwed\n");
            
            return STATUS_INVALID_PARAMETER;
        }
        
        
    }
    else {
        
        status = STATUS_INVALID_PARAMETER;
    }
    
    return status;
}




// Intented usage: strings, not optimized for big buffers, used only in kernel mode to detect some special directory activity
inline BOOLEAN
NarSubMemoryExists(const void* mem1, const void* mem2, unsigned int mem1len, unsigned int mem2len) {
    
    if (mem1 == 0 || mem2 == 0) return FALSE;
    if (mem2len > mem1len)      return FALSE;
    
    unsigned char* S1 = (unsigned char*)mem1;
    unsigned char* S2 = (unsigned char*)mem2;
    
    
    unsigned int k = 0;
    unsigned int j = 0;
    
    while (k < mem1len - mem2len && j < mem2len) {
        
        if (S1[k] == S2[j]) {
            
            int temp = k;
            
            while (k < mem1len && j < mem2len) {
                if (S1[k] == S2[j]) {
                    j++;
                    k++;
                }
                else {
                    break;
                }
            }
            
            if (j == mem2len) {
                return TRUE;
            }
            
            k = temp;
            
        }
        else {
            k++;
            j = 0;
        }
        
    }
    
    return FALSE;
}


/*
tp MUST BE ALLOCATED FROM NON-PAGED area, otherwise blue screen is almost inevitable
-- tp user initialized thread parameters to pass PsCreateSystemThread

-- if OutTObject is NULL, routine waits system thread's termination
   
-- if OutTObject is not NULL, routine returns referenced thread object, WITHOUT waiting associated thread. It's caller's responsibility to dereference that object
Caller might use returned object to wait for thread.


return : Returns lastly failed function's return value.
*/
inline NTSTATUS
NarWriteLogsToFile(nar_log_thread_params* tp, PETHREAD *OutTObject) {
    
    HANDLE Thread = 0;
    PETHREAD ThreadObject = 0;
    NTSTATUS status = PsCreateSystemThread(&Thread, THREAD_ALL_ACCESS, 0, 0, 0, NarLogThread, tp);
    if (NT_SUCCESS(status)) {
        
        status = ObReferenceObjectByHandle(Thread,
                                           THREAD_ALL_ACCESS,
                                           NULL,
                                           KernelMode,
                                           &ThreadObject,
                                           NULL);
        
        if (NT_SUCCESS(status)) {
            ZwClose(Thread);
        }
        else {
            DbgPrint("Obj reference failed with code %X", status);
        }
        
    }
    else {
        DbgPrint("failed to create thread, code %X\n", status);
    }
    
    if (NT_SUCCESS(status)) {
        if (OutTObject == NULL) {
            status = KeWaitForSingleObject(ThreadObject, Executive, KernelMode, FALSE, 0);
            if (NT_SUCCESS(status)) {
                ObDereferenceObject(ThreadObject);
            }
            else {
                DbgPrint("Failed to wait termination of the thread, code : %X\n", status);
            }
        }
        else {
            *OutTObject = ThreadObject;
        }
    }
    
    return status;
    
}




inline void
NarLogThread(PVOID param) {
    
    if(param ==0)return;
    
    NTSTATUS status = 0; // SUCCESS
    nar_log_thread_params* tp = (nar_log_thread_params*)param;
    
    if (tp->FileID >= NAR_KERNEL_MAX_FILE_ID || tp->FileID <= 0) {
        DbgPrint("Filed id is not in bounds, %u\n", tp->FileID);
        return;
    }
    
    IO_STATUS_BLOCK iosb = { 0 };
    
    if(tp->ShouldDelete){
        
        FILE_END_OF_FILE_INFORMATION eofinf = {0};
        status = ZwSetInformationFile(NarData.FileHandles[tp->FileID], &iosb, &eofinf, sizeof(eofinf), FileEndOfFileInformation);
        
        if(NT_SUCCESS(status)){
            
        }
        else{
            DbgPrint("File truncation failed with code %X", status);
        }
        
        goto END;
    } 
    
    if (tp->DataLen > 0 && tp->Data != NULL) {
        if (tp->DataLen < NAR_MEMORYBUFFER_SIZE) {
            status = ZwWriteFile(NarData.FileHandles[tp->FileID], 0, 0, 0, &iosb, (tp->Data), tp->DataLen, 0, 0);
            if (NT_SUCCESS(status)) {
                
            }
            else {
                DbgPrint("couldnt write to file, err id %X\n", status);
            }
        }
        else{
            
        }
    }
    else {
        if(tp->DataLen == 0) DbgPrint("datalen was zero");
        if (tp->Data == 0) DbgPrint("data was zero");
    }
    
    
    if (NT_SUCCESS(status)) {
        if (tp->ShouldFlush == TRUE) {
            status = ZwFlushBuffersFile(NarData.FileHandles[tp->FileID], &iosb);
            DbgPrint("Flush status code %X, iosb status %X", status, iosb.Status);
        }
        
        if (tp->ShouldQueryFileSize == TRUE) {
            FILE_STANDARD_INFORMATION standardInfo;
            status = NtQueryInformationFile(NarData.FileHandles[tp->FileID], &iosb, &standardInfo, sizeof(standardInfo), FileStandardInformation);
            if (NT_SUCCESS(status)) {
                tp->FileSize = standardInfo.EndOfFile.QuadPart;
            }
            else {
                DbgPrint("File size query failed, error code %X", status);
            }
        }
    }
    
    END:
    return;
}

const UNICODE_STRING IgnoreSuffixTable[] = {
    RTL_CONSTANT_STRING(L"$Mft"),
    RTL_CONSTANT_STRING(L"$LogFile"),
    RTL_CONSTANT_STRING(L"$BitMap"),
    RTL_CONSTANT_STRING(L".tmp"),
    RTL_CONSTANT_STRING(L"~"),
    RTL_CONSTANT_STRING(L".tib.metadata"),
    RTL_CONSTANT_STRING(L".tib"),
    RTL_CONSTANT_STRING(L".pf"),
    RTL_CONSTANT_STRING(L".cookie"),
    RTL_CONSTANT_STRING(L".nlfx")
};

const UNICODE_STRING IgnoreMidStringTable[] = {
    RTL_CONSTANT_STRING(L"$"),
    RTL_CONSTANT_STRING(L"\\AppData\\Local\\Temp"),
    RTL_CONSTANT_STRING(L"\\AppData\\Local\\Microsoft\\Windows\\Temporary Internet Files"),
    RTL_CONSTANT_STRING(L"\\AppData\\Google\\Chrome\\User Data\\Default\\Cache"),
    RTL_CONSTANT_STRING(L"\\AppData\\Local\\Microsoft\\EDGE\\User Data\\Default\\Cache"),
    RTL_CONSTANT_STRING(L"\\AppData\\Local\\Opera Software"),
    RTL_CONSTANT_STRING(L"\\AppData\\Local\\Mozilla\\Firefox\\Profiles"),
    RTL_CONSTANT_STRING(L"\\AppData\\Local\\Microsoft\\Windows\\INetCache\\IE")
};


//---------------------------------------------------------------------------
//              Operation filtering routines
//---------------------------------------------------------------------------


FLT_PREOP_CALLBACK_STATUS
#pragma warning(suppress: 6262) // higher than usual stack usage is considered safe in this case
SpyPreOperationCallback(
                        _Inout_ PFLT_CALLBACK_DATA Data,
                        _In_ PCFLT_RELATED_OBJECTS FltObjects,
                        _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
                        )
/*++

Routine Description:

  This routine receives ALL pre-operation callbacks for this filter.  It then
  tries to log information about the given operation.  If we are able
  to log information then we will call our post-operation callback  routine.

  NOTE:  This routine must be NON-PAGED because it can be called on the
      paging path.

Arguments:

  Data - Contains information about the given operation.

  FltObjects - Contains pointers to the various objects that are pertinent
    to this operation.

  CompletionContext - This receives the address of our log buffer for this
    operation.  Our completion routine then receives this buffer address.

Return Value:

  Identifies how processing should continue for this operation

--*/
{
    
    UNICODE_STRING defaultName;
    PUNICODE_STRING nameToUse;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(nameToUse);
    UNREFERENCED_PARAMETER(defaultName);
    UNREFERENCED_PARAMETER(nameToUse);
    UNREFERENCED_PARAMETER(CompletionContext);
    
    
    
    // If system shutdown requested, dont bother to log changes
    if (Data->Iopb->MajorFunction == IRP_MJ_SHUTDOWN) {
        
        for (int i = 0; i < NAR_MAX_VOLUME_COUNT; i++) {
            
            ExAcquireFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
            
            if (NarData.VolumeRegionBuffer[i].VolFileID != NAR_KERNEL_INVALID_FILE_ID) {
                
                if (NAR_MB_DATA_USED(NarData.VolumeRegionBuffer[i].MemoryBuffer) > 0) {
                    nar_log_thread_params* tp = (nar_log_thread_params*)ExAllocatePoolWithTag(NonPagedPool, sizeof(nar_log_thread_params), NAR_TAG);
                    
                    if (tp != NULL) {
                        
                        memset(tp, 0, sizeof(*tp));
                        tp->Data                = NAR_MB_DATA(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                        tp->DataLen             = NAR_MB_DATA_USED(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                        tp->FileID              = NarData.VolumeRegionBuffer[i].VolFileID;
                        tp->ShouldFlush         = TRUE;
                        tp->ShouldQueryFileSize = FALSE;
                        
                        status = NarWriteLogsToFile(tp, 0);
                        if (NT_SUCCESS(status)) {
                            
                        }
                        else {
                            
                        }
                        
                        NAR_INIT_MEMORYBUFFER(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                        ExFreePoolWithTag(tp, NAR_TAG);
                        
                    }
                    else {
                        DbgPrint("Unable to allocate memory to flush logs at preoperation routine\n");
                    }
                }
            }
            
            ExReleaseFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
            
        }
        
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }
    

    // filter out temporary files and files that would be closed if last handle freed
    if ((Data->Iopb->TargetFileObject->Flags & FO_TEMPORARY_FILE) == FO_TEMPORARY_FILE) {
        return  FLT_PREOP_SUCCESS_NO_CALLBACK;
    }
    
    if ((Data->Iopb->TargetFileObject->Flags & FO_DELETE_ON_CLOSE) == FO_DELETE_ON_CLOSE) {
        return  FLT_PREOP_SUCCESS_NO_CALLBACK;
    }
    
    if (Data->Iopb->MajorFunction != IRP_MJ_WRITE) {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }
    
    ULONG LenReturned = 0;
    // skip directories




    
    //FILE_STANDARD_INFORMATION fsi = { 0 };
    //status = FltQueryInformationFile(FltObjects->Instance, FltObjects->FileObject, &fsi, sizeof(FILE_STANDARD_INFORMATION), FileStandardInformation, &LenReturned);
    //if (NT_SUCCESS(status)) {
    //    if (fsi.Directory == TRUE) {
    //        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    //    }
    //}
    //else {
    //    DbgPrint("Failed to query information file\n");
    //}
    
    
    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    
#if 1
    
    unsigned char UnicodeStrBuffer[NAR_LOOKASIDE_SIZE];
    
    if (FltObjects->FileObject != NULL) {
        
        status = FltGetFileNameInformation(Data,
                                           FLT_FILE_NAME_NORMALIZED |
                                           NarData.NameQueryMethod,
                                           &nameInfo);
        if (NT_SUCCESS(status)) {
            
            for (size_t i = 0; 
                 i < sizeof(IgnoreSuffixTable) / sizeof(IgnoreSuffixTable[0]); 
                 i++) {
                if (RtlSuffixUnicodeString(&IgnoreSuffixTable[i], &nameInfo->Name, TRUE)) {
                    goto NAR_PREOP_FAILED_END;
                }
            }
            
            for (size_t i = 0; i < sizeof(IgnoreMidStringTable) / sizeof(IgnoreMidStringTable[0]); i++) {
                if (NarSubMemoryExists(nameInfo->Name.Buffer, IgnoreMidStringTable[i].Buffer, nameInfo->Name.Length, IgnoreMidStringTable[i].Length)) {
                    goto NAR_PREOP_FAILED_END;
                }
            }
            

            
            
            
#if 1
            ULONG BytesReturned = 0;
#pragma warning(push)
#pragma warning(disable:4090) //TODO what is this warning code
            
            STARTING_VCN_INPUT_BUFFER StartingInputVCNBuffer;
            RETRIEVAL_POINTERS_BUFFER ClusterMapBuffer;
            
            DWORD WholeFileMapBufferSize = sizeof(UnicodeStrBuffer);// sizeof(RETRIEVAL_POINTERS_BUFFER) * 128;
            RETRIEVAL_POINTERS_BUFFER* WholeFileMapBuffer = (RETRIEVAL_POINTERS_BUFFER*)&UnicodeStrBuffer[0]; // (RETRIEVAL_POINTERS_BUFFER*)ExAllocatePoolWithTag(PagedPool, WholeFileMapBufferSize, NAR_TAG);
            
            
            ClusterMapBuffer.StartingVcn.QuadPart = 0;
            StartingInputVCNBuffer.StartingVcn.QuadPart = 0;
            
            
            UINT32 RegionLen = 0;
            
            ULONG ClusterSize = 512 * 8; //TODO cluster size might not be 8*sector. check this
            
            LARGE_INTEGER WriteOffsetLargeInt = Data->Iopb->Parameters.Write.ByteOffset;
            ULONG WriteLen = Data->Iopb->Parameters.Write.Length;
            UINT32 NClustersToWrite = (WriteLen / (ClusterSize)+1);
            
            UINT32 ClusterWriteStartOffset = (UINT32)(WriteOffsetLargeInt.QuadPart / (LONGLONG)(ClusterSize));
            
            
#define NAR_PREOP_MAX_RECORD_COUNT 100
            
            struct {
                UINT32 S;
                UINT32 L;
            }P[NAR_PREOP_MAX_RECORD_COUNT];
            
            UINT32 RecCount = 0;
            UINT32 Error = 0;
            
            UINT32 MaxIteration = 0;
            
            BOOLEAN HeadFound = FALSE;
            BOOLEAN CeilTest = FALSE;
            
            status =    (FltObjects->Instance,
                                      Data->Iopb->TargetFileObject,
                                      FSCTL_GET_RETRIEVAL_POINTERS,
                                      &StartingInputVCNBuffer,
                                      sizeof(StartingInputVCNBuffer),
                                      WholeFileMapBuffer,
                                      WholeFileMapBufferSize,
                                      &BytesReturned
                                      );
            
            
            if (status != STATUS_END_OF_FILE && !NT_SUCCESS(status)) {
                DbgPrint("FltFsControl failed with code %i,  file name %wZ\n", status, &nameInfo->Name);
                
                
                goto NAR_PREOP_FAILED_END;
            }
            if (WholeFileMapBuffer->Extents[0].Lcn.QuadPart == -1) {
                goto NAR_PREOP_FAILED_END;
            }
            
            for (unsigned int i = 0; i < WholeFileMapBuffer->ExtentCount; i++) {
                
                ClusterMapBuffer.Extents[0] = WholeFileMapBuffer->Extents[i];
                
                /*
                the value (LONGLONG) -1 indicates either a compression unit that is partially allocated, or an unallocated region of a sparse file.
                */
                if (ClusterMapBuffer.Extents[0].Lcn.QuadPart == (LONGLONG)-1) { 
                    break; 
                }
                
                MaxIteration++;
                if (MaxIteration > 100) {
                    Error |= NAR_ERR_MAX_ITER;
                    break;
                }

                if (ClusterMapBuffer.Extents[0].NextVcn.QuadPart == (LONGLONG)-1) {
                    break;
                }
                if (ClusterMapBuffer.StartingVcn.QuadPart == (LONGLONG)-1) {
                    break;
                }
                if (ClusterMapBuffer.Extents[0].NextVcn.QuadPart - ClusterMapBuffer.StartingVcn.QuadPart >= MAXUINT32) {
                    break;
                }

                
                
                if (!HeadFound) {
                    CeilTest = ((LONGLONG)ClusterWriteStartOffset < ClusterMapBuffer.Extents[0].NextVcn.QuadPart);
                }
                else {
                    CeilTest = TRUE;
                }
                
                if ((LONGLONG)ClusterWriteStartOffset >= StartingInputVCNBuffer.StartingVcn.QuadPart && CeilTest) {
                    
                    RegionLen = (UINT32)(ClusterMapBuffer.Extents[0].NextVcn.QuadPart - ClusterMapBuffer.StartingVcn.QuadPart);
                    
                    if (!HeadFound) {
                        
                        //Starting position of the write operation
                        UINT32 OffsetFromRegionStart = (ClusterWriteStartOffset - (UINT32)ClusterMapBuffer.StartingVcn.QuadPart);
                        
                        
                        if (ClusterMapBuffer.Extents->NextVcn.QuadPart - OffsetFromRegionStart < NClustersToWrite) {
                            //Region overflow
                            P[RecCount].S = ((UINT32)ClusterMapBuffer.Extents[0].Lcn.QuadPart + OffsetFromRegionStart);//start
                            P[RecCount].L = RegionLen - OffsetFromRegionStart;
                            RecCount++;
                            NClustersToWrite -= (RegionLen - OffsetFromRegionStart);
                        }
                        else {
                            //Operation fits the region
                            P[RecCount].S = (UINT32)ClusterMapBuffer.Extents[0].Lcn.QuadPart + OffsetFromRegionStart;
                            P[RecCount].L = NClustersToWrite;
                            RecCount++;
                            NClustersToWrite = 0;
                        }
                        
                        ClusterWriteStartOffset = (UINT32)ClusterMapBuffer.Extents[0].NextVcn.QuadPart;
                        
                        HeadFound = TRUE;
                        
                    }
                    else { // HeadFound
                        //Write operation falls over other region(s)
                        if ((NClustersToWrite - RegionLen) > 0) {
                            //write operation does not fit this region
                            P[RecCount].S = (UINT32)ClusterMapBuffer.Extents[0].Lcn.QuadPart;
                            P[RecCount].L = RegionLen;
                            RecCount++;
                            NClustersToWrite -= RegionLen;
                        }
                        else if (RecCount < NAR_PREOP_MAX_RECORD_COUNT) {
                            /*
                            Write operation fits that region
                            */
                            P[RecCount].S = (UINT32)ClusterMapBuffer.Extents[0].Lcn.QuadPart;
                            P[RecCount].L = NClustersToWrite;
                            RecCount++;
                            NClustersToWrite = 0;
                            break;
                        }
                        else {
                            DbgPrint("Cant fill any more regions\n");
                            break;
                        }
                        
                        ClusterWriteStartOffset = (UINT32)ClusterMapBuffer.Extents[0].NextVcn.QuadPart;
                    }
                    
                }
                
                if (NClustersToWrite <= 0) {
                    break;
                }
                if (RecCount == NAR_PREOP_MAX_RECORD_COUNT) {
                    DbgPrint("Record count = %i, breaking now\n", NAR_PREOP_MAX_RECORD_COUNT);
                    break;
                }
                
                
            }
            
            
            
            // TODO(BATUHAN): try one loop via KeTestSpinLock, to fast check if volume is available for fast access, if it is available and matches GUID, immidiately flush all regions and return, if not available test higher elements in list.
            char CompareBuffer[NAR_GUID_STR_SIZE];
            
            ULONG SizeNeededForGUIDStr = 0;
            
            UNICODE_STRING GUIDStringNPaged;
            BOOLEAN Added = FALSE;
            if (Error) {
                if ((Error & 0x1) != 0x1) DbgPrint("Error flag(s) detected, %X, %wZ\n", Error, &nameInfo->Name);
            }
            else {
                if (RecCount == 0) {
                    DbgPrint("RECCOUNT WAS ZERO %wZ\n", &nameInfo->Name);
                }
            }
            
            if (RecCount > 0) {
                
                INT32 SizeNeededForMemoryBuffer = 2 * RecCount * sizeof(UINT32);
                INT32 RemainingSizeOnBuffer = 0;
                RtlInitEmptyUnicodeString(&GUIDStringNPaged, &CompareBuffer[0], NAR_GUID_STR_SIZE);
                status = FltGetVolumeGuidName(FltObjects->Volume, &GUIDStringNPaged, &SizeNeededForGUIDStr);
                
                if (NT_SUCCESS(status)) {
                    
                    for (int i = 0; i < NAR_MAX_VOLUME_COUNT; i++) {
                        
                        ExAcquireFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                        
                        if (NarData.VolumeRegionBuffer[i].MemoryBuffer == 0) {
                            ExReleaseFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                            continue;
                        }
                        
                        if (RtlCompareMemory(&CompareBuffer[0], NarData.VolumeRegionBuffer[i].GUIDStrVol.Buffer, NAR_GUID_STR_SIZE) == NAR_GUID_STR_SIZE) {
                            RemainingSizeOnBuffer = NAR_MEMORYBUFFER_SIZE - NAR_MB_USED(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                            
                            if (RemainingSizeOnBuffer >= SizeNeededForMemoryBuffer) {
                                NAR_MB_PUSH(NarData.VolumeRegionBuffer[i].MemoryBuffer, &P[0], SizeNeededForMemoryBuffer);
                                Added = TRUE;
                            }
                            else {
                                
                                nar_log_thread_params* tp = (nar_log_thread_params*)ExAllocatePoolWithTag(NonPagedPool, sizeof(nar_log_thread_params), NAR_TAG);
                                if (tp != NULL) {
                                    memset(tp, 0, sizeof(*tp));
                                    tp->Data = NAR_MB_DATA(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                                    tp->DataLen = NAR_MB_DATA_USED(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                                    tp->FileID = NarData.VolumeRegionBuffer[i].VolFileID;
                                    tp->ShouldFlush = FALSE;
                                    tp->ShouldQueryFileSize = FALSE;
                                    
                                    NarWriteLogsToFile(tp, 0);
                                    NAR_INIT_MEMORYBUFFER(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                                    
                                    ExFreePoolWithTag(tp, NAR_TAG);
                                }
                                else {
                                    DbgPrint("Couldnt allocate memory to flush logs\n");
                                    // couldnt allocate memory, report error to user
                                }
                                
                            }
                            
                            ExReleaseFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                            break;
                        }
                        
                        ExReleaseFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                        
                    }
                    
                }
                else {
                    DbgPrint("Couldnt get volume GUID, status : %i, sizeneeded : %i", status, SizeNeededForGUIDStr);
                }               
                
            }
            else {
                //DbgPrint("Temporary if failed 1293123\n");
            }
            
#pragma warning(pop)
#endif
            
            
        }
        else {
            goto NAR_PREOP_FAILED_END;
        }
        
        
    }
    
#endif
    
    
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
    
    NAR_PREOP_FAILED_END:
    
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
    
}


FLT_POSTOP_CALLBACK_STATUS
SpyPostOperationCallback(
                         _Inout_ PFLT_CALLBACK_DATA Data,
                         _In_ PCFLT_RELATED_OBJECTS FltObjects,
                         _In_ PVOID CompletionContext,
                         _In_ FLT_POST_OPERATION_FLAGS Flags
                         )
/*++

Routine Description:

  This routine receives ALL post-operation callbacks.  This will take
  the log record passed in the context parameter and update it with
  the completion information.  It will then insert it on a list to be
  sent to the usermode component.
  ,
  NOTE:  This routine must be NON-PAGED because it can be called at DPC level

Arguments:

  Data - Contains information about the given operation.

  FltObjects - Contains pointers to the various objects that are pertinent
    to this operation.

  CompletionContext - Pointer to the RECORD_LIST structure in which we
    store the information we are logging.  This was passed from the
    pre-operation callback

  Flags - Contains information as to why this routine was called.

Return Value:

  Identifies how processing should continue for this operation

--*/
{
    
    
    
    
    //PFLT_TAG_DATA_BUFFER tagData;
    
    ULONG copyLength;
    
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(copyLength);
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);
    
    NTSTATUS status;
    UNREFERENCED_PARAMETER(status);
    
    
    //
    //  If our instance is in the process of being torn down don't bother to
    //  log this record, free it now.
    //
    
    
    return FLT_POSTOP_FINISHED_PROCESSING;
    
    
    
}

NTSTATUS
SpyVolumeInstanceSetup(
                       _In_ PCFLT_RELATED_OBJECTS FltObjects,
                       _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
                       _In_ DEVICE_TYPE VolumeDeviceType,
                       _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType) {
    
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    UNREFERENCED_PARAMETER(VolumeDeviceType);
    UNREFERENCED_PARAMETER(VolumeFilesystemType);
    
    PAGED_CODE();
    
    if ((Flags & FLTFL_INSTANCE_SETUP_MANUAL_ATTACHMENT)
        && VolumeFilesystemType == FLT_FSTYPE_NTFS
        && VolumeDeviceType == FILE_DEVICE_DISK_FILE_SYSTEM) {
        
        return STATUS_SUCCESS;
        
    }
    else {
        return STATUS_FLT_DO_NOT_ATTACH;
    }
    
}


LONG
SpyExceptionFilter(
                   _In_ PEXCEPTION_POINTERS ExceptionPointer,
                   _In_ BOOLEAN AccessingUserBuffer
                   )
/*++

Routine Description:

  Exception filter to catch errors touching user buffers.

Arguments:

  ExceptionPointer - The exception record.

  AccessingUserBuffer - If TRUE, overrides FsRtlIsNtStatusExpected to allow
             the caller to munge the error to a desired status.

Return Value:

  EXCEPTION_EXECUTE_HANDLER - If the exception handler should be run.

  EXCEPTION_CONTINUE_SEARCH - If a higher exception handler should take care of
                this exception.

--*/
{
    NTSTATUS Status;
    
    Status = ExceptionPointer->ExceptionRecord->ExceptionCode;
    
    //
    //  Certain exceptions shouldn't be dismissed within the namechanger filter
    //  unless we're touching user memory.
    //
    
    if (!FsRtlIsNtstatusExpected(Status) &&
        !AccessingUserBuffer) {
        
        return EXCEPTION_CONTINUE_SEARCH;
    }
    
    return EXCEPTION_EXECUTE_HANDLER;
}


