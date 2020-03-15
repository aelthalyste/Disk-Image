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

#define TIME_BUFFER_LENGTH 20
#define TIME_ERROR         "time error"

#define POLL_INTERVAL   10     // 10 milliseconds

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
			buffer,
			sizeof(alignedBuffer),
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

			if (context->LogToScreen && pRecordData->Arg5 != NAR_ERR_TRINITY && context->ShouldFilter) {
				if (pRecordData->Arg5 != 0) {
					printf("Err incomin ");
				}
				printf("Hell");
				ScreenDump(pLogRecord->SequenceNumber,
					pLogRecord->Name,
					pRecordData);
			}

			if (context->LogToFile
				&& pRecordData->Arg5 == 0
				&& context->ShouldFilter
				&& pRecordData->Arg1 != 0) {

				BOOL r = FileDump(pLogRecord->SequenceNumber,
					pLogRecord->Name,
					pRecordData,
					context->OutputFile);
				if (r == FALSE) {
					printf("Error occured while writing new record to the file. \nPlease restart the program\n");
					break;
				}
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
				}

			}
			else if (FlagOn(pLogRecord->RecordType, RECORD_TYPE_FLAG_EXCEED_MEMORY_ALLOWANCE)) {

				printf("Exceeded Mamimum Allowed Memory Buffers! This is an fatal error!\n", pLogRecord->SequenceNumber);

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
	_In_ ULONG SequenceNumber,
	_In_ WCHAR CONST* Name,
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

		SequenceNumber - the sequence number for this log record
		Name - the name of the file that this Irp relates to
		RecordData - the Data record to print
		File - the file to print to

Return Value:

		None.

--*/
{
	DWORD BytesWritten = 0;
	BOOL Result = 0;
	DWORD BytesToWrite = sizeof(ULONGLONG) * 2;
	if (RecordData->Arg3 != 0 && RecordData->Arg4 != 0) {
		BytesToWrite = 4 * sizeof(ULONGLONG);
	}
	Result = WriteFile(File, &RecordData->Arg1, BytesToWrite, &BytesWritten, 0);
	if (!SUCCEEDED(Result) || BytesWritten != BytesToWrite) {
		printf("Error occured!\n");
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
	//
	//  Display informatoin
	//
	/*
	if (RecordData->Flags & FLT_CALLBACK_DATA_IRP_OPERATION) {

			printf( "IRP ");

	} else if (RecordData->Flags & FLT_CALLBACK_DATA_FAST_IO_OPERATION) {

			printf( "FIO ");

	} else if (RecordData->Flags & FLT_CALLBACK_DATA_FS_FILTER_OPERATION) {

			printf( "FSF " );
	} else {

			printf( "ERR ");
	}
	printf( "%08X ", SequenceNumber );

	*/

	if (RecordData->CallbackMajorId == IRP_MJ_WRITE) {

		printf("%S\t", Name);
		if (RecordData->Arg5 == NAR_ERR_TRINITY) {
			printf("Holy Trinity error!\n");
		}
		else if (RecordData->Arg5 == NAR_ERR_REG_OVERFLOW) {
			printf("REG OVERFLOW!!! error ! \n");
		}
		else if (RecordData->Arg5 == NAR_ERR_REG_CANT_FILL) {
			printf("CANT_FILL error!\n");
		}
		else if (RecordData->Arg5 == NAR_ERR_ALIGN) {
			printf("Align error!\n");
		}
		else if (RecordData->Arg5 == NAR_ERR_MAX_ITER) {
			printf("MAX_ITER ERROR!\n");
		}
		else {
			printf("%I64u\t", RecordData->Arg1);
			printf("%I64u\t", RecordData->Arg2);

			printf("%I64u\t", RecordData->Arg3);
			printf("%I64u\t", RecordData->Arg4);
		}

		printf("\n");
	}
}

