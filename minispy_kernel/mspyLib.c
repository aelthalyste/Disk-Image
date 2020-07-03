/*++

Copyright (c) 1989-2002  Microsoft Corporation

Module Name:

  mspyLib.c

Abstract:
  This contains library support routines for MiniSpy

Environment:

  Kernel mode

--*/

#include <initguid.h>
#include <stdio.h>

#include "mspyKern.h"


//
// Can't pull in wsk.h until after MINISPY_VISTA is defined
//
#include <ntifs.h>

#if MINISPY_VISTA
#include <ntifs.h>

#include <wsk.h>
#endif

//---------------------------------------------------------------------------
//  Assign text sections for each routine.
//---------------------------------------------------------------------------

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, SpyReadDriverParameters)
#if MINISPY_VISTA
#pragma alloc_text(PAGE, SpyBuildEcpDataString)
#pragma alloc_text(PAGE, SpyParseEcps)
#endif
#endif

UCHAR TxNotificationToMinorCode(
  _In_ ULONG TxNotification
)
/*++

Routine Description:

  This routine has been written to convert a transaction notification code
  to an Irp minor code. This function is needed because RECORD_DATA has a
  UCHAR field for the Irp minor code whereas TxNotification is ULONG. As
  of now all this function does is compute log_base_2(TxNotification) + 1.
  That fits our need for now but might have to be evolved later. This
  function is intricately tied with the enumeration TRANSACTION_NOTIFICATION_CODES
  in mspyLog.h and the case statements related to transactions in the function
  PrintIrpCode (Minispy\User\mspyLog.c).

Arguments:

  TxNotification - The transaction notification received.

Return Value:

  0 if TxNotification is 0;
  log_base_2(TxNotification) + 1 otherwise.

--*/
{
  UCHAR count = 0;

  if (TxNotification == 0)
    return 0;

  //
  //  This assert verifies if no more than one flag is set
  //  in the TxNotification variable. TxNotification flags are
  //  supposed to be mutually exclusive. The assert below verifies
  //  if the value of TxNotification is a power of 2. If it is not
  //  then we will break.
  //

  FLT_ASSERT(!((TxNotification) & (TxNotification - 1)));

  while (TxNotification) {

    count++;

    TxNotification >>= 1;

    //
    //  If we hit this assert then we have more notification codes than
    //  can fit in a UCHAR. We need to revaluate our approach for
    //  storing minor codes now.
    //

    FLT_ASSERT(count != 0);
  }

  return (count);
}


//---------------------------------------------------------------------------
//                    Log Record allocation routines
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
//                    Logging routines
//---------------------------------------------------------------------------






VOID
SpyLogPreOperationData(
  _In_ PFLT_CALLBACK_DATA Data,
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _Inout_ PRECORD_LIST RecordList
)
/*++

Routine Description:

  This is called from the pre-operation callback routine to copy the
  necessary information into the log record.

  NOTE:  This code must be NON-PAGED because it can be called on the
      paging path.

Arguments:

  Data - The Data structure that contains the information we want to record.

  FltObjects - Pointer to the io objects involved in this operation.

  RecordList - Where we want to save the data

Return Value:

  None.

--*/
{
  PRECORD_DATA recordData = &RecordList->LogRecord.Data;
  PDEVICE_OBJECT devObj;
  NTSTATUS status;

  status = FltGetDeviceObject(FltObjects->Volume, &devObj);
  if (NT_SUCCESS(status)) {

    ObDereferenceObject(devObj);

  }
  else {

    devObj = NULL;
  }

  //
  //  Save the information we want
  //
  {
    recordData->CallbackMajorId = Data->Iopb->MajorFunction;
    recordData->CallbackMinorId = Data->Iopb->MinorFunction;
    recordData->IrpFlags = Data->Iopb->IrpFlags;
    recordData->Flags = Data->Flags;

    recordData->DeviceObject = (FILE_ID)devObj;
    recordData->FileObject = (FILE_ID)FltObjects->FileObject;
    recordData->Transaction = (FILE_ID)FltObjects->Transaction;
    recordData->ProcessId = (FILE_ID)PsGetCurrentProcessId();
    recordData->ThreadId = (FILE_ID)PsGetCurrentThreadId();

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

    UINT32 MaxIteration = 0;
    recordData->RecCount = 0;
    recordData->Error = 0;

    BOOLEAN HeadFound = FALSE;
    BOOLEAN CeilTest = FALSE;

    for (;;) {
      MaxIteration++;
      if (MaxIteration > 1024) {
        recordData->Error = NAR_ERR_MAX_ITER;
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
        recordData->Error = NAR_ERR_TRINITY;
        break;
      }

      /*
      It is unlikely, but we should check overflows
      */

      if (ClusterMapBuffer.Extents[0].Lcn.QuadPart > MAXUINT32 || ClusterMapBuffer.Extents[0].NextVcn.QuadPart > MAXUINT32 || ClusterMapBuffer.StartingVcn.QuadPart > MAXUINT32) {
        recordData->Error = NAR_ERR_OVERFLOW;
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
            recordData->P[recordData->RecCount].S = ((UINT32)ClusterMapBuffer.Extents[0].Lcn.QuadPart + OffsetFromRegionStart);//start
            recordData->P[recordData->RecCount].L = RegionLen - OffsetFromRegionStart;
            recordData->RecCount++;
            NClustersToWrite -= (RegionLen - OffsetFromRegionStart);
          }
          else {
            //Operation fits the region
            recordData->P[recordData->RecCount].S = (UINT32)ClusterMapBuffer.Extents[0].Lcn.QuadPart + OffsetFromRegionStart;
            recordData->P[recordData->RecCount].L = NClustersToWrite;
            recordData->RecCount++;
            NClustersToWrite = 0;
          }

          ClusterWriteStartOffset = (UINT32)ClusterMapBuffer.Extents[0].NextVcn.QuadPart;

          HeadFound = TRUE;

        }
        else { // HeadFound
          //Write operation falls over other region(s)
          if ((NClustersToWrite - RegionLen) > 0) {
            //write operation does not fit this region
            recordData->P[recordData->RecCount].S = (UINT32)ClusterMapBuffer.Extents[0].Lcn.QuadPart;
            recordData->P[recordData->RecCount].L = RegionLen;
            recordData->RecCount++;
            NClustersToWrite -= RegionLen;
          }
          else if (recordData->RecCount < 5) {
            /*
            Write operation fits that region
            */
            recordData->P[recordData->RecCount].S = (UINT32)ClusterMapBuffer.Extents[0].Lcn.QuadPart;
            recordData->P[recordData->RecCount].L = NClustersToWrite;
            recordData->RecCount++;
            NClustersToWrite = 0;
            break;
          }
          else {
            recordData->Error = NAR_ERR_REG_CANT_FILL;
            break;
          }

          ClusterWriteStartOffset = (UINT32)ClusterMapBuffer.Extents[0].NextVcn.QuadPart;
        }

      }

      if (NClustersToWrite <= 0) {
        break;
      }
      if (recordData->RecCount == 5) {
        break;
      }

      StartingInputVCNBuffer.StartingVcn = ClusterMapBuffer.Extents->NextVcn;

      if (status == STATUS_END_OF_FILE) {
        break;
      }

    }




#pragma warning(pop)
#endif

    KeQuerySystemTime(&recordData->OriginatingTime);
  }
}









//---------------------------------------------------------------------------
//                    Logging routines
//---------------------------------------------------------------------------

