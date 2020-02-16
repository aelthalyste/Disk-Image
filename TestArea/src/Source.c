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

DWORD LogRecords(LPVOID Parameters);

typedef struct _nar_record {
	ULONGLONG Start; //start of the operation in context of LCN
	ULONGLONG OperationSize; // End of operation, in LCN //TODO change variable name
}nar_record; nar_record GlobalNarFsRecord;

typedef struct _nar_log { // Align 64byte? 
	UINT32 Count; //leftmost bit is used for overflow flag
	nar_record Record[64];
}nar_log; nar_log GlobalFileLog;

inline void
PrintLastError() {
	DWORD Err = GetLastError();
	printf("Last error was %d\n", Err);
}

BOOL GlobalShouldCleanup = FALSE;
HANDLE GlobalSemaphore;

int _cdecl
main(_In_ int argc,
	_In_reads_(argc) char *argv[]
) {
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

	GlobalSemaphore = CreateSemaphore(
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
		Sleep(100);
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

	
	DWORD OutBufferSize = 8 * 1024;
	LPVOID OutBuffer = malloc(OutBufferSize);
	DWORD BytesReturned = 0;

	nar_log* Data = (nar_log*)OutBuffer;

	for (;;) {
		Result = FilterSendMessage(Port, Port, sizeof(PVOID), OutBuffer, OutBufferSize, &BytesReturned);
		
		if (!SUCCEEDED(Result)) {
			printf("Failed filtermessage function -> %d",Result);
			break;
		}
		
		if (BytesReturned > 0) {
			if (Data->Count != 0) {
				for (int i = 0; i < Data->Count; i++) {
					if (Data->Record[i].OperationSize == 920) {
						printf("Errorcode -> %p\n", (LPVOID)Data->Record[i].Start);
					}
				}
				//printf("%d\t%I64d\t%I64d (0th Element)\n", Data->Count, Data->Record[0].Start, Data->Record[0].OperationSize);
			}
		}
		else {
			printf("Bytes returned was zero \n", Data->Count);
		}

		if (GlobalShouldCleanup) {
			break;
		}
		Sleep(125);
	
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