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





int
main() {

	nar_record *R = DEBUGReadEntireFile("OutputFile.txt");
	nar_record StackR[5664 / sizeof(nar_record)] = { 0 };
	int a = 0;
	for (int i = 0; i < (5664 / sizeof(nar_record)); i++) {
		StackR[i] = R[i];
		if (StackR[i].Type == NarError) {
			printf("ErrIndex -> %d, ErrVal-> %I64d\n", i, StackR->Err);
		}
	}
	

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
		L"C:",
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

	DWORD OutBufferSize = 16 * 1024;
	nar_log* Data = (nar_log*)malloc(OutBufferSize);
	DWORD BytesReturned = 0;

	UINT32 MaxEntryCount = 256;
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
		
		if (BytesReturned > 0 && Data->Count > 0) {
			printf("\r%d                       ", CurrentEntryCount);
			BOOL Result = FALSE;

			if (Data->Count + CurrentEntryCount > MaxEntryCount) {
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

			for (UINT32 Indx = 0; Indx < Data->Count; Indx++) {
				AllRecords[CurrentEntryCount].Start = Data->Record[Indx].Start;
				AllRecords[CurrentEntryCount].OperationSize = Data->Record[Indx].OperationSize;
				AllRecords[CurrentEntryCount].Type = Data->Record[Indx].OperationSize;
				CurrentEntryCount++;
			}
		}
		else {
			printf("\rBytes returned was zero -- %d",CurrentEntryCount);
		}
		
		if (GlobalShouldCleanup) {
			break;
		}

		Sleep(50);
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