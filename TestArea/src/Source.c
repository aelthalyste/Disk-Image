#include <Windows.h>
#include <winioctl.h>
#include <ntddvol.h>

#if _DEBUG
#define ASSERT(expression) if(!(expression)) {*(int*)0=0;}
#define Assert(expression) ASSERT(expression)
#else //WUMBO_SLOW 
#define ASSERT(expression)
#endif //WUMBO_SLOW

#include <DriverSpecs.h>

#include <stdio.h>
#include <fltUser.h>
#include <stdlib.h>
#include <windows.h>
#include <assert.h>

#define PORT_NAME L"\\NarMiniFilterPort"
#define FILTER_NAME L"NarMinifilterDriver"

#include "NarMFVars.h"

DWORD LogRecords(LPVOID Parameters);


inline void
PrintLastError() {
	DWORD Err = GetLastError();
	printf("Last error was %d\n", Err);
}

BOOL GlobalShouldCleanup = FALSE;
HANDLE GlobalSemaphore;

LPVOID
DEBUGReadEntireFile(char* Filename) {

	HANDLE Filehandle = CreateFileA(Filename,
		GENERIC_READ,
		FILE_SHARE_READ,
		0,
		OPEN_EXISTING,
		0,
		0);

	LPVOID Result = 0;
	ASSERT(Filehandle != INVALID_HANDLE_VALUE);

	if (Filehandle != INVALID_HANDLE_VALUE) {

		LARGE_INTEGER FileSize = { 0 };

		if (GetFileSizeEx(Filehandle, &FileSize)) {

			DWORD BytesRead = 0;

			Result = VirtualAlloc(0, FileSize.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

			if (ReadFile(Filehandle, Result, FileSize.QuadPart, &BytesRead, NULL)
				&& (BytesRead == FileSize.QuadPart)) {


				// NOTE(Batuhan G.): File read successfully;

			}
		}
		else {// Couldn't get filesize
				// TODO(Batuhan G.): Log
		}
		CloseHandle(Filehandle);
	}
	else { //Couldn't open file
			// TODO(Batuhan G.): Log
	}

	return Result;
}



void ListDevices();

ULONG
IsAttachedToVolume(
	LPCWSTR VolumeName
);

int
main() {

	HANDLE ConnectionPort = INVALID_HANDLE_VALUE;
	HRESULT Result = 0;
	WCHAR instanceName[INSTANCE_NAME_MAX_CHARS + 1];
	HANDLE Thread = 0;
	ULONG ThreadID = 0;

	printf("User mode started!\n");

	Result = FilterConnectCommunicationPort(
		PORT_NAME,
		0,
		NULL,
		0,
		NULL,
		&ConnectionPort
	);
	if (IS_ERROR(Result)) {
		printf("FilterConnectCommunicationPort failed! %d\n", Result);
		PrintLastError();
		goto MainCleanup;
	}
	
	
	Result = FilterAttach(
		FILTER_NAME,
		L"E:",
		NULL,
		sizeof(instanceName),
		instanceName
	);
	if (!SUCCEEDED(Result)) {
		if (Result == ERROR_FLT_INSTANCE_ALTITUDE_COLLISION) {
			printf("ERROR_FLT_INSTANCE_ALTITUDE_COLLISION\n", Result);
			printf("FilterAttach failed! -> %d\n", Result);
			PrintLastError();
			goto MainCleanup;
		}
		else if (Result == ERROR_FLT_INSTANCE_NAME_COLLISION) {
			printf("ERROR_FLT_INSTANCE_NAME_COLLISION\n", Result);
		}
		else if (Result == ERROR_FILE_NOT_FOUND) {
			printf("ERROR_FILE_NOT_FOUND\n", Result);
			printf("FilterAttach failed! -> %d\n", Result);
			PrintLastError();
			goto MainCleanup;
		}
	}

	ListDevices();
	FilterDetach(
		FILTER_NAME,
		"C:\\",
		NULL
	);

	printf("Instance name -> %S\n", instanceName);

	ListDevices();

	GlobalSemaphore = CreateSemaphoreW(
		NULL,
		0,
		1,
		L"NarFilter"
	);
	if (GlobalSemaphore == NULL) {
		printf("Couldn't initialize semaphore!\n");
		goto MainCleanup;
	}

	Thread = CreateThread(
		NULL,
		0,
		LogRecords,
		ConnectionPort, //Parameters to pass
		0,
		&ThreadID
	);
	if (Thread == INVALID_HANDLE_VALUE) {
		printf("Thread creation failed\n");
		PrintLastError();
		goto MainCleanup;
	}

	CHAR inputChar;
	while (inputChar = (CHAR)getchar()) {
		if (inputChar == 'q' || inputChar == 'Q') {
			goto MainCleanup;
		}
	}


MainCleanup:
	printf("Cleaning up..\n");
	//TODO clean things
	GlobalShouldCleanup = TRUE;

	if (GlobalSemaphore) {
		WaitForSingleObject(GlobalSemaphore, INFINITE);
		CloseHandle(GlobalSemaphore);
	}

	//Clean thread
	if (Thread) {
		CloseHandle(Thread);
	}

	if (ConnectionPort != INVALID_HANDLE_VALUE) {
		CloseHandle(ConnectionPort);
	}


	return 0;
}


DWORD
LogRecords(LPVOID Parameters) {

	HANDLE Port = Parameters;
	HRESULT Result = 0;

	DWORD OutBufferSize = sizeof(nar_record) * 128;
	nar_record* Data = (nar_log*)malloc(OutBufferSize);
	DWORD BytesReturned = 0;

	UINT32 MaxEntryCount = 1024*4;
	UINT32 CurrentEntryCount = 0;
	nar_record* AllRecords = (nar_record*)malloc(sizeof(nar_record) * MaxEntryCount);
	
	HANDLE OutputFile = CreateFile(
		"OutputFile.txt",
		GENERIC_WRITE,
		FILE_SHARE_READ,
		0,
		CREATE_ALWAYS,
		0,
		0
	);
	if (OutputFile == INVALID_HANDLE_VALUE) {
		printf("Couldn't create log file.. Terminating thread!");
		goto Cleanup;
	}

	for (;;) {
		memset(Data, 0, OutBufferSize);

		Result = FilterSendMessage(Port, Port, sizeof(PVOID), Data, OutBufferSize, &BytesReturned);
		
		if (!SUCCEEDED(Result)) {
			printf("Failed filtermessage function -> %d",Result);
			break;
		}
		
		if (BytesReturned > 0) {
			BOOL Result = FALSE;
			UINT32 Count = BytesReturned / sizeof(nar_record);

			if (Count + CurrentEntryCount > MaxEntryCount) {
				DWORD BytesWritten = 0;
				Result = WriteFile(OutputFile, AllRecords, sizeof(nar_record) * CurrentEntryCount, BytesWritten, 0);
				if (Result != TRUE) {
					printf("WriteFile error!\n");
					PrintLastError();
					goto Cleanup;
				}
				printf("Flushed all logs. Terminating thread...\n");
				goto Cleanup;
			}

			for (UINT32 Indx = 0; Indx < Count; Indx++) {
				AllRecords[CurrentEntryCount].Start = Data[Indx].Start;
				AllRecords[CurrentEntryCount].OperationSize = Data[Indx].OperationSize;
				AllRecords[CurrentEntryCount].Type = Data[Indx].Type;

				if (Data[Indx].Type == NarError) {
					if (Data[Indx].Err == NE_REGION_OVERFLOW) {
						printf("Region overflow! ");
					}
					if (Data[Indx].Err == NE_BREAK_WITHOUT_LOG) {
						printf("NE_BREAK_WITHOUT_LOG ");
						printf("Name-> %S ", Data[Indx].Name);
					}
					if (Data[Indx].Err == 0) {
						printf("SUCCESS CALLBACK! ");
					}
					if (Data[Indx].Err == NE_ANSI_UNICODE_CONVERSION) {

					}
					if (Data[Indx].Err == NE_GETFILENAMEINF_FUNC_FAILED) {

					}
					if (Data[Indx].Err == NE_PAGEFILE_FOUND) {

					}

					if (Data[Indx].Err == NE_UNDEFINED) {
						printf("Undefined error-> %I64d \n", Data[Indx].Reserved);
						printf("Name-> %S ", Data[Indx].Name);
					}
					
					
					printf("\n");
				}

				//else {
				//	printf("--> %I64d\t%I64d\n", Data[Indx].Start, Data[Indx].OperationSize);
				//}
				
				CurrentEntryCount++;
			}
		}

		
		if (GlobalShouldCleanup) {
			break;
		}

		Sleep(25);
	}
	
Cleanup:
	CloseHandle(OutputFile);
	
	if (Data) {
		free(Data);
	}
	if (AllRecords) {
		free(AllRecords);
	}

	ReleaseSemaphore(GlobalSemaphore, 1, NULL);
	printf("\nExiting logrecords thread..\n");

	return 0;
}

ULONG
IsAttachedToVolume(
	_In_ LPCWSTR VolumeName
)
/*++
Routine Description:
		Determine if our filter is attached to this volume
Arguments:
		VolumeName - The volume we are checking
Return Value:
		TRUE - we are attached
		FALSE - we are not attached (or we couldn't tell)
--*/
{
	PWCHAR filtername;
	CHAR buffer[1024];
	PINSTANCE_FULL_INFORMATION data = (PINSTANCE_FULL_INFORMATION)buffer;
	HANDLE volumeIterator = INVALID_HANDLE_VALUE;
	ULONG bytesReturned;
	ULONG instanceCount = 0;
	HRESULT hResult;

	//
	//  Enumerate all instances on this volume
	//

	hResult = FilterVolumeInstanceFindFirst(VolumeName,
		InstanceFullInformation,
		data,
		sizeof(buffer) - sizeof(WCHAR),
		&bytesReturned,
		&volumeIterator);

	if (IS_ERROR(hResult)) {

		return instanceCount;
	}

	do {

		assert((data->FilterNameBufferOffset + data->FilterNameLength) <= (sizeof(buffer) - sizeof(WCHAR)));
		_Analysis_assume_((data->FilterNameBufferOffset + data->FilterNameLength) <= (sizeof(buffer) - sizeof(WCHAR)));

		//
		//  Get the name.  Note that we are NULL terminating the buffer
		//  in place.  We can do this because we don't care about the other
		//  information and we have guaranteed that there is room for a NULL
		//  at the end of the buffer.
		//

#define Add2Ptr(P,I) ((PVOID)((PUCHAR)(P) + (I)))
		filtername = Add2Ptr(data, data->FilterNameBufferOffset);
		filtername[data->FilterNameLength / sizeof(WCHAR)] = L'\0';

		//
		//  Bump the instance count when we find a match
		//

		if (_wcsicmp(filtername, FILTER_NAME) == 0) {

			instanceCount++;
		}

	} while (SUCCEEDED(FilterVolumeInstanceFindNext(volumeIterator,
		InstanceFullInformation,
		data,
		sizeof(buffer) - sizeof(WCHAR),
		&bytesReturned)));

	//
	//  Close the handle
	//

	FilterVolumeInstanceFindClose(volumeIterator);
	return instanceCount;
}

void
ListDevices(
	VOID
)
/*++
Routine Description:
		Display the volumes we are attached to
Arguments:
Return Value:
--*/
{
	UCHAR buffer[1024];
	PFILTER_VOLUME_BASIC_INFORMATION volumeBuffer = (PFILTER_VOLUME_BASIC_INFORMATION)buffer;
	HANDLE volumeIterator = INVALID_HANDLE_VALUE;
	ULONG volumeBytesReturned;
	HRESULT hResult = S_OK;
	WCHAR driveLetter[15] = { 0 };
	ULONG instanceCount;

	__try {

		//
		//  Find out size of buffer needed
		//

		hResult = FilterVolumeFindFirst(FilterVolumeBasicInformation,
			volumeBuffer,
			sizeof(buffer) - sizeof(WCHAR),   //save space to null terminate name
			&volumeBytesReturned,
			&volumeIterator);

		if (IS_ERROR(hResult)) {

			__leave;
		}

		assert(INVALID_HANDLE_VALUE != volumeIterator);

		//
		//  Output the header
		//

		printf("\n"
			"Dos Name        Volume Name                            Status \n"
			"--------------  ------------------------------------  --------\n");

		//
		//  Loop through all of the filters, displaying instance information
		//

		do {

			assert((FIELD_OFFSET(FILTER_VOLUME_BASIC_INFORMATION, FilterVolumeName) + volumeBuffer->FilterVolumeNameLength) <= (sizeof(buffer) - sizeof(WCHAR)));
			_Analysis_assume_((FIELD_OFFSET(FILTER_VOLUME_BASIC_INFORMATION, FilterVolumeName) + volumeBuffer->FilterVolumeNameLength) <= (sizeof(buffer) - sizeof(WCHAR)));

			volumeBuffer->FilterVolumeName[volumeBuffer->FilterVolumeNameLength / sizeof(WCHAR)] = UNICODE_NULL;

			instanceCount = IsAttachedToVolume(volumeBuffer->FilterVolumeName);

			printf("%-14ws  %-36ws  %s",
				(SUCCEEDED(FilterGetDosName(
					volumeBuffer->FilterVolumeName,
					driveLetter,
					sizeof(driveLetter) / sizeof(WCHAR))) ? driveLetter : L""),
				volumeBuffer->FilterVolumeName,
				(instanceCount > 0) ? "Attached" : "");

			if (instanceCount > 1) {

				printf(" (%d)\n", instanceCount);

			}
			else {

				printf("\n");
			}

		} while (SUCCEEDED(hResult = FilterVolumeFindNext(volumeIterator,
			FilterVolumeBasicInformation,
			volumeBuffer,
			sizeof(buffer) - sizeof(WCHAR),    //save space to null terminate name
			&volumeBytesReturned)));

		if (HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS) == hResult) {

			hResult = S_OK;
		}

	}
	__finally {

		if (INVALID_HANDLE_VALUE != volumeIterator) {

			FilterVolumeFindClose(volumeIterator);
		}

		if (IS_ERROR(hResult)) {

			if (HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS) == hResult) {

				printf("No volumes found.\n");

			}
			else {

				printf("Volume listing failed with error: 0x%08x\n",
					hResult);
			}
		}
	}
}

