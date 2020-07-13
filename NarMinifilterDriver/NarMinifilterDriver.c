/*++

Module Name:

  NarMinifilterDriver.c

Abstract:

  This is the main module of the NarMinifilterDriver miniFilter driver.

Environment:

  Kernel mode

--*/

#define _NAR_KERNEL

#include <fltKernel.h>
#include <dontuse.h>
#include "NarMFVars.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")


ULONG_PTR OperationStatusCtx = 1;

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

ULONG gTraceFlags = 0;


#define PT_DBG_PRINT( _dbgLevel, _string )          \
(FlagOn(gTraceFlags,(_dbgLevel)) ?              \
 DbgPrint _string :                          \
 ((int)0))

/*************************************************************************
  Prototypes
*************************************************************************/

EXTERN_C_START



DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry(
  _In_ PDRIVER_OBJECT DriverObject,
  _In_ PUNICODE_STRING RegistryPath
);

NTSTATUS
NarMinifilterDriverInstanceSetup(
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
  _In_ DEVICE_TYPE VolumeDeviceType,
  _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
);

VOID
NarMinifilterDriverInstanceTeardownStart(
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
);

VOID
NarMinifilterDriverInstanceTeardownComplete(
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
);

NTSTATUS
NarMinifilterDriverUnload(
  _In_ FLT_FILTER_UNLOAD_FLAGS Flags
);

