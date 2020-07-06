/*++

Copyright (c) 1989-2002  Microsoft Corporation

Module Name:

    mspyLog.c

Abstract:

    This module contains functions used to retrieve and see the log records
    recorded by MiniSpy.sys.

Environment:

    User mode

--*/

#include <DriverSpecs.h>
_Analysis_mode_(_Analysis_code_type_user_code_)


#include <stdio.h>
#include <windows.h>
#include <stdlib.h>
#include <winioctl.h>
#include <wchar.h>

#include "mspyLog.h"
#include "minispy.h"

#define TIME_BUFFER_LENGTH 20
#define TIME_ERROR         "time error"

#define POLL_INTERVAL   5

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

BOOLEAN
TranslateFileTag(
  _In_ PLOG_RECORD logRecord
)
/*++

Routine Description:

    If this is a mount point reparse point, move the given name string to the
    correct position in the log record structure so it will be displayed
    by the common routines.

Arguments:

    logRecord - The log record to update

Return Value:

    TRUE - if this is a mount point reparse point
    FALSE - otherwise

--*/
{
  PFLT_TAG_DATA_BUFFER TagData;
  ULONG Length;

  //
  // The reparse data structure starts in the NAME field, point to it.
  //

  TagData = (PFLT_TAG_DATA_BUFFER)&logRecord->Name[0];

  //
  //  See if MOUNT POINT tag
  //

  if (TagData->FileTag == IO_REPARSE_TAG_MOUNT_POINT) {

    //
    //  calculate how much to copy
    //

    Length = min(MAX_NAME_SPACE - sizeof(UNICODE_NULL), TagData->MountPointReparseBuffer.SubstituteNameLength);

    //
    //  Position the reparse name at the proper position in the buffer.
    //  Note that we are doing an overlapped copy
    //

    MoveMemory(&logRecord->Name[0],
      TagData->MountPointReparseBuffer.PathBuffer,
      Length);

    logRecord->Name[Length / sizeof(WCHAR)] = UNICODE_NULL;
    return TRUE;
  }

  return FALSE;
}


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
  DWORD bytesReturned = 0;
  DWORD used;
  PVOID alignedBuffer[BUFFER_SIZE / sizeof(PVOID)];
  PCHAR buffer = (PCHAR)alignedBuffer;
  HRESULT hResult;
  PLOG_RECORD pLogRecord;
  PRECORD_DATA pRecordData;
  COMMAND_MESSAGE commandMessage;

  //printf("Log: Starting up\n");
  std::wstring LogDOSName = L"";
