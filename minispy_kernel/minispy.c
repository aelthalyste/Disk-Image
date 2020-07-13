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

MINISPY_DATA MiniSpyData;
NTSTATUS StatusToBreakOn = 0;

#define NAR_GLOBAL_BUFFER_SIZE 512
WCHAR GlobalWCharBuffer[NAR_GLOBAL_BUFFER_SIZE]; // max 256 for

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

    MiniSpyData.NameQueryMethod = DEFAULT_NAME_QUERY_METHOD;

    MiniSpyData.DriverObject = DriverObject;


    //
    //  Now that our global configuration is complete, register with FltMgr.
    //

    status = FltRegisterFilter(DriverObject,
      &FilterRegistration,
      &MiniSpyData.Filter);

    if (!NT_SUCCESS(status)) {

      leave;
    }


    status = FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);

    if (!NT_SUCCESS(status)) {
      leave;
    }

    RtlInitUnicodeString(&uniString, MINISPY_PORT_NAME);

    InitializeObjectAttributes(&oa,
      &uniString,
      OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
      NULL,
      sd);

    status = FltCreateCommunicationPort(MiniSpyData.Filter,
      &MiniSpyData.ServerPort,
      &oa,
      NULL,
      SpyConnect,
      SpyDisconnect,
      SpyMessage,
      1);

    FltFreeSecurityDescriptor(sd);

    if (!NT_SUCCESS(status)) {
      __leave;
    }


    ExInitializePagedLookasideList(&MiniSpyData.LAL, 0, 0, 0, 1024, 'NAR', 0);


    RtlInitEmptyUnicodeString(&MiniSpyData.Nar.IgnoreBuffer, GlobalWCharBuffer, NAR_GLOBAL_BUFFER_SIZE);



    UNICODE_STRING     uniName;
    OBJECT_ATTRIBUTES  objAttr;

    RtlInitUnicodeString(&uniName, L"\\SystemRoot\\Example.txt");  // or L"\\SystemRoot\\example.txt"
    InitializeObjectAttributes(&objAttr, &uniName,
      OBJ_CASE_INSENSITIVE,
      NULL, NULL);

    HANDLE H;
    IO_STATUS_BLOCK IOSB = { 0 };

    status = ZwCreateFile(&H,
      GENERIC_WRITE,
      &objAttr, &IOSB, NULL,
      FILE_ATTRIBUTE_NORMAL,
      0,
      FILE_OPEN,
      FILE_SYNCHRONOUS_IO_NONALERT | FILE_RANDOM_ACCESS,
      NULL, 0);

    IO_STATUS_BLOCK Sb = { 0 };
    char WriteBuffer[512];


    if (NT_SUCCESS(status)) {
      size_t Len = 0;

      void* P = ExAllocateFromPagedLookasideList(&MiniSpyData.LAL);

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
        void* P = ExAllocateFromPagedLookasideList(&MiniSpyData.LAL);

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

    ZwFlushBuffersFile(H, &Sb);

    ZwClose(H);


    //
    //  We are now ready to start filtering
    //

    status = FltStartFiltering(MiniSpyData.Filter);

  }
  finally {

    if (!NT_SUCCESS(status)) {

      if (NULL != MiniSpyData.ServerPort) {
        FltCloseCommunicationPort(MiniSpyData.ServerPort);
      }

      if (NULL != MiniSpyData.Filter) {
        FltUnregisterFilter(MiniSpyData.Filter);
      }


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

  UNREFERENCED_PARAMETER(ServerPortCookie);
  UNREFERENCED_PARAMETER(ConnectionContext);
  UNREFERENCED_PARAMETER(SizeOfContext);
  UNREFERENCED_PARAMETER(ConnectionCookie);

  FLT_ASSERT(MiniSpyData.ClientPort == NULL);
  NAR_CONNECTION_CONTEXT* CTX = ConnectionContext;
  CTX->UserName[255] = '\0';

  MiniSpyData.ClientPort = ClientPort;

  MiniSpyData.Nar.UserModePID = CTX->PID;

  //Copy user name
  int Index = 0;
  while (Index < 256) {
    MiniSpyData.Nar.UserName[Index] = CTX->UserName[Index];
    Index++;
    if (CTX->UserName[Index] == '\0') break;
  }


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

  FltCloseClientPort(MiniSpyData.Filter, &MiniSpyData.ClientPort);
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

  ExDeleteLookasideListEx(&MiniSpyData.LAL);

  FltCloseCommunicationPort(MiniSpyData.ServerPort);

  FltUnregisterFilter(MiniSpyData.Filter);


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
  MINISPY_COMMAND command;
  NTSTATUS status;

  PAGED_CODE();

  UNREFERENCED_PARAMETER(ConnectionCookie);

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

  if ((InputBuffer != NULL) &&
    (InputBufferSize >= (FIELD_OFFSET(COMMAND_MESSAGE, Command) +
      sizeof(MINISPY_COMMAND)))) {

    try {

      //
      //  Probe and capture input message: the message is raw user mode
      //  buffer, so need to protect with exception handler
      //

      command = ((PCOMMAND_MESSAGE)InputBuffer)->Command;

    } except(SpyExceptionFilter(GetExceptionInformation(), TRUE)) {

      return GetExceptionCode();
    }

    switch (command) {

    case GetMiniSpyLog:

      //
      //  Return as many log records as can fit into the OutputBuffer
      //

      if ((OutputBuffer == NULL) || (OutputBufferSize == 0)) {

        status = STATUS_INVALID_PARAMETER;
        break;
      }

      //
      //  We want to validate that the given buffer is POINTER
      //  aligned.  But if this is a 64bit system and we want to
      //  support 32bit applications we need to be careful with how
      //  we do the check.  Note that the way SpyGetLog is written
      //  it actually does not care about alignment but we are
      //  demonstrating how to do this type of check.
      //

#if defined(_WIN64)

      if (IoIs32bitProcess(NULL)) {

        //
        //  Validate alignment for the 32bit process on a 64bit
        //  system
        //

        if (!IS_ALIGNED(OutputBuffer, sizeof(ULONG))) {

          status = STATUS_DATATYPE_MISALIGNMENT;
          break;
        }

      }
      else {

#endif

        if (!IS_ALIGNED(OutputBuffer, sizeof(PVOID))) {

          status = STATUS_DATATYPE_MISALIGNMENT;
          break;
        }

#if defined(_WIN64)

      }

#endif

      break;


    case GetMiniSpyVersion:

      //
      //  Return version of the MiniSpy filter driver.  Verify
      //  we have a valid user buffer including valid
      //  alignment
      //

      if ((OutputBufferSize < sizeof(MINISPYVER)) ||
        (OutputBuffer == NULL)) {

        status = STATUS_INVALID_PARAMETER;
        break;
      }

      //
      //  Validate Buffer alignment.  If a minifilter cares about
      //  the alignment value of the buffer pointer they must do
      //  this check themselves.  Note that a try/except will not
      //  capture alignment faults.
      //

      if (!IS_ALIGNED(OutputBuffer, sizeof(ULONG))) {

        status = STATUS_DATATYPE_MISALIGNMENT;
        break;
      }

      //
      //  Protect access to raw user-mode output buffer with an
      //  exception handler
      //

      try {

        ((PMINISPYVER)OutputBuffer)->Major = MINISPY_MAJ_VERSION;
        ((PMINISPYVER)OutputBuffer)->Minor = MINISPY_MIN_VERSION;

      } except(SpyExceptionFilter(GetExceptionInformation(), TRUE)) {

        return GetExceptionCode();
      }

      *ReturnOutputBufferLength = sizeof(MINISPYVER);
      status = STATUS_SUCCESS;
      break;

    default:
      status = STATUS_INVALID_PARAMETER;
      break;
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

  ULONG PID = FltGetRequestorProcessId(Data);
  //return returnStatus;



  if (Data->Iopb->TargetFileObject->Flags & FO_TEMPORARY_FILE || PID == MiniSpyData.Nar.UserModePID) {
    return 	FLT_PREOP_SUCCESS_NO_CALLBACK;
  }

  void* Buffer = ExAllocateFromLookasideListEx(&MiniSpyData.LAL);
  if (Buffer == NULL) {
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
  }


  if (FltObjects->FileObject != NULL) {

    status = FltGetFileNameInformation(Data,
      FLT_FILE_NAME_NORMALIZED |
      MiniSpyData.NameQueryMethod,
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
      RtlInitEmptyUnicodeString(&UniStr, Buffer, 1024);


      //UNICODE_STRING TEMPSTR =  RTL_CONSTANT_STRING("What is that");

      //nameInfo->Name;

      RtlUnicodeStringPrintf(&UniStr, L"\\Device\\HarddiskVolume%i\\Users\\%S\\AppData\\Local\\Temp", MiniSpyData.Nar.OsDeviceID, MiniSpyData.Nar.UserName);
      if (RtlPrefixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
        goto NAR_PREOP_END;
      }

      RtlUnicodeStringPrintf(&UniStr, L"\\Device\\HarddiskVolume%i\\Users\\%S\\AppData\\Local\\Microsoft\\Windows\\Temporary Internet Files", MiniSpyData.Nar.OsDeviceID, MiniSpyData.Nar.UserName);
      if (RtlPrefixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
        goto NAR_PREOP_END;
      }

      RtlUnicodeStringPrintf(&UniStr, L"\\Device\\HarddiskVolume%i\\Users\\%S\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\Cache", MiniSpyData.Nar.OsDeviceID, MiniSpyData.Nar.UserName);
      if (RtlPrefixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
        goto NAR_PREOP_END;
      }

      RtlUnicodeStringPrintf(&UniStr, L"\\Device\\HarddiskVolume%i\\Users\\%S\\AppData\\Local\\Opera Software", MiniSpyData.Nar.OsDeviceID, MiniSpyData.Nar.UserName);
      if (RtlPrefixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
        goto NAR_PREOP_END;
      }

      RtlUnicodeStringPrintf(&UniStr, L"\\Device\\HarddiskVolume%i\\Users\\%S\\AppData\\Local\\Mozilla\\Firefox\\Profiles", MiniSpyData.Nar.OsDeviceID, MiniSpyData.Nar.UserName);
      if (RtlPrefixUnicodeString(&UniStr, &nameInfo->Name, FALSE)) {
        goto NAR_PREOP_END;
      }


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


      {
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
        }P[5];
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
              else if (RecCount < 5) {
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
                Error = NAR_ERR_REG_CANT_FILL;
                break;
              }

              ClusterWriteStartOffset = (UINT32)ClusterMapBuffer.Extents[0].NextVcn.QuadPart;
            }

          }

          if (NClustersToWrite <= 0) {
            break;
          }
          if (RecCount == 5) {
            break;
          }

          StartingInputVCNBuffer.StartingVcn = ClusterMapBuffer.Extents->NextVcn;

          if (status == STATUS_END_OF_FILE) {
            break;
          }

        }




#pragma warning(pop)
#endif

      }

    }
  }

  ExFreeToLookasideListEx(&MiniSpyData.LAL, Buffer);

NAR_PREOP_END:
  return returnStatus;
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