NTSTATUS
NarMinifilterDriverInstanceQueryTeardown(
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
NarMinifilterDriverPreOperation(
  _Inout_ PFLT_CALLBACK_DATA Data,
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

VOID
NarMinifilterDriverOperationStatusCallback(
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
  _In_ NTSTATUS OperationStatus,
  _In_ PVOID RequesterContext
);

FLT_POSTOP_CALLBACK_STATUS
NarMinifilterDriverPostOperation(
  _Inout_ PFLT_CALLBACK_DATA Data,
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _In_opt_ PVOID CompletionContext,
  _In_ FLT_POST_OPERATION_FLAGS Flags
);

FLT_PREOP_CALLBACK_STATUS
NarMinifilterDriverPreOperationNoPostOperation(
  _Inout_ PFLT_CALLBACK_DATA Data,
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

BOOLEAN
NarMinifilterDriverDoRequestOperationStatus(
  _In_ PFLT_CALLBACK_DATA Data
);

EXTERN_C_END

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, NarMinifilterDriverUnload)
#pragma alloc_text(PAGE, NarMinifilterDriverInstanceQueryTeardown)
#pragma alloc_text(PAGE, NarMinifilterDriverInstanceSetup)
#pragma alloc_text(PAGE, NarMinifilterDriverInstanceTeardownStart)
#pragma alloc_text(PAGE, NarMinifilterDriverInstanceTeardownComplete)
#endif

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
  {
    IRP_MJ_WRITE,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation
  },
#if 0 
  { IRP_MJ_CREATE,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_CREATE_NAMED_PIPE,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_CLOSE,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_READ,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_WRITE,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_QUERY_INFORMATION,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_SET_INFORMATION,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_QUERY_EA,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_SET_EA,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_FLUSH_BUFFERS,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_QUERY_VOLUME_INFORMATION,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_SET_VOLUME_INFORMATION,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_DIRECTORY_CONTROL,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_FILE_SYSTEM_CONTROL,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_DEVICE_CONTROL,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_INTERNAL_DEVICE_CONTROL,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_SHUTDOWN,
    0,
    NarMinifilterDriverPreOperationNoPostOperation,
    NULL },                               //post operations not supported

  { IRP_MJ_LOCK_CONTROL,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_CLEANUP,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_CREATE_MAILSLOT,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_QUERY_SECURITY,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_SET_SECURITY,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_QUERY_QUOTA,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_SET_QUOTA,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_PNP,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_RELEASE_FOR_SECTION_SYNCHRONIZATION,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_ACQUIRE_FOR_MOD_WRITE,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_RELEASE_FOR_MOD_WRITE,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_ACQUIRE_FOR_CC_FLUSH,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_RELEASE_FOR_CC_FLUSH,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_FAST_IO_CHECK_IF_POSSIBLE,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_NETWORK_QUERY_OPEN,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_MDL_READ,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_MDL_READ_COMPLETE,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_PREPARE_MDL_WRITE,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_MDL_WRITE_COMPLETE,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_VOLUME_MOUNT,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

  { IRP_MJ_VOLUME_DISMOUNT,
    0,
    NarMinifilterDriverPreOperation,
    NarMinifilterDriverPostOperation },

#endif 

  { IRP_MJ_OPERATION_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

  sizeof(FLT_REGISTRATION),         //  Size
  FLT_REGISTRATION_VERSION,           //  Version
  0,                                  //  Flags

  NULL,                               //  Context
  Callbacks,                          //  Operation callbacks

  NarMinifilterDriverUnload,                           //  MiniFilterUnload

  NarMinifilterDriverInstanceSetup,                    //  InstanceSetup
  NarMinifilterDriverInstanceQueryTeardown,            //  InstanceQueryTeardown
  NarMinifilterDriverInstanceTeardownStart,            //  InstanceTeardownStart
  NarMinifilterDriverInstanceTeardownComplete,         //  InstanceTeardownComplete

  NULL,                               //  GenerateFileName
  NULL,                               //  GenerateDestinationFileName
  NULL                                //  NormalizeNameComponent

};



NTSTATUS NarUserConnectCallback(
  _In_ PFLT_PORT ClientPort,
  _In_ PVOID ServerPortCookie,
  _In_reads_bytes_(SizeOfContext) PVOID ConnectionContext,
  _In_ ULONG SizeOfContext,
  _Flt_ConnectionCookie_Outptr_ PVOID* ConnectionCookie
);

VOID
NarUserDisconnectCallback(
  _In_opt_ PVOID ConnectionCookie
);

NTSTATUS
NarUserMessageCallback(
  _In_ PVOID ConnectionCookie,
  _In_reads_bytes_opt_(InputBufferSize) PVOID InputBuffer,
  _In_ ULONG InputBufferSize,
  _Out_writes_bytes_to_opt_(OutputBufferSize, *ReturnOutputBufferLength) PVOID OutputBuffer,
  _In_ ULONG OutputBufferSize,
  _Out_ PULONG ReturnOutputBufferLength
);

VOID
PushFsRecord(
  nar_record* Record
);
/*
VOID
NarSendMessage();
*/
NTSTATUS
NarMinifilterDriverInstanceSetup(
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
  _In_ DEVICE_TYPE VolumeDeviceType,
  _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
)
/*++

Routine Description:

  This routine is called whenever a new instance is created on a volume. This
  gives us a chance to decide if we need to attach to this volume or not.

  If this routine is not defined in the registration structure, automatic
  instances are always created.

Arguments:

  FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
    opaque handles to this filter, instance and its associated volume.

  Flags - Flags describing the reason for this attach request.

Return Value:

  STATUS_SUCCESS - attach
  STATUS_FLT_DO_NOT_ATTACH - do not attach

--*/
{
  UNREFERENCED_PARAMETER(FltObjects);
  UNREFERENCED_PARAMETER(Flags);
  UNREFERENCED_PARAMETER(VolumeDeviceType);
  UNREFERENCED_PARAMETER(VolumeFilesystemType);

  PAGED_CODE();

  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
    ("NarMinifilterDriver!NarMinifilterDriverInstanceSetup: Entered\n"));
  if (Flags & FLTFL_INSTANCE_SETUP_MANUAL_ATTACHMENT) {
    return STATUS_SUCCESS;
  }

  return STATUS_FLT_DO_NOT_ATTACH;
}


NTSTATUS
NarMinifilterDriverInstanceQueryTeardown(
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
)
/*++

Routine Description:

  This is called when an instance is being manually deleted by a
  call to FltDetachVolume or FilterDetach thereby giving us a
  chance to fail that detach request.

  If this routine is not defined in the registration structure, explicit
  detach requests via FltDetachVolume or FilterDetach will always be
  failed.

Arguments:

  FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
    opaque handles to this filter, instance and its associated volume.

  Flags - Indicating where this detach request came from.

Return Value:

  Returns the status of this operation.

--*/
{
  UNREFERENCED_PARAMETER(FltObjects);
  UNREFERENCED_PARAMETER(Flags);

  PAGED_CODE();

  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
    ("NarMinifilterDriver!NarMinifilterDriverInstanceQueryTeardown: Entered\n"));

  return STATUS_SUCCESS;
}


VOID
NarMinifilterDriverInstanceTeardownStart(
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
)
/*++

Routine Description:

  This routine is called at the start of instance teardown.

Arguments:

  FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
    opaque handles to this filter, instance and its associated volume.

  Flags - Reason why this instance is being deleted.

Return Value:

  None.

--*/
{
  UNREFERENCED_PARAMETER(FltObjects);
  UNREFERENCED_PARAMETER(Flags);

  PAGED_CODE();

  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
    ("NarMinifilterDriver!NarMinifilterDriverInstanceTeardownStart: Entered\n"));
}