#pragma warning(push)
#pragma warning(disable:4127) // conditional expression is constant

  while (TRUE) {

#pragma warning(pop)

    //
    //  Check to see if we should shut down.
    //

    if (context->CleaningUp) {

      break;
    }

    //
    //  Request log data from MiniSpy.
    //

    commandMessage.Command = GetMiniSpyLog;

    hResult = FilterSendMessage(context->Port,
      &commandMessage,
      sizeof(COMMAND_MESSAGE),
      buffer, sizeof(alignedBuffer),
      &bytesReturned);


    if (IS_ERROR(hResult)) {

      if (HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE) == hResult) {

        printf("The kernel component of minispy has unloaded. Exiting\n");
        ExitProcess(0);
      }
      else {

        if (hResult != HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS)) {

          printf("UNEXPECTED ERROR received: %x\n", hResult);
        }

        Sleep(POLL_INTERVAL);
      }

      continue;
    }

    //
    //  Buffer is filled with a series of LOG_RECORD structures, one
    //  right after another.  Each LOG_RECORD says how long it is, so
    //  we know where the next LOG_RECORD begins.
    //

    pLogRecord = (PLOG_RECORD)buffer;
    used = 0;

    //
    //  Logic to write record to screen and/or file
    //

    for (;;) {

      if (used + FIELD_OFFSET(LOG_RECORD, Name) > bytesReturned) {

        break;
      }

      if (pLogRecord->Length < (sizeof(LOG_RECORD) + sizeof(WCHAR))) {

        printf("UNEXPECTED LOG_RECORD->Length: length=%d expected>=%d\n",
          pLogRecord->Length,
          (ULONG)(sizeof(LOG_RECORD) + sizeof(WCHAR)));

        break;
      }

      used += pLogRecord->Length;

      if (used > bytesReturned) {

        printf("UNEXPECTED LOG_RECORD size: used=%d bytesReturned=%d\n",
          used,
          bytesReturned);

        break;
      }

      pRecordData = &pLogRecord->Data;

      //
      //  See if a reparse point entry
      //

      if (FlagOn(pLogRecord->RecordType, RECORD_TYPE_FILETAG)) {

        if (!TranslateFileTag(pLogRecord)) {

          //
          // If this is a reparse point that can't be interpreted, move on.
          //

          pLogRecord = (PLOG_RECORD)Add2Ptr(pLogRecord, pLogRecord->Length);
          continue;
        }
      }


      if (pRecordData->Error != NAR_ERR_TRINITY &&
        pRecordData->RecCount != 0) { //Valid log

        /*
Check errors here
*/
        if (pLogRecord->Name[0] != L'\\') {
          continue;
        }

        int thirdindex = -1;
        int count = 0;
        LogDOSName = std::wstring(pLogRecord->Name);

        for (int i = 0; LogDOSName[i] != '\0'; i++) {
          if (LogDOSName[i] == L'\\') count++;
          if (count == 3) {
            thirdindex = i;
            break;
          }
        }
        if (thirdindex == -1) {
          printf("FATAL ERROR: Third index shouldnt be -1 \n");
          continue;
        }

        LogDOSName = LogDOSName.substr(0, thirdindex);

        for (UINT i = 0; i < context->Volumes.Count; i++) {
          volume_backup_inf* V = &context->Volumes.Data[i];

          if (V->INVALIDATEDENTRY) continue; // This entry is corrupt

          if (StrCmpW(LogDOSName.c_str(), V->DOSName) == 0) {

            if (V->FilterFlags.FlushToFile) {
              if (V->RecordsMem.size()) {
                printf("Dumping memory contents to file\n");
                HRESULT Result;
                DWORD BytesWritten = 0;
                DWORD BufferSize = V->RecordsMem.size() * sizeof(nar_record);
                DWORD SucRecCount = 0;
                Result = WriteFile(V->LogHandle, V->RecordsMem.data(), BufferSize, &BytesWritten, 0);
                if (!SUCCEEDED(Result) || BytesWritten != BufferSize) {
                  printf("Couldnt dump memory contents\n");
                  printf("written => %d\tbuffersize -> %d\n", BytesWritten, BufferSize);
                  printf("Result -> %d\n", Result);
                }
                SucRecCount = BytesWritten / sizeof(nar_record);
                V->IncRecordCount += SucRecCount;
              }
              V->FilterFlags.SaveToFile = TRUE;
              V->FilterFlags.FlushToFile = FALSE;
              V->RecordsMem.clear();
            }


            if (!V->FilterFlags.IsActive) {
              printf("Volume isnt active, breaking now\n");
              break;
            }
            if (V->FilterFlags.SaveToFile) {
              //ScreenDump(0, pLogRecord->Name, pRecordData);

              if (FileDump(pRecordData, V->LogHandle)) {
                V->IncRecordCount += pRecordData->RecCount;
                break;
              } {
                printf("## Error occured while writing log to file. FERROR!!\n");
              }

            }
            else {
              for (int k = 0; k < pRecordData->RecCount; k++) {
                V->RecordsMem.emplace_back(nar_record{ pRecordData->P[k].S, pRecordData->P[k].L });
              }
            }
          }
          else {

            printf("ERROR : %S \t %S\n", LogDOSName.c_str(), V->DOSName);

          }


        }

      }
      else {
        //printf("Err :");
        //ScreenDump(0, pLogRecord->Name, pRecordData);
        // TODO(Batuhan):
      }




      //
      //  The RecordType could also designate that we are out of memory
      //  or hit our program defined memory limit, so check for these
      //  cases.
      //

      if (FlagOn(pLogRecord->RecordType, RECORD_TYPE_FLAG_OUT_OF_MEMORY)) {

        if (context->LogToScreen) {

          printf("M:  %08X System Out of Memory\n",
            pLogRecord->SequenceNumber);
          context->DriverErrorOccured = TRUE;

        }

      }
      else if (FlagOn(pLogRecord->RecordType, RECORD_TYPE_FLAG_EXCEED_MEMORY_ALLOWANCE)) {

        printf("Exceeded Mamimum Allowed Memory Buffers! This is an fatal error!\n", pLogRecord->SequenceNumber);
        context->DriverErrorOccured = TRUE;
        break;
      }

      //
      // Move to next LOG_RECORD
      //

      pLogRecord = (PLOG_RECORD)Add2Ptr(pLogRecord, pLogRecord->Length);
    }

    //
    //  If we didn't get any data, pause for 1/2 second
    //

    if (bytesReturned == 0) {

      Sleep(POLL_INTERVAL);
    }


  }

  printf("Log: Shutting down\n");
  ReleaseSemaphore(context->ShutDown, 1, NULL);
  printf("Log: All done\n");
  return 0;
}


