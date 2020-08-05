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

#define POLL_INTERVAL   5000 // ms

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
    HRESULT hResult;

    NAR_COMMAND commandMessage;

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
        static WCHAR VolumeGUIDStr[50];
        memset(wStrBuffer, 0, WSTRBUFFERLEN * sizeof(WCHAR));
        memset(VolumeGUIDStr, 0, USERGUIDSTRLEN * sizeof(WCHAR));

        for (int VolumeIndex = 0; VolumeIndex < context->Volumes.Count; VolumeIndex++) {


            volume_backup_inf* V = &context->Volumes.Data[VolumeIndex];

            if (V == NULL || V->INVALIDATEDENTRY)
            {
                continue;
            }

            wsprintfW(wStrBuffer, L"%c:\\", V->Letter);
            BOOLEAN Result = GetVolumeNameForVolumeMountPointW(wStrBuffer, VolumeGUIDStr, USERGUIDSTRLEN);

            if (Result != 0) {
                VolumeGUIDStr[1] = L'?';

                DWORD Hell = 0;
                NAR_COMMAND Command;
                memset(&Command, 0, sizeof(NAR_COMMAND));

                memset(OutBuffer, 0, NAR_MEMORYBUFFER_SIZE);
                Command.Type = NarCommandType_GetVolumeLog;
                memcpy(Command.VolumeGUIDStr, VolumeGUIDStr, USERGUIDSTRLEN * sizeof(WCHAR));
                hResult = FilterSendMessage(context->Port, &Command, sizeof(NAR_COMMAND), OutBuffer, NAR_MEMORYBUFFER_SIZE, &Hell);

                if (SUCCEEDED(hResult)) {
                    printf("For volume %c message sent successfully\n", (char)V->Letter);

                    if (!NAR_MB_ERROR_OCCURED(OutBuffer)) {

                        // if volume isnt active, just fetch all data and dump it, thats it
                        if (!V->FilterFlags.IsActive) {
                            continue;
                        }

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
                            continue;
                        }



                        if (!V->FilterFlags.IsActive) {
                            //printf("Volume isnt active, breaking now\n");
                            continue;
                        }
                        if (V->FilterFlags.SaveToFile) {
                            //ScreenDump(0, pLogRecord->Name, pRecordData);
                            INT32 DataUsed = NAR_MB_DATA_USED(OutBuffer);

                            if (DataUsed > 0 && DataUsed < NAR_MEMORYBUFFER_SIZE - 2 * sizeof(INT32)) {

                                if (FileDump(NAR_MB_DATA(OutBuffer), DataUsed, V->LogHandle)) {
                                    V->IncRecordCount += NAR_MB_DATA_USED(OutBuffer) / sizeof(nar_record);
                                }
                                else {
                                    printf("## Error occured while writing log to file. FERROR!!\n");
                                }

                            }
                            else {

                                if (DataUsed < 0) {
                                    printf("Data used was lower than zero %i\n", DataUsed);
                                }
                                if (DataUsed > NAR_MEMORYBUFFER_SIZE - 2 * sizeof(INT32)) {
                                    printf("Data size exceeded buffer size itself, this is a fatal error(size = %i)\n", DataUsed);
                                }

                            }

                            continue;

                        }
                        else {

                            INT32 RecCount = NAR_MB_DATA_USED(OutBuffer) / sizeof(nar_record);
                            nar_record* Recs = (nar_record*)NAR_MB_DATA(OutBuffer);

                            for (int RecordIndex = 0; RecordIndex < RecCount; RecordIndex++) {
                                V->RecordsMem.emplace_back(Recs[RecordIndex]);
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
    BOOL Result = TRUE;

    Result = WriteFile(File, Data, DataSize, &BytesWritten, 0);
    if (!SUCCEEDED(Result) || BytesWritten != DataSize) {
        printf("Error occured!\n");
        printf("Bytes written -> %d, BytesToWrite -> %d\n", BytesWritten, DataSize);
        DisplayError(GetLastError());
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