VOID
NarMinifilterDriverInstanceTeardownComplete(
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
)
/*++

Routine Description:

  This routine is called at the end of instance teardown.

Arguments:

  FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
    opaque handles to this filter, instance and its associated volume.

  Flags - Reason why this instance is being deleted.

Return Value:

  None.

--*/
{
  UNREFERENCED_PARAMETER(FltObjects);
  UNREFERENCED_PARAMETER(Flags);

  PAGED_CODE();

  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
    ("NarMinifilterDriver!NarMinifilterDriverInstanceTeardownComplete: Entered\n"));
}


/*************************************************************************
  MiniFilter initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry(
  _In_ PDRIVER_OBJECT DriverObject,
  _In_ PUNICODE_STRING RegistryPath
)
/*++

Routine Description:

  This is the initialization routine for this miniFilter driver.  This
  registers with FltMgr and initializes all global data structures.

Arguments:

  DriverObject - Pointer to driver object created by the system to
    represent this driver.

  RegistryPath - Unicode string identifying where the parameters for this
    driver are located in the registry.

Return Value:

  Routine can return non success error codes.

--*/
{

  NTSTATUS status;
  OBJECT_ATTRIBUTES ObjAttr;
  UNICODE_STRING UniStr;
  PSECURITY_DESCRIPTOR SecDescription;

  UNREFERENCED_PARAMETER(RegistryPath);

  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
    ("NarMinifilterDriver!DriverEntry: Entered\n"));

  status = STATUS_UNSUCCESSFUL;

  memset(&GlobalFileLog, 0, sizeof(GlobalFileLog));

  try
  {

    KeInitializeSpinLock(&GlobalNarConfiguration.SpinLock);

    GlobalNarConfiguration.ShouldFilter = TRUE;

    //
    //  Register with FltMgr to tell it our callback routines
    //

    status = FltRegisterFilter(DriverObject,
      &FilterRegistration,
      &GlobalNarConfiguration.FilterHandle);

    if (!NT_SUCCESS(status)) {
      leave;
    }

    //
    //  Start filtering i/o
    //

    status = FltBuildDefaultSecurityDescriptor(&SecDescription, FLT_PORT_ALL_ACCESS);
    if (!(NT_SUCCESS(status))) {
      leave;
    }


    RtlInitUnicodeString(&UniStr, NAR_PORT_NAME);

    InitializeObjectAttributes(
      &ObjAttr,
      &UniStr,
      OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
      NULL,
      SecDescription
    );

    status = FltCreateCommunicationPort(
      GlobalNarConfiguration.FilterHandle,
      &GlobalNarConfiguration.ServerPort,
      &ObjAttr,
      NULL,
      NarUserConnectCallback,
      NarUserDisconnectCallback,
      NarUserMessageCallback,
      1
    );

    FltFreeSecurityDescriptor(SecDescription);

    if (!NT_SUCCESS(status)) {
      __leave;
    }

    status = FltStartFiltering(GlobalNarConfiguration.FilterHandle);

    if (!NT_SUCCESS(status)) {
      __leave;
    }

  }
  __finally {

    if (!NT_SUCCESS(status)) {

      if (GlobalNarConfiguration.ServerPort != NULL) {
        FltCloseCommunicationPort(GlobalNarConfiguration.ServerPort);
      }
      if (GlobalNarConfiguration.FilterHandle != NULL) {
        FltUnregisterFilter(GlobalNarConfiguration.FilterHandle);
      }

    }

  }

  return status;
}