ULONG
FormatSystemTime(
  _In_ SYSTEMTIME* SystemTime,
  _Out_writes_bytes_(BufferLength) CHAR* Buffer,
  _In_ ULONG BufferLength
)
/*++
Routine Description:

    Formats the values in a SystemTime struct into the buffer
    passed in.  The resulting string is NULL terminated.  The format
    for the time is:
        hours:minutes:seconds:milliseconds

Arguments:

    SystemTime - the struct to format
    Buffer - the buffer to place the formatted time in
    BufferLength - the size of the buffer

Return Value:

    The length of the string returned in Buffer.

--*/
{
  ULONG returnLength = 0;

  if (BufferLength < TIME_BUFFER_LENGTH) {

    //
    // Buffer is too short so exit
    //

    return 0;
  }

  returnLength = sprintf_s(Buffer,
    BufferLength,
    "%02d:%02d:%02d:%03d",
    SystemTime->wHour,
    SystemTime->wMinute,
    SystemTime->wSecond,
    SystemTime->wMilliseconds);


  return returnLength;
}


BOOL
FileDump(
  _In_ PRECORD_DATA RecordData,
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
  BOOL Result = TRUE;
  DWORD BytesToWrite = 0;

  BytesToWrite = RecordData->RecCount * sizeof(nar_record);

  Result = WriteFile(File, &RecordData->P, BytesToWrite, &BytesWritten, 0);
  if (!SUCCEEDED(Result) || BytesWritten != BytesToWrite) {
    printf("Error occured!\n");
    printf("Bytes written -> %d, BytesToWrite -> %d\n", BytesWritten, BytesToWrite);
    printf("Result => %d\n", Result);
    Result = FALSE;
  }

  return Result;
}


VOID
ScreenDump(
  _In_ ULONG SequenceNumber,
  _In_ WCHAR CONST* Name,
  _In_ PRECORD_DATA RecordData
)
/*++
Routine Description:

    Prints a Irp log record to the screen in the following order:
    SequenceNumber, OriginatingTime, CompletionTime, IrpMajor, IrpMinor,
    Flags, IrpFlags, NoCache, Paging I/O, Synchronous, Synchronous paging,
    FileName, ReturnStatus, FileName

Arguments:

    SequenceNumber - the sequence number for this log record
    Name - the file name to which this Irp relates
    RecordData - the Irp record to print

Return Value:

    None.

--*/
{


  printf("%S\t", Name);
  if (RecordData->Error == NAR_ERR_TRINITY) {
    printf("Holy Trinity error! ");
  }
  if (RecordData->Error == NAR_ERR_REG_OVERFLOW) {
    printf("REG OVERFLOW!!! error ! ");
  }
  if (RecordData->Error == NAR_ERR_REG_CANT_FILL) {
    printf("CANT_FILL error! ");
  }
  if (RecordData->Error == NAR_ERR_ALIGN) {
    printf("Align error! ");
  }
  if (RecordData->Error == NAR_ERR_MAX_ITER) {
    printf("MAX_ITER ERROR! ");
  }
  if (RecordData->Error == NAR_ERR_OVERFLOW) {
    printf("OVERFLOW ERROR ");
  }
  {
    for (int i = 0; i < RecordData->RecCount; i++) {
      printf("%d\t\%d\t", RecordData->P[i].S, RecordData->P[i].L);
    }
    printf("\n");
  }

}