/*
	HANDLE Filehandle = CreateFileA(
		"N:\\test - Copy.txt",
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		NULL,
		NULL
	);

	ASSERT(Filehandle != INVALID_HANDLE_VALUE);

	DWORD BytesReturned = 0;
	DWORD BufferSize = 1024;

	BOOL Result = FALSE;

	STARTING_VCN_INPUT_BUFFER StartingInputVCNBuffer;
	PRETRIEVAL_POINTERS_BUFFER PointersBuffer = (PRETRIEVAL_POINTERS_BUFFER)malloc(BufferSize);
	VOLUME_LOGICAL_OFFSET VolumeLogicalOffset;

	StartingInputVCNBuffer.StartingVcn.QuadPart = 0;

	Result = DeviceIoControl(
		Filehandle,
		FSCTL_GET_RETRIEVAL_POINTERS,
		&StartingInputVCNBuffer,
		sizeof(StartingInputVCNBuffer),
		PointersBuffer,
		BufferSize,
		&BytesReturned,
		NULL
	);
	ASSERT(Result == TRUE);

	Result = DeviceIoControl(
		Filehandle,
		FSCTL_GET_RETRIEVAL_POINTERS,
		&StartingInputVCNBuffer,
		sizeof(StartingInputVCNBuffer),
		PointersBuffer,
		BufferSize,
		&BytesReturned,
		NULL
	);
	ASSERT(Result == TRUE);


	DWORD Dump = 0;
	DWORD ClusterSize = 0;
	DWORD SecPerCluster = 0;
	DWORD BytesPerSector = 0;

	GetDiskFreeSpace("N:\\", &SecPerCluster, &BytesPerSector, &Dump, &Dump);
	ClusterSize = BytesPerSector * SecPerCluster;

	//GetShadowPath("")
	HANDLE VolumeHandle = CreateFile(
		"\\\\.\\N:",
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		NULL,
		NULL
	);

	ASSERT(VolumeHandle != INVALID_HANDLE_VALUE);

	{
		DWORD Transferred = 0;
		DWORD FileBufferSize = 1024;
		void* FileBuffer = malloc(FileBufferSize);
		LONGLONG Val = PointersBuffer->Extents[0].Lcn.QuadPart * ClusterSize;
		DWORD SeekResult = 0;
#if 1
		//Pure volume read
		SeekResult = SetFilePointer(VolumeHandle, Val, 0, FILE_BEGIN);
		ASSERT(SeekResult == Val);

		Result = ReadFile(VolumeHandle, FileBuffer, FileBufferSize, &Transferred, NULL);
		ASSERT(Result == TRUE);
		ASSERT(Transferred == FileBufferSize);
#endif

#if 1
		Val = 0;
		SeekResult = SetFilePointer(Filehandle, Val, 0, FILE_BEGIN);
		ASSERT(SeekResult == Val);

		Result = ReadFile(Filehandle, FileBuffer, FileBufferSize, &Transferred, NULL);
		ASSERT(Result == TRUE);
		std::cout << GetLastError() << "\n";
		ASSERT(Transferred == FileBufferSize);
#endif

	}
*/