NTSTATUS
NarMinifilterDriverUnload(
  _In_ FLT_FILTER_UNLOAD_FLAGS Flags
)
/*++

Routine Description:

  This is the unload routine for this miniFilter driver. This is called
  when the minifilter is about to be unloaded. We can fail this unload
  request if this is not a mandatory unload indicated by the Flags
  parameter.

Arguments:

  Flags - Indicating if this is a mandatory unload.

Return Value:

  Returns STATUS_SUCCESS.

--*/
{
  UNREFERENCED_PARAMETER(Flags);

  PAGED_CODE();

  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
    ("NarMinifilterDriver!NarMinifilterDriverUnload: Entered\n"));


  FltCloseCommunicationPort(GlobalNarConfiguration.ServerPort);
  FltUnregisterFilter(GlobalNarConfiguration.FilterHandle);

  return STATUS_SUCCESS;
}


/*************************************************************************
  MiniFilter callback routines.
*************************************************************************/
FLT_PREOP_CALLBACK_STATUS
NarMinifilterDriverPreOperation(
  _Inout_ PFLT_CALLBACK_DATA Data,
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
)
/*++

Routine Description:

  This routine is a pre-operation dispatch routine for this miniFilter.

  This is non-pageable because it could be called on the paging path

Arguments:

  Data - Pointer to the filter callbackData that is passed to us.

  FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
    opaque handles to this filter, instance, its associated volume and
    file object.

  CompletionContext - The context for the completion routine for this
    operation.

Return Value:

  The return value is the status of the operation.

--*/
{
  NTSTATUS status;
  UNREFERENCED_PARAMETER(status);
  UNREFERENCED_PARAMETER(Data);
  UNREFERENCED_PARAMETER(FltObjects);
  UNREFERENCED_PARAMETER(CompletionContext);

  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
    ("NarMinifilterDriver!NarMinifilterDriverPreOperation: Entered\n"));


  //Skip temporary files
  if (Data->Iopb->TargetFileObject->Flags & FO_TEMPORARY_FILE) {
    return 	FLT_PREOP_SUCCESS_NO_CALLBACK;
  }

  //if (GlobalNarConfiguration.ShouldFilter == FALSE) {
  //	return FLT_PREOP_SUCCESS_NO_CALLBACK;
  //}

