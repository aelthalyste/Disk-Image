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
      if (status = STATUS_OBJECT_NAME_COLLISION) {
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


#if 0

    UNICODE_STRING     uniName;
    OBJECT_ATTRIBUTES  objAttr;

    RtlInitUnicodeString(&uniName, L"\\SystemRoot\\Example.txt");  // or L"\\SystemRoot\\example.txt"
    InitializeObjectAttributes(&objAttr, &uniName,
      OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
      NULL, NULL);

    IO_STATUS_BLOCK IOSB = { 0 };

    status = FltCreateFile(
      NarData.Filter,
      NULL,
      &NarData.MetadataHandle,
      GENERIC_WRITE,
      &objAttr, &IOSB, 0,
      FILE_ATTRIBUTE_NORMAL,
      FILE_SHARE_WRITE | FILE_SHARE_READ,
      FILE_OPEN_IF,
      FILE_SYNCHRONOUS_IO_NONALERT | FILE_RANDOM_ACCESS,
      0, 0, 0);


    IO_STATUS_BLOCK Sb = { 0 };
    char WriteBuffer[512];


    if (NT_SUCCESS(status)) {
      DbgPrint("Successfully created file!\n");

      size_t Len = 0;

      status = RtlStringCbPrintfA(WriteBuffer, 512, "Driver entry success\n");
      status = RtlStringCbLengthA(WriteBuffer, 512, &Len);


      if (NT_SUCCESS(status)) {
        status = ZwWriteFile(NarData.MetadataHandle, 0, 0, 0, &Sb, WriteBuffer, (ULONG)Len, 0, 0);
        //status = FltWriteFile(0, &TEST, 0, (ULONG)Len, WriteBuffer, FLTFL_IO_OPERATION_SYNCHRONOUS_PAGING, 0, 0, 0);
        if (!NT_SUCCESS(status)) {
          DbgPrint("Failed to write file, status : %i\n", status);
        }
        else {
          DbgPrint("Successfully written to file\n");
        }
      }
      else {
        DbgPrint("Failed sprintf\n");
      }

      //status = FltClose(NarData.MetadataHandle);
      //if (NT_SUCCESS(status)) {
      //  
      //  DbgPrint("Successfully closed file\n");
      //}
    }
    else {
      DbgPrint("Failed to create file status : %i, inf ptr : %p, iosb status : %i\n", status, IOSB.Information, IOSB.Status);
    }
    //FltClose(NarData.MetadataHandle);

    else {

      // Special metadata file doesnt exist, create new one

      status = ZwCreateFile(&H,
        GENERIC_WRITE,
        &objAttr, &IOSB, NULL,
        FILE_ATTRIBUTE_NORMAL,
        0,
        FILE_CREATE,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_RANDOM_ACCESS,
        NULL, 0);
      if (NT_SUCCESS(status)) {
        size_t Len = 0;
        void* P = ExAllocateFromPagedLookasideList(&NarData.LookAsideList);

        if (P == NULL) {
          status = RtlStringCbPrintfA(WriteBuffer, 512, "couldnt allocate memory from paged pool");
          status = RtlStringCbLengthA(WriteBuffer, 512, &Len);
        }
        else {
          status = RtlStringCbPrintfA(WriteBuffer, 512, "allocated memory from paged pool");
          status = RtlStringCbLengthA(WriteBuffer, 512, &Len);

        }

        if (NT_SUCCESS(status)) {
          ZwWriteFile(H, 0, 0, 0, &Sb, &WriteBuffer, (ULONG)Len, 0, 0);
        }
        else {
          ZwWriteFile(H, 0, 0, 0, &Sb, WriteBuffer, 512, 0, 0);
        }
      }
      else {

        // TODO (Batuhan) : 
        // NOTE (Batuhan): Cant create file, fatal error.
        __leave;

      }

    }
#endif

    NarData.RWListSpinLock = 0;
    DbgPrint("Initialized RWListSpinLock EX_SPIN_LOCK\n");

    ExInitializePagedLookasideList(&NarData.LookAsideList, 0, 0, 0, NAR_LOOKASIDE_SIZE, NAR_TAG, 0);
    ExInitializeNPagedLookasideList(&NarData.GUIDCompareNPagedLookAsideList, 0, 0, 0, NAR_GUID_STR_SIZE, NAR_TAG, 0);

    NarData.VolumeRegionBuffer = ExAllocatePoolWithTag(NonPagedPoolNx, NAR_VOLUMEREGIONBUFFERSIZE, NAR_TAG);
    if (NarData.VolumeRegionBuffer == NULL) {
      DbgPrint("Couldnt allocate %I64d nonpaged memory for volume buffer\n", NAR_VOLUMEREGIONBUFFERSIZE);
      status = STATUS_FAILED_DRIVER_ENTRY;
      leave;
    }
    memset(NarData.VolumeRegionBuffer, 0, NAR_VOLUMEREGIONBUFFERSIZE);

    //Initializing each volume entry
    for (int i = 0; i < NAR_MAX_VOLUME_COUNT; i++) {

      KeInitializeSpinLock(&NarData.VolumeRegionBuffer[i].Spinlock); // KSPIN_LOCK initialization
      NarData.VolumeRegionBuffer[i].MemoryBuffer = 0;
      NarData.VolumeRegionBuffer[i].MemoryBuffer = ExAllocatePoolWithTag(NonPagedPoolNx, NAR_MEMORYBUFFER_SIZE, NAR_TAG);
      NarData.VolumeRegionBuffer[i].GUIDStrVol.MaximumLength = sizeof(NarData.VolumeRegionBuffer[i].Reserved) / sizeof(WCHAR);
      NarData.VolumeRegionBuffer[i].GUIDStrVol.Buffer = (void*)NarData.VolumeRegionBuffer[i].Reserved;
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

  //NOTE (Batuhan): Disabled 
  //NAR_CONNECTION_CONTEXT* CTX = ConnectionContext;
  //CTX->UserName[255] = '\0';
  //NarData.ClientPort = ClientPort;
  //NarData.UserModePID = CTX->PID;
  ////Copy user name
  //int Index = 0;
  //while (Index < 256) {
  //  NarData.UserName[Index] = CTX->UserName[Index];
  //  Index++;
  //  if (CTX->UserName[Index] == '\0') break;
  //}


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

  //NarData.Terminating = TRUE;

  DbgPrint("Filter unloading\n");

  FltCloseCommunicationPort(NarData.ServerPort);
  DbgPrint("Communication port closed\n");

  //KIRQL IRQL = ExAcquireSpinLockExclusive(NarData.Spinlock);

  //ExReleaseSpinLockExclusive(NarData.Spinlock, IRQL);
  //DbgPrint("Succesfully released spinlock");

  //FltClose(NarData.MetadataHandle);
  //DbgPrint("Metadata handle closed\n");

  if (NarData.VolumeRegionBuffer != NULL) {

    for (int i = 0; i < NAR_MAX_VOLUME_COUNT; i++) {
      if (NarData.VolumeRegionBuffer[i].MemoryBuffer != NULL) {
        DbgPrint("Freeing volume memory buffer %i\n", i);
        ExFreePoolWithTag(NarData.VolumeRegionBuffer[i].MemoryBuffer, NAR_TAG);
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
  NTSTATUS status;

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

      //
      //  Probe and capture input message: the message is raw user mode
      //  buffer, so need to protect with exception handler
      //

      NAR_COMMAND *Command = (NAR_COMMAND*)InputBuffer;
      if (Command->Type == NarCommandType_GetVolumeLog) {
        
        void *NPagedGUIDStrtBuffer = ExAllocateFromNPagedLookasideList(&NarData.GUIDCompareNPagedLookAsideList);
        void* NPagedDataBuffer = ExAllocatePoolWithTag(NonPagedPoolNx, OutputBufferSize, NAR_TAG);
        INT32 MemoryBufferUsed = 0;
        INT32 FoundVolume = FALSE;

        if (Command->VolumeGUIDStr != NULL 
          && NPagedGUIDStrtBuffer != NULL
          && OutputBuffer != NULL 
          && OutputBufferSize == NAR_MEMORYBUFFER_SIZE
          && NPagedDataBuffer != NULL
          && Command->VolumeGUIDStrSize == NAR_GUID_STR_SIZE) {
          
          memset(OutputBuffer, 0, OutputBufferSize);
          
          if (Command->VolumeGUIDStrSize >= NAR_GUID_STR_SIZE) {
            memcpy(NPagedGUIDStrtBuffer, Command->VolumeGUIDStrSize, NAR_GUID_STR_SIZE);
          }

          for (int i = 0; i < NAR_MAX_VOLUME_COUNT; i++) {
            KIRQL IRQL;
            KeAcquireSpinLock(&NarData.VolumeRegionBuffer[i].Spinlock, &IRQL);
            
            if (RtlCompareMemory(NPagedGUIDStrtBuffer, NarData.VolumeRegionBuffer[i].GUIDStrVol.Buffer, NAR_GUID_STR_SIZE) == NAR_GUID_STR_SIZE) {
              
              INT32 MBUSED = NAR_MB_USED(NarData.VolumeRegionBuffer[i].GUIDStrVol.Buffer);
              
              memcpy(NPagedDataBuffer, NarData.VolumeRegionBuffer[i].GUIDStrVol.Buffer, MBUSED);
              
              // reset memory buffer.
              memset(NarData.VolumeRegionBuffer[i].GUIDStrVol.Buffer, 0, NAR_MEMORYBUFFER_SIZE);
              NAR_INIT_MEMORYBUFFER(NarData.VolumeRegionBuffer[i].GUIDStrVol.Buffer);
              FoundVolume = TRUE;
            }

            KeReleaseSpinLock(&NarData.VolumeRegionBuffer[i].Spinlock, IRQL);

          }
          
          if (FoundVolume == FALSE) {

          }


          ExFreeToNPagedLookasideList(&NarData.GUIDCompareNPagedLookAsideList, NPagedGUIDStrtBuffer);
          ExFreePoolWithTag(NPagedDataBuffer, NAR_TAG);

        }
        else {
          DbgPrint("BIG IF BLOCK FAILED IN SPYMESSAGE\n"); // short but effective temporary message
        }

        
      }
      if (Command->Type == NarCommandType_AddVolume) {
        
        if (Command->VolumeGUIDStr != NULL) {

        }
        else {
            
        }
        
      }
      if (Command->Type == NarCommandType_QueryErrors) {
        
      }

      if (((wchar_t*)InputBuffer)[0] == L'C') {
        DbgPrint("Successfully received message\n");
        
        //UNICODE_STRING     uniName;
        //OBJECT_ATTRIBUTES  objAttr;
        //
        //RtlInitUnicodeString(&uniName, L"\\SystemRoot\\Example.txt");  // or L"\\SystemRoot\\example.txt"
        //InitializeObjectAttributes(&objAttr, &uniName,
        //  OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
        //  NULL, NULL);
        //
        //IO_STATUS_BLOCK IOSB = { 0 };
        //
        //
        //status = FltCreateFile(
        //  NarData.Filter,
        //  NULL,
        //  &NarData.MetadataHandle,
        //  GENERIC_WRITE,
        //  &objAttr, &IOSB, 0,
        //  FILE_ATTRIBUTE_NORMAL,
        //  FILE_SHARE_WRITE | FILE_SHARE_READ,
        //  FILE_OPEN_IF,
        //  FILE_SYNCHRONOUS_IO_NONALERT | FILE_RANDOM_ACCESS,
        //  0, 0, 0);
        //if (NT_SUCCESS(status)) {
        //  DbgPrint("Successfully created file\n");
        //}
        //else {
        //  DbgPrint("Couldnt create file : %i\n", status);
        //}


      }
      else {
        DbgPrint("Message is invaild\n");
      }

    } except(SpyExceptionFilter(GetExceptionInformation(), TRUE)) {
      DbgPrint("exception throwed\n");

      return GetExceptionCode();
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
  FLT_PREOP_CALLBACK_STATUS returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK; //assume we are NOT going to call our completion routine
  PFLT_FILE_NAME_INFORMATION nameInfo = NULL;
  UNICODE_STRING defaultName;
  PUNICODE_STRING nameToUse;
  NTSTATUS status;
  UNREFERENCED_PARAMETER(nameToUse);
  UNREFERENCED_PARAMETER(defaultName);
  UNREFERENCED_PARAMETER(nameToUse);
  UNREFERENCED_PARAMETER(CompletionContext);


  //ULONG PID = FltGetRequestorProcessId(Data);
  //if (Data->Iopb->TargetFileObject->Flags & FO_TEMPORARY_FILE || PID == NarData.UserModePID) {
  //  return 	FLT_PREOP_SUCCESS_NO_CALLBACK;
  //}
  if (Data->Iopb->TargetFileObject->Flags & FO_TEMPORARY_FILE) {
    DbgPrint("Temporary file skipped\n");
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
  }



#if 1

  ULONG SizeNeededForGUIDStr = 0;
  UNICODE_STRING GUIDString;
  RtlInitEmptyUnicodeString(&GUIDString, ExAllocateFromPagedLookasideList(&NarData.LookAsideList), NAR_LOOKASIDE_SIZE);

  status = FltGetVolumeGuidName(FltObjects->Volume, &GUIDString, &SizeNeededForGUIDStr);
  
  ExFreeToPagedLookasideList(&NarData.LookAsideList, GUIDString.Buffer);

#endif 


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
      if (0) {

        RtlUnicodeStringPrintf(&UniStr, L"\\Device\\HarddiskVolume%i\\Users\\%S\\AppData\\Local\\Temp", NarData.OsDeviceID, NarData.UserName);
        if (RtlPrefixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
          goto NAR_PREOP_END;
        }

        RtlUnicodeStringPrintf(&UniStr, L"\\Device\\HarddiskVolume%i\\Users\\%S\\AppData\\Local\\Microsoft\\Windows\\Temporary Internet Files", NarData.OsDeviceID, NarData.UserName);
        if (RtlPrefixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
          goto NAR_PREOP_END;
        }

        RtlUnicodeStringPrintf(&UniStr, L"\\Device\\HarddiskVolume%i\\Users\\%S\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\Cache", NarData.OsDeviceID, NarData.UserName);
        if (RtlPrefixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
          goto NAR_PREOP_END;
        }

        RtlUnicodeStringPrintf(&UniStr, L"\\Device\\HarddiskVolume%i\\Users\\%S\\AppData\\Local\\Opera Software", NarData.OsDeviceID, NarData.UserName);
        if (RtlPrefixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
          goto NAR_PREOP_END;
        }

        RtlUnicodeStringPrintf(&UniStr, L"\\Device\\HarddiskVolume%i\\Users\\%S\\AppData\\Local\\Mozilla\\Firefox\\Profiles", NarData.OsDeviceID, NarData.UserName);
        if (RtlPrefixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
          goto NAR_PREOP_END;
        }

      }
#endif


      //Suffix area
      RtlUnicodeStringPrintf(&UniStr, L".tmp");
      if (RtlSuffixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
        goto NAR_PREOP_END;
      }
      RtlUnicodeStringPrintf(&UniStr, L"~");
      if (RtlSuffixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
        goto NAR_PREOP_END;
      }
      RtlUnicodeStringPrintf(&UniStr, L".tib.metadata");
      if (RtlSuffixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
        goto NAR_PREOP_END;
      }
      RtlUnicodeStringPrintf(&UniStr, L".tib");
      if (RtlSuffixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
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
          Error = NAR_ERR_MAX_ITER;
          break;
        }

        status = FltFsControlFile(
          FltObjects->Instance,
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
          Error = NAR_ERR_TRINITY;
          break;
        }

        /*
        It is unlikely, but we should check overflows
        */

        if (ClusterMapBuffer.Extents[0].Lcn.QuadPart > MAXUINT32 || ClusterMapBuffer.Extents[0].NextVcn.QuadPart > MAXUINT32 || ClusterMapBuffer.StartingVcn.QuadPart > MAXUINT32) {
          Error = NAR_ERR_OVERFLOW;
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
              Error = NAR_ERR_REG_CANT_FILL;
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
      void* CompareBuffer = ExAllocateFromNPagedLookasideList(&NarData.GUIDCompareNPagedLookAsideList);
      memcpy(CompareBuffer, UniStr.Buffer, NAR_GUID_STR_SIZE);

      if (RecCount > 0 && UniStr.MaximumLength >= NAR_GUID_STR_SIZE) {
        UINT32 SizeNeededForMemoryBuffer = 2 * RecCount * sizeof(UINT32);

        for (int i = 0; i < NAR_MAX_VOLUME_COUNT; i++) {
          KIRQL IRQL;
          KeAcquireSpinLock(&NarData.VolumeRegionBuffer[i].Spinlock, &IRQL);

          if (RtlCompareMemory(CompareBuffer, NarData.VolumeRegionBuffer[i].GUIDStrVol.Buffer, NAR_GUID_STR_SIZE) == NAR_GUID_STR_SIZE) {
            
            if (NAR_MEMORYBUFFER_SIZE - NAR_MB_USED(NarData.VolumeRegionBuffer[i].MemoryBuffer) >= SizeNeededForMemoryBuffer) {
              NAR_MB_PUSH(NarData.VolumeRegionBuffer[i].MemoryBuffer, &P[0], SizeNeededForMemoryBuffer);
            }
            NAR_MB_MARK_NOT_ENOUGH_SPACE(NarData.VolumeRegionBuffer[i].MemoryBuffer);
            
          }

          KeReleaseSpinLock(&NarData.VolumeRegionBuffer[i].Spinlock, IRQL);

        }
      }

      

#pragma warning(pop)
#endif

      ExFreeToPagedLookasideList(&NarData.LookAsideList, UnicodeStrBuffer);
      ExFreeToNPagedLookasideList(&NarData.GUIDCompareNPagedLookAsideList, CompareBuffer);
      CompareBuffer = MM_BAD_POINTER;

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

    KIRQL oldirql = KeGetCurrentIrql();
    if (oldirql != PASSIVE_LEVEL) {
      DbgPrint("Not valid IRQL level, terminating now: %i\n", oldirql);
      ExFreeToPagedLookasideList(&NarData.LookAsideList, TempBuffer);
      return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    UNICODE_STRING     uniName;
    OBJECT_ATTRIBUTES  objAttr;
    HANDLE FileHandle;

    RtlInitUnicodeString(&uniName, L"\\DosDevices\\D:\\Example.txt");  // or L"\\SystemRoot\\example.txt"
    InitializeObjectAttributes(&objAttr, &uniName,
      OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
      NULL, NULL);


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

  return FLT_PREOP_SUCCESS_WITH_CALLBACK;
#endif 

  return FLT_PREOP_SUCCESS_NO_CALLBACK;

NAR_PREOP_END:
  //ExFreeToPagedLookasideList(&NarData.LookAsideList, Buffer);
  ExFreeToPagedLookasideList(&NarData.LookAsideList, GUIDString.Buffer);

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


  PFLT_TAG_DATA_BUFFER tagData;
  ULONG copyLength;

  UNREFERENCED_PARAMETER(FltObjects);
  UNREFERENCED_PARAMETER(copyLength);
  UNREFERENCED_PARAMETER(Data);
  UNREFERENCED_PARAMETER(CompletionContext);

  NTSTATUS status;



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
  return STATUS_SUCCESS;

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


