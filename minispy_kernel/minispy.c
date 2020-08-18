/*++

Copyright (c) 1989-2002  Microsoft Corporation

Module Name:

  MiniSpy.c

Abstract:

  This is the main module for the MiniSpy mini-filter.

Environment:

  Kernel mode

--*/

#include "mspyKern.h"
#include <stdio.h>
#include <Ntstrsafe.h>
#include <string.h>

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
        
        void *UnicodeStringBuffer = ExAllocatePoolWithTag(PagedPool, 512, NAR_TAG);
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
                    }
                    else {
                        FileSize = FI.EndOfFile.QuadPart;
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
        
        
        ExInitializePagedLookasideList(&NarData.LookAsideList, 0, 0, 0, NAR_LOOKASIDE_SIZE, NAR_TAG, 0);
        ExInitializePagedLookasideList(&NarData.GUIDComparePagedLookAsideList, 0, 0, 0, NAR_GUID_STR_SIZE, NAR_TAG, 0);
        
        NarData.VolumeRegionBuffer = ExAllocatePoolWithTag(PagedPool, NAR_VOLUMEREGIONBUFFERSIZE, NAR_TAG);
        if (NarData.VolumeRegionBuffer == NULL) {
            DbgPrint("Couldnt allocate %I64d paged memory for volume buffer\n", NAR_VOLUMEREGIONBUFFERSIZE);
            status = STATUS_FAILED_DRIVER_ENTRY;
            leave;
        }
        memset(NarData.VolumeRegionBuffer, 0, NAR_VOLUMEREGIONBUFFERSIZE);
        
        //Initializing each volume entry
        for (int i = 0; i < NAR_MAX_VOLUME_COUNT; i++) {
            
            
            ExInitializeFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
            NarData.VolumeRegionBuffer[i].MemoryBuffer = 0;
            NarData.VolumeRegionBuffer[i].MemoryBuffer = ExAllocatePoolWithTag(PagedPool, NAR_MEMORYBUFFER_SIZE, NAR_TAG);
            
            NarData.VolumeRegionBuffer[i].GUIDStrVol.Length = 0;
            NarData.VolumeRegionBuffer[i].GUIDStrVol.MaximumLength = sizeof(NarData.VolumeRegionBuffer[i].Reserved);
            NarData.VolumeRegionBuffer[i].GUIDStrVol.Buffer = (PWCH)NarData.VolumeRegionBuffer[i].Reserved;
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
        
        for (int i = 0; i < TrackedVolumesCount; i++) {
            
            memset(UniBuffer, 0, 32);
            UNICODE_STRING VolumeNameUniStr;
            RtlInitEmptyUnicodeString(&VolumeNameUniStr, UniBuffer, 32);
            
            status = RtlUnicodeStringPrintf(&VolumeNameUniStr, L"%c:", TrackedVolumes[i].Letter);
            if (NT_SUCCESS(status)) {
                PFLT_VOLUME Volume;
                
                // For every succ call, we must deference PFLT_VOLUME
                status = FltGetVolumeFromName(NarData.Filter, &VolumeNameUniStr, &Volume);
                if (NT_SUCCESS(status)) {
                    
                    ULONG BufferNeeded = 0;
                    status = FltGetVolumeGuidName(Volume, &NarData.VolumeRegionBuffer[i].GUIDStrVol, &BufferNeeded);
                    
                    if (NT_SUCCESS(status)) {
                        
                        status = FltAttachVolume(NarData.Filter, Volume, NULL, NULL);
                        if (NT_SUCCESS(status)) {
                            DbgPrint("Successfully attached volume %wZ\n", &NarData.VolumeRegionBuffer[i].GUIDStrVol);
                        }
                        else {
                            DbgPrint("Couldnt attach volume volume %wZ, status %i\n", &NarData.VolumeRegionBuffer[i].GUIDStrVol, status);
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
                
                ExFreePoolWithTag(NarData.VolumeRegionBuffer, NAR_TAG);
                DbgPrint("Freed volume region buffer\n");
                
            }// If nardata.volumeregionbuffer != NULL
            
            ExDeletePagedLookasideList(&NarData.LookAsideList);
            DbgPrint("Freed paged lookaside list\n");
            
            
            
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
    
    //
    //  Close the server port. This will stop new connections.
    //
    
    
    DbgPrint("Filter unloading\n");
    
    FltCloseCommunicationPort(NarData.ServerPort);
    DbgPrint("Communication port closed\n");
    
    
    if (NarData.VolumeRegionBuffer != NULL) {
        
        for (int i = 0; i < NAR_MAX_VOLUME_COUNT; i++) {
            if (NarData.VolumeRegionBuffer[i].MemoryBuffer != NULL) {
                DbgPrint("Freeing volume memory buffer %i\n", i);
                ExFreePoolWithTag(NarData.VolumeRegionBuffer[i].MemoryBuffer, NAR_TAG);
                NarData.VolumeRegionBuffer[i].MemoryBuffer = 0;
            }
            else {
                DbgPrint("Volume entry %i was null\n", i);
                break;
            }
        }
        
        ExFreePoolWithTag(NarData.VolumeRegionBuffer, NAR_TAG);
        DbgPrint("Freed volume region buffer\n");
        
    }// If nardata.volumeregionbuffer != NULL
    
    
    ExDeletePagedLookasideList(&NarData.LookAsideList);
    DbgPrint("Lookasidelist deleted\n");
    
    ExDeletePagedLookasideList(&NarData.GUIDComparePagedLookAsideList);
    DbgPrint("GUID compare paged lookaside list deleted\n");
    
    FltUnregisterFilter(NarData.Filter);
    DbgPrint("Filter unregistered\n");
    
    
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
            if (Command->Type == NarCommandType_GetVolumeLog) {
                
                INT32 FoundVolume = FALSE;
                
                if (Command->VolumeGUIDStr != NULL
                    && OutputBuffer != NULL
                    && OutputBufferSize == NAR_MEMORYBUFFER_SIZE) {
                    
                    //memset(OutputBuffer, 0, OutputBufferSize);
                    
                    
                    for (int i = 0; i < NAR_MAX_VOLUME_COUNT; i++) {
                        
                        
                        ExAcquireFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                        
                        if (RtlCompareMemory(Command->VolumeGUIDStr, NarData.VolumeRegionBuffer[i].Reserved, NAR_GUID_STR_SIZE) == NAR_GUID_STR_SIZE) {
                            
                            memcpy(OutputBuffer, NarData.VolumeRegionBuffer[i].MemoryBuffer, NAR_MB_USED(NarData.VolumeRegionBuffer[i].MemoryBuffer));
                            
                            memset(NarData.VolumeRegionBuffer[i].MemoryBuffer, 0, NAR_MEMORYBUFFER_SIZE);
                            NAR_INIT_MEMORYBUFFER(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                            
                            // reset memory buffer.
                            FoundVolume = TRUE;
                            
                            // early termination
                            ExReleaseFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                            break;
                            
                        }
                        
                        ExReleaseFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                        
                    }
                    
                    
                    *ReturnOutputBufferLength = 0;
                    if (FoundVolume == FALSE) {
                        DbgPrint("Volume not found! %wZ\n", Command->VolumeGUIDStr);
                    }
                    else {
                        *ReturnOutputBufferLength = (ULONG)NAR_MEMORYBUFFER_SIZE;
                        DbgPrint("Succ copied data to output buffer\n");
                    }
                    
                    
                    
                }
                else {
                    DbgPrint("BIG IF BLOCK FAILED IN SPYMESSAGE\n"); // short but effective temporary message
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
                                    
                                    VolumeAdded = TRUE;
                                    
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
                            DbgPrint("Successfully added volume to list\n");
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
    PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
    UNICODE_STRING defaultName;
    PUNICODE_STRING nameToUse;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(nameToUse);
    UNREFERENCED_PARAMETER(defaultName);
    UNREFERENCED_PARAMETER(nameToUse);
    UNREFERENCED_PARAMETER(CompletionContext);
    
    
    ULONG PID = FltGetRequestorProcessId(Data);
    // filter out temporary files and files that would be closed if last handle freed
    if ((Data->Iopb->TargetFileObject->Flags & FO_TEMPORARY_FILE) == FO_TEMPORARY_FILE || (Data->Iopb->TargetFileObject->Flags & FO_DELETE_ON_CLOSE) == FO_DELETE_ON_CLOSE || PID == NarData.UserModePID) {
        return  FLT_PREOP_SUCCESS_NO_CALLBACK;
    }
    
    
    
#if 1
    
    
    if (FltObjects->FileObject != NULL) {
        
        status = FltGetFileNameInformation(Data,
                                           FLT_FILE_NAME_NORMALIZED |
                                           NarData.NameQueryMethod,
                                           &nameInfo);
        if (NT_SUCCESS(status)) {
            
            // Harddiskdevice\path\asdfs\sdffsd\file
            /*
               C:\Users\adm\AppData\Local\Temp
               C:\Users\adm\AppData\Local\Microsoft\Windows\Temporary Internet Files
               C:\Users\adm\AppData\Local\Google\Chrome\User Data\Default\Cache
               C:\Users\adm\AppData\Local\Opera Software
               C:\Users\adm\AppData\Local\Mozilla\Firefox\Profiles
               C:\Windows\CSC
            */
            
            //FltGetVolumeName(FltObjects->Volume, StringBuffer, 0);
            
            UNICODE_STRING UniStr;
            void* UnicodeStrBuffer = ExAllocateFromPagedLookasideList(&NarData.LookAsideList);
            if (UnicodeStrBuffer == NULL) {
                DbgPrint("Couldnt allocate memory for unicode string\n");
                return FLT_PREOP_SUCCESS_NO_CALLBACK;
            }
            
            RtlInitEmptyUnicodeString(&UniStr, UnicodeStrBuffer, NAR_LOOKASIDE_SIZE);
            
            //
#if 0
                
                RtlUnicodeStringPrintf(&UniStr, L"\\Device\\HarddiskVolume%i\\Users\\%S\\AppData\\Local\\Temp", NarData.OsDeviceID, NarData.UserName);
                if (RtlPrefixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
                    ExFreeToPagedLookasideList(&NarData.LookasideList, UnicodeStrBuffer);
                    goto NAR_PREOP_END;
                }
                
                RtlUnicodeStringPrintf(&UniStr, L"\\Device\\HarddiskVolume%i\\Users\\%S\\AppData\\Local\\Microsoft\\Windows\\Temporary Internet Files", NarData.OsDeviceID, NarData.UserName);
                if (RtlPrefixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
                    ExFreeToPagedLookasideList(&NarData.LookasideList, UnicodeStrBuffer);
                    goto NAR_PREOP_END;
                }
                
                RtlUnicodeStringPrintf(&UniStr, L"\\Device\\HarddiskVolume%i\\Users\\%S\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\Cache", NarData.OsDeviceID, NarData.UserName);
                if (RtlPrefixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
                    ExFreeToPagedLookasideList(&NarData.LookasideList, UnicodeStrBuffer);
                    goto NAR_PREOP_END;
                }
                
                RtlUnicodeStringPrintf(&UniStr, L"\\Device\\HarddiskVolume%i\\Users\\%S\\AppData\\Local\\Opera Software", NarData.OsDeviceID, NarData.UserName);
                if (RtlPrefixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
                    ExFreeToPagedLookasideList(&NarData.LookasideList, UnicodeStrBuffer);
                    goto NAR_PREOP_END;
                }
                
                RtlUnicodeStringPrintf(&UniStr, L"\\Device\\HarddiskVolume%i\\Users\\%S\\AppData\\Local\\Mozilla\\Firefox\\Profiles", NarData.OsDeviceID, NarData.UserName);
                if (RtlPrefixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
                    ExFreeToPagedLookasideList(&NarData.LookasideList, UnicodeStrBuffer);
                    goto NAR_PREOP_END;
                }
                
            
#endif
            
            
            //Suffix area
            RtlUnicodeStringPrintf(&UniStr, L".tmp");
            if (RtlSuffixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
                ExFreeToPagedLookasideList(&NarData.LookAsideList, UnicodeStrBuffer);
                goto NAR_PREOP_END;
            }
            RtlUnicodeStringPrintf(&UniStr, L"~");
            if (RtlSuffixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
                ExFreeToPagedLookasideList(&NarData.LookAsideList, UnicodeStrBuffer);
                goto NAR_PREOP_END;
            }
            RtlUnicodeStringPrintf(&UniStr, L".tib.metadata");
            if (RtlSuffixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
                ExFreeToPagedLookasideList(&NarData.LookAsideList, UnicodeStrBuffer);
                goto NAR_PREOP_END;
            }
            RtlUnicodeStringPrintf(&UniStr, L".tib");
            if (RtlSuffixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
                ExFreeToPagedLookasideList(&NarData.LookAsideList, UnicodeStrBuffer);
                goto NAR_PREOP_END;
            }
            RtlUnicodeStringPrintf(&UniStr, L".pf");
            if (RtlSuffixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
                ExFreeToPagedLookasideList(&NarData.LookAsideList, UnicodeStrBuffer);
                goto NAR_PREOP_END;
            }

            
            //NAR
            
            
#if 1
            ULONG BytesReturned = 0;
#pragma warning(push)
#pragma warning(disable:4090) //TODO what is this warning code
            
            STARTING_VCN_INPUT_BUFFER StartingInputVCNBuffer;
            RETRIEVAL_POINTERS_BUFFER ClusterMapBuffer;
            ClusterMapBuffer.StartingVcn.QuadPart = 0;
            StartingInputVCNBuffer.StartingVcn.QuadPart = 0;
            
            UINT32 RegionLen = 0;
            
            ULONG ClusterSize = 512 * 8; //TODO cluster size might not be 8*sector. check this
            
            LARGE_INTEGER WriteOffsetLargeInt = Data->Iopb->Parameters.Write.ByteOffset;
            ULONG WriteLen = Data->Iopb->Parameters.Write.Length;
            UINT32 NClustersToWrite = (WriteLen / (ClusterSize)+1);
            
            UINT32 ClusterWriteStartOffset = (UINT32)(WriteOffsetLargeInt.QuadPart / (LONGLONG)(ClusterSize));
            
            
            struct {
                UINT32 S;
                UINT32 L;
            }P[10];
            
            UINT32 RecCount = 0;
            UINT32 Error = 0;
            
            UINT32 MaxIteration = 0;
            
            BOOLEAN HeadFound = FALSE;
            BOOLEAN CeilTest = FALSE;
            
            for (;;) {

                MaxIteration++;
                if (MaxIteration > 1024) {
                    Error |= NAR_ERR_MAX_ITER;
                    break;
                }
                
                status = FltFsControlFile(FltObjects->Instance,
                                          Data->Iopb->TargetFileObject,
                                          FSCTL_GET_RETRIEVAL_POINTERS,
                                          &StartingInputVCNBuffer,
                                          sizeof(StartingInputVCNBuffer),
                                          &ClusterMapBuffer,
                                          sizeof(ClusterMapBuffer),
                                          &BytesReturned
                                          );
                
                if (status != STATUS_BUFFER_OVERFLOW
                    && status != STATUS_SUCCESS
                    && status != STATUS_END_OF_FILE) {
                    Error |= NAR_ERR_TRINITY;
                    break;
                }
                
                /*
                It is unlikely, but we should check overflows
                */
                
                if (ClusterMapBuffer.Extents[0].Lcn.QuadPart > MAXUINT32 || ClusterMapBuffer.Extents[0].NextVcn.QuadPart > MAXUINT32 || ClusterMapBuffer.StartingVcn.QuadPart > MAXUINT32) {
                    DbgPrint("OVERFLOW OCCURED lcn :%X, nextvcn : %X, starting vcn : %X, index %i, name %wZ\n",ClusterMapBuffer.Extents[0].Lcn.QuadPart, ClusterMapBuffer.Extents[0].NextVcn.QuadPart, ClusterMapBuffer.Extents[0].NextVcn.QuadPart, RecCount, &nameInfo->Name);
                    Error |= NAR_ERR_REG_OVERFLOW;
                    break;
                }

                if (ClusterMapBuffer.Extents[0].NextVcn.QuadPart - ClusterMapBuffer.StartingVcn.QuadPart < (UINT64)0) {
                    DbgPrint("Region was lower than 0, reg len :%X, extents %X, StartingVcn : %X, name : %wZ\n", ClusterMapBuffer.Extents[0].NextVcn.QuadPart - ClusterMapBuffer.StartingVcn.QuadPart, ClusterMapBuffer.Extents[0].NextVcn.QuadPart, ClusterMapBuffer.StartingVcn.QuadPart, &nameInfo->Name);
                    
                    for (int i = 0; i < RecCount; i++) {
                        DbgPrint("Start: %X\tLen: %X\n", P[i].S, P[i].L);
                    }

                    Error |= NAR_ERR_REG_BELOW_ZERO;
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
                        else if (RecCount < 10) {
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
                if (RecCount == 10) {
                    DbgPrint("Record count = 10, breaking now\n");
                    break;
                }
                
                StartingInputVCNBuffer.StartingVcn = ClusterMapBuffer.Extents->NextVcn;
                
                if (status == STATUS_END_OF_FILE) {
                    break;
                }
                
            }
            


            // TODO(BATUHAN): try one loop via KeTestSpinLock, to fast check if volume is available for fast access, if it is available and matches GUID, immidiately flush all regions and return, if not available test higher elements in list.
            void* CompareBuffer = ExAllocateFromPagedLookasideList(&NarData.GUIDComparePagedLookAsideList);
            ULONG SizeNeededForGUIDStr = 0;
            
            UNICODE_STRING GUIDStringNPaged;
            BOOLEAN Added = FALSE;
            if(Error){
                DbgPrint("Error flag(s) detected, %X\n", Error);
            }
            
            if (RecCount > 0 && UniStr.MaximumLength >= NAR_GUID_STR_SIZE && Error == 0) {
                INT32 SizeNeededForMemoryBuffer = 2 * RecCount * sizeof(UINT32);
                INT32 RemainingSizeOnBuffer = 0;
                RtlInitEmptyUnicodeString(&GUIDStringNPaged, CompareBuffer, NAR_GUID_STR_SIZE);
                status = FltGetVolumeGuidName(FltObjects->Volume, &GUIDStringNPaged, &SizeNeededForGUIDStr);
                if (NT_SUCCESS(status)) {
                    
                    
                    
                    for (int i = 0; i < NAR_MAX_VOLUME_COUNT; i++) {
                        
                        ExAcquireFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                        
                        if (RtlCompareMemory(CompareBuffer, NarData.VolumeRegionBuffer[i].GUIDStrVol.Buffer, NAR_GUID_STR_SIZE) == NAR_GUID_STR_SIZE) {
                            RemainingSizeOnBuffer = NAR_MEMORYBUFFER_SIZE - NAR_MB_USED(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                            
                            if (RemainingSizeOnBuffer >= SizeNeededForMemoryBuffer) {
                                NAR_MB_PUSH(NarData.VolumeRegionBuffer[i].MemoryBuffer, &P[0], SizeNeededForMemoryBuffer);
                                Added = TRUE;
                            }
                            else {
                                NAR_MB_MARK_NOT_ENOUGH_SPACE(NarData.VolumeRegionBuffer[i].MemoryBuffer);
                            }
                            
                            ExReleaseFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                            break;
                        }
                        
                        ExReleaseFastMutex(&NarData.VolumeRegionBuffer[i].FastMutex);
                        
                    }
                    
                    if (!Added) {
                        DbgPrint("Couldnt add entry to memory buffer, %wZ\n", &GUIDStringNPaged);
                    }
                    
                    
                }
                else {
                    DbgPrint("Couldnt get volume GUID, status : %i, sizeneeded : %i", status, SizeNeededForGUIDStr);
                }
                
                
                
            }
            
            
            
#pragma warning(pop)
#endif
            
            ExFreeToPagedLookasideList(&NarData.LookAsideList, UnicodeStrBuffer);
            ExFreeToPagedLookasideList(&NarData.GUIDComparePagedLookAsideList, CompareBuffer);
            
        }
        else {
            goto NAR_PREOP_END;
        }
        
        
    }
    
    
    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
#endif
    
    //#define STRBUFFERSIZE 64
    //char StrBuffer[STRBUFFERSIZE];
    //size_t SizeNeeded = 0;
    //RtlStringCbPrintfA(StrBuffer, STRBUFFERSIZE, "GUIDLEN : %i\n", 24);
    //RtlStringCbLengthA(StrBuffer, STRBUFFERSIZE, &SizeNeeded);
    //#undef STRBUFFERSIZE
    
    //KIRQL IRQL;
    //KeAcquireSpinLock(&NarData.Spinlock, &IRQL);
    //LARGE_INTEGER EntryTime;
    //KeQuerySystemTimePrecise(&EntryTime);
    //LARGE_INTEGER CurrentTime;
    
    //while (!ExTryToAcquireFastMutex(&NarData.FastMutex)) {
    //  KeQuerySystemTimePrecise(&CurrentTime);
    //  // 100 nenoseconds per unit, 100ns*10=1us, 1us*1000= 1ms
    //  if ((CurrentTime.QuadPart - EntryTime.QuadPart) > 2LL * 10LL * 1000LL) {
    //    DbgPrint("Waited too much(2ms) returning now, %wZ\n", &nameInfo->Name);
    //    return FLT_PREOP_SUCCESS_NO_CALLBACK;
    //  }
    //}
    //
    //// Locked
    //if (NarData.MemoryBufferUsed + SizeNeeded <= NAR_LOOKASIDE_SIZE) {
    //  DbgPrint("Adding entry to buffer with size %i, new buffer size  %i\n", SizeNeeded, NarData.MemoryBufferUsed + SizeNeeded);
    //  memcpy((char*)NarData.MemoryBuffer + NarData.MemoryBufferUsed, StrBuffer, SizeNeeded);
    //  NarData.MemoryBufferUsed += (UINT32)SizeNeeded;
    //  ExReleaseFastMutex(&NarData.FastMutex);
    //  return FLT_PREOP_SUCCESS_NO_CALLBACK;
    //}
    
    
#if 0
    void* TempBuffer = ExAllocateFromPagedLookasideList(&NarData.LookAsideList);
    UINT32 TempBufferSize = NarData.MemoryBufferUsed;
    memcpy(TempBuffer, NarData.MemoryBuffer, NarData.MemoryBufferUsed);
    memset(NarData.MemoryBuffer, 0, NarData.MemoryBufferUsed);
    
    NarData.MemoryBufferUsed = 0;
    ExReleaseFastMutex(&NarData.FastMutex);
    
    DbgPrint("Flushing all data cached so far %i\n", TempBufferSize);
    {
        //DbgPrint("Exceeded buffer, size %i\n", NarData.MemoryBufferUsed);
        
        
        
        UNICODE_STRING     uniName;
        OBJECT_ATTRIBUTES  objAttr;
        HANDLE FileHandle;
        
        RtlInitUnicodeString(&uniName, L"\\DosDevices\\D:\\Example.txt");  // or L"\\SystemRoot\\example.txt"
        InitializeObjectAttributes(&objAttr, &uniName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
        
        
        IO_STATUS_BLOCK IOSB = { 0 };
        
        status = ZwCreateFile(
                              &FileHandle,
                              FILE_APPEND_DATA | SYNCHRONIZE,
                              &objAttr, &IOSB, NULL,
                              FILE_ATTRIBUTE_NORMAL,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              FILE_OPEN_IF,
                              FILE_SEQUENTIAL_ONLY | FILE_WRITE_THROUGH | FILE_SYNCHRONOUS_IO_NONALERT,
                              NULL, 0);
        
        if (NT_SUCCESS(status)) {
            DbgPrint("Succ opened file\n");
            
            status = ZwWriteFile(FileHandle, 0, 0, 0, &IOSB, TempBuffer, TempBufferSize, 0, 0);
            if (NT_SUCCESS(status)) {
                DbgPrint("Successfully written to file\n");
            }
            else {
                DbgPrint("couldnt write to file, err id %i\n", status);
            }
            
            ZwClose(FileHandle);
            
        }
        else {
            DbgPrint("Couldnt open file\n");
        }
        //Flush to file
        
    }
    
    
    //if (NarData.MemoryBuffer != NULL) {
    //  memcpy((char*)NarData.MemoryBuffer + NarData.MemoryBufferUsed, StrBuffer, SizeNeeded);
    //  NarData.MemoryBufferUsed += (UINT32)SizeNeeded;
    //}
    
    //ExReleaseFastMutex(&NarData.FastMutex);
    ExFreeToPagedLookasideList(&NarData.LookAsideList, TempBuffer);
    
#endif
    
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
    
    NAR_PREOP_END:
    //ExFreeToPagedLookasideList(&NarData.LookAsideList, Buffer);
    
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
    
    NTSTATUS status;
    UNREFERENCED_PARAMETER(status);
    
    
    //
    //  If our instance is in the process of being torn down don't bother to
    //  log this record, free it now.
    //
    
    
    if (FlagOn(Flags, FLTFL_POST_OPERATION_DRAINING)) {
        
        return FLT_POSTOP_FINISHED_PROCESSING;
    }
    
    
    
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
    
    if (Flags & FLTFL_INSTANCE_SETUP_MANUAL_ATTACHMENT
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