#if 1
  STARTING_VCN_INPUT_BUFFER StartingInputVCNBuffer;
  RETRIEVAL_POINTERS_BUFFER ClusterMapBuffer;
  ClusterMapBuffer.StartingVcn.QuadPart = 0;
  StartingInputVCNBuffer.StartingVcn.QuadPart = 0;

  ULONG BytesReturned = 0;

  ULONGLONG RegionLen = 0;

  ULONG ClusterSize = Data->Iopb->TargetFileObject->DeviceObject->SectorSize * 8; //TODO cluster size might not be 8*sector. check this 

  LARGE_INTEGER WriteOffsetLargeInt = Data->Iopb->Parameters.Write.ByteOffset;
  ULONG WriteLen = Data->Iopb->Parameters.Write.Length;
  ULONGLONG NClustersToWrite = (WriteLen / (ClusterSize)+1);

  ULONGLONG ClusterWriteStartOffset = WriteOffsetLargeInt.QuadPart / (ClusterSize);

  UINT32 MaxIteration = 0;

  nar_record NarFsRecord;

  NarFsRecord.Temp[0] = WriteLen;
  NarFsRecord.Temp[1] = NClustersToWrite;
  NarFsRecord.Temp[2] = ClusterWriteStartOffset;

  BOOLEAN HeadFound = FALSE;
  BOOLEAN CeilTest = FALSE;

  UNICODE_STRING UString;
  UString.MaximumLength = MAX_NAME_CHAR_COUNT;
  PFLT_FILE_NAME_INFORMATION inf = NULL;
  status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &inf);
  if (NT_SUCCESS(status)) {
    wcsncpy(NarFsRecord.Name, inf->Name.Buffer, MAX_NAME_CHAR_COUNT);
  }
  else {
    NarFsRecord.Err = NE_GETFILENAMEINF_FUNC_FAILED;
    NarFsRecord.Reserved = status;
    NarFsRecord.Type = NarError;
    PushFsRecordToLog(&NarFsRecord);
  }

  for (;;) {
    MaxIteration++;
    if (MaxIteration > 512) {
      NarFsRecord.Type = NarError;
      NarFsRecord.Err = NE_MAX_ITER_EXCEEDED;
      NarFsRecord.Reserved = NE_MAX_ITER_EXCEEDED;
      PushFsRecordToLog(&NarFsRecord);
      break;
    }

    NarFsRecord.Start = 0;
    NarFsRecord.OperationSize = 0;
    NarFsRecord.Type = NarInf;

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

      NarFsRecord.Type = NarError;
      NarFsRecord.Err = NE_RETRIEVAL_POINTERS;
      NarFsRecord.Reserved = status;

      PushFsRecordToLog(&NarFsRecord);

      break;
    }

    if (!HeadFound) {
      CeilTest = ((LONGLONG)ClusterWriteStartOffset < ClusterMapBuffer.Extents[0].NextVcn.QuadPart);
    }
    else {
      CeilTest = TRUE;
    }

    if ((LONGLONG)ClusterWriteStartOffset >= StartingInputVCNBuffer.StartingVcn.QuadPart && CeilTest) {
      RegionLen = (ClusterMapBuffer.Extents[0].NextVcn.QuadPart - ClusterMapBuffer.StartingVcn.QuadPart);

      if (!HeadFound) {

        //Starting position of the write operation
        ULONGLONG OffsetFromRegionStart = (ClusterWriteStartOffset - ClusterMapBuffer.StartingVcn.QuadPart);

        NarFsRecord.Temp[3] = RegionLen;

        PushFsRecordToLog(&NarFsRecord);

        if (ClusterMapBuffer.Extents->NextVcn.QuadPart - OffsetFromRegionStart < NClustersToWrite) {
          //Region overflow
          NarFsRecord.Type = NarInf;
          NarFsRecord.Start = ClusterMapBuffer.Extents[0].Lcn.QuadPart + OffsetFromRegionStart;
          NarFsRecord.OperationSize = RegionLen - OffsetFromRegionStart;
          NClustersToWrite -= (RegionLen - OffsetFromRegionStart);
          PushFsRecordToLog(&NarFsRecord);
        }
        else {
          //Operation fits the region
          NarFsRecord.Type = NarInf;
          NarFsRecord.Start = ClusterMapBuffer.Extents[0].Lcn.QuadPart + OffsetFromRegionStart;
          NarFsRecord.OperationSize = NClustersToWrite;
          NClustersToWrite = 0;
          PushFsRecordToLog(&NarFsRecord);
        }

        ClusterWriteStartOffset = ClusterMapBuffer.Extents[0].NextVcn.QuadPart;

        HeadFound = TRUE;

      }
      else { // HeadFound
        //Write operation falls over other region(s)

        if ((NClustersToWrite - RegionLen) > 0) {
          //write operation does not fit this region
          NarFsRecord.Start = ClusterMapBuffer.Extents[0].Lcn.QuadPart;
          NarFsRecord.OperationSize = RegionLen;
          PushFsRecordToLog(&NarFsRecord);
          NClustersToWrite -= RegionLen;
        }
        else {
          NarFsRecord.Start = ClusterMapBuffer.Extents[0].Lcn.QuadPart;
          NarFsRecord.OperationSize = NClustersToWrite;
          PushFsRecordToLog(&NarFsRecord);
          NClustersToWrite = 0;
          break;
        }
        ClusterWriteStartOffset = ClusterMapBuffer.Extents[0].NextVcn.QuadPart;

      }
    }

    if (NClustersToWrite <= 0) {
      break;
    }

    StartingInputVCNBuffer.StartingVcn = ClusterMapBuffer.Extents->NextVcn;

    if (status == STATUS_END_OF_FILE) {
      break;
    }

  }

#endif

  return FLT_PREOP_SUCCESS_WITH_CALLBACK;

}



VOID
NarMinifilterDriverOperationStatusCallback(
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
  _In_ NTSTATUS OperationStatus,
  _In_ PVOID RequesterContext
)
/*++

Routine Description:

  This routine is called when the given operation returns from the call
  to IoCallDriver.  This is useful for operations where STATUS_PENDING
  means the operation was successfully queued.  This is useful for OpLocks
  and directory change notification operations.

  This callback is called in the context of the originating thread and will
  never be called at DPC level.  The file object has been correctly
  referenced so that you can access it.  It will be automatically
  dereferenced upon return.

  This is non-pageable because it could be called on the paging path

Arguments:

  FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
    opaque handles to this filter, instance, its associated volume and
    file object.

  RequesterContext - The context for the completion routine for this
    operation.

  OperationStatus -

Return Value:

  The return value is the status of the operation.

--*/
{
  UNREFERENCED_PARAMETER(FltObjects);

  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
    ("NarMinifilterDriver!NarMinifilterDriverOperationStatusCallback: Entered\n"));

  PT_DBG_PRINT(PTDBG_TRACE_OPERATION_STATUS,
    ("NarMinifilterDriver!NarMinifilterDriverOperationStatusCallback: Status=%08x ctx=%p IrpMj=%02x.%02x \"%s\"\n",
      OperationStatus,
      RequesterContext,
      ParameterSnapshot->MajorFunction,
      ParameterSnapshot->MinorFunction,
      FltGetIrpName(ParameterSnapshot->MajorFunction)));
}


FLT_POSTOP_CALLBACK_STATUS
NarMinifilterDriverPostOperation(
  _Inout_ PFLT_CALLBACK_DATA Data,
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _In_opt_ PVOID CompletionContext,
  _In_ FLT_POST_OPERATION_FLAGS Flags
)
/*++

Routine Description:

  This routine is the post-operation completion routine for this
  miniFilter.

  This is non-pageable because it may be called at DPC level.

Arguments:

  Data - Pointer to the filter callbackData that is passed to us.

  FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
    opaque handles to this filter, instance, its associated volume and
    file object.

  CompletionContext - The completion context set in the pre-operation routine.

  Flags - Denotes whether the completion is successful or is being drained.

Return Value:

  The return value is the status of the operation.

--*/
{
  UNREFERENCED_PARAMETER(Data);
  UNREFERENCED_PARAMETER(FltObjects);
  UNREFERENCED_PARAMETER(CompletionContext);
  UNREFERENCED_PARAMETER(Flags);

  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
    ("NarMinifilterDriver!NarMinifilterDriverPostOperation: Entered\n"));

  if ((Data->IoStatus.Status == STATUS_SUCCESS)) {

    UNREFERENCED_PARAMETER(Data);
  }

  return FLT_POSTOP_FINISHED_PROCESSING;
}


FLT_PREOP_CALLBACK_STATUS
NarMinifilterDriverPreOperationNoPostOperation(
  _Inout_ PFLT_CALLBACK_DATA Data,
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
)
/*++

Routine Description:

  This routine is a pre-operation dispatch routine for this miniFilter.

  This is non-pageable because it could be called on the paging path

Arguments:

  Data - Pointer to the filter callbackData that is passed to us.

  FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
    opaque handles to this filter, instance, its associated volume and
    file object.

  CompletionContext - The context for the completion routine for this
    operation.

Return Value:

  The return value is the status of the operation.

--*/
{
  UNREFERENCED_PARAMETER(Data);
  UNREFERENCED_PARAMETER(FltObjects);
  UNREFERENCED_PARAMETER(CompletionContext);

  PT_DBG_PRINT(PTDBG_TRACE_ROUTINES,
    ("NarMinifilterDriver!NarMinifilterDriverPreOperationNoPostOperation: Entered\n"));

  // This template code does not do anything with the callbackData, but
  // rather returns FLT_PREOP_SUCCESS_NO_CALLBACK.
  // This passes the request down to the next miniFilter in the chain.

  return FLT_PREOP_SUCCESS_NO_CALLBACK;
}


BOOLEAN
NarMinifilterDriverDoRequestOperationStatus(
  _In_ PFLT_CALLBACK_DATA Data
)
/*++

Routine Description:

  This identifies those operations we want the operation status for.  These
  are typically operations that return STATUS_PENDING as a normal completion
  status.

Arguments:

Return Value:

  TRUE - If we want the operation status
  FALSE - If we don't

--*/
{
  PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;

  //
  //  return boolean state based on which operations we are interested in
  //

  return (BOOLEAN)

    //
    //  Check for oplock operations
    //

    (((iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) &&
      ((iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_FILTER_OPLOCK) ||
        (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_BATCH_OPLOCK) ||
        (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_1) ||
        (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_2)))

      ||

      //
      //    Check for directy change notification
      //

      ((iopb->MajorFunction == IRP_MJ_DIRECTORY_CONTROL) &&
        (iopb->MinorFunction == IRP_MN_NOTIFY_CHANGE_DIRECTORY))
      );
}

NTSTATUS NarUserConnectCallback(
  _In_ PFLT_PORT ClientPort,
  _In_ PVOID ServerPortCookie,
  _In_reads_bytes_(SizeOfContext) PVOID ConnectionContext,
  _In_ ULONG SizeOfContext,
  _Flt_ConnectionCookie_Outptr_ PVOID* ConnectionCookie
) {
  PAGED_CODE();

  UNREFERENCED_PARAMETER(ServerPortCookie);
  UNREFERENCED_PARAMETER(ConnectionContext);
  UNREFERENCED_PARAMETER(SizeOfContext);
  UNREFERENCED_PARAMETER(ConnectionCookie);

  GlobalNarConfiguration.ClientPort = ClientPort;
  return STATUS_SUCCESS;
}

VOID
NarUserDisconnectCallback(
  _In_opt_ PVOID ConnectionCookie
) {
  UNREFERENCED_PARAMETER(ConnectionCookie);

  FltCloseClientPort(GlobalNarConfiguration.FilterHandle, &GlobalNarConfiguration.ClientPort);
}


NTSTATUS
NarUserMessageCallback(
  _In_ PVOID ConnectionCookie,
  _In_reads_bytes_opt_(InputBufferSize) PVOID InputBuffer,
  _In_ ULONG InputBufferSize,
  _Out_writes_bytes_to_opt_(OutputBufferSize, *ReturnOutputBufferLength) PVOID OutputBuffer,
  _In_ ULONG OutputBufferSize,
  _Out_ PULONG ReturnOutputBufferLength
) {
  UNREFERENCED_PARAMETER(ConnectionCookie);
  UNREFERENCED_PARAMETER(InputBuffer);
  UNREFERENCED_PARAMETER(InputBufferSize);
  UNREFERENCED_PARAMETER(OutputBuffer);
  UNREFERENCED_PARAMETER(OutputBufferSize);
  UNREFERENCED_PARAMETER(ReturnOutputBufferLength);

  //TODO check input buffer

#if 1
  * ReturnOutputBufferLength = 0;
  KIRQL OldIrql = 0;
  BOOLEAN SpinLockAcquired = FALSE;

  __try {
    //ProbeForRead(InputBuffer, InputBufferSize, 1);
    ProbeForWrite(OutputBuffer, OutputBufferSize, 1);

    KeAcquireSpinLock(&GlobalNarConfiguration.SpinLock, &OldIrql);
    SpinLockAcquired = TRUE;

    //UINT32 Val = *(UINT32*)InputBuffer;
    UINT32 Val = GET_RECORDS;

    if (Val == RESET_BUFFER) {
      GlobalFileLog.Count = 0;
      __leave;
    }
    if (Val == START_FILTERING) {
      GlobalNarConfiguration.ShouldFilter = TRUE;
      GlobalFileLog.Count = 0;
      __leave;
    }
    if (Val == STOP_FILTERING) {
      GlobalNarConfiguration.ShouldFilter = FALSE;
      GlobalFileLog.Count = 0;
      __leave;
    }

    nar_record* Log = OutputBuffer; //Offset to records array

    if (GlobalFileLog.Overflowed == TRUE) {
      *ReturnOutputBufferLength = MAX_BUFFER_SIZE + 10;
      __leave;
    }

    for (UINT32 Indx = 0; Indx < GlobalFileLog.Count; Indx++) {
      memcpy(&Log[Indx], &GlobalFileLog.Record[Indx], sizeof(nar_record));
    }

    (*ReturnOutputBufferLength) += GlobalFileLog.Count * (sizeof(nar_record));

    GlobalFileLog.Count = 0;

  }
  __finally {
    if (SpinLockAcquired == TRUE) {
      //KeReleaseSpinLock(&GlobalNarConfiguration.SpinLock, OldIrql);
    }
  }


#endif 

  return STATUS_SUCCESS; //TODO
}

VOID
PushFsRecord(
  nar_record* Record
) {
  if (GlobalFileLog.Count >= 126) {
    GlobalFileLog.Count = 125;
    GlobalFileLog.Overflowed = TRUE;
  }

  memcpy(&GlobalFileLog.Record[GlobalFileLog.Count], Record, sizeof(nar_record));

  GlobalFileLog.Count++;
}
