#include <Windows.h>
#include <winioctl.h>
#include <ntddvol.h>


#if 1
#define ASSERT(expression) if(!(expression)) {*(int*)0=0;}
#define Assert(expression) ASSERT(expression)
#define ASSERT_VSS(expression) if(!(expression)) {printf("Err @ %d",__LINE__);*(int*)0=0; }
#else //WUMBO_SLOW 
#define ASSERT(expression)
#endif //WUMBO_SLOW

#include <DriverSpecs.h>

#include <stdio.h>
#include <fltUser.h>
#include <stdlib.h>
#include <windows.h>
#include <assert.h>

#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <vsmgmt.h>

// VDS includes
#include <vds.h>

// ATL includes
#include <atlbase.h>


#include <fileapi.h>

#include <string>

#define PORT_NAME L"\\NarMiniFilterPort"
#define FILTER_NAME L"NarMinifilterDriver"

#include "NarMFVars.h"

DWORD LogRecords(LPVOID Parameters);

int CompareRecords(const void * R1, const void * R2) {
	int Result = 0;
	nar_record * T1 = (nar_record*)R1;
	nar_record* T2 = (nar_record*)R2;

	Result == T1->OperationSize > T2->OperationSize;
	if (T1->Start == T2->Start) {
		Result = T1->OperationSize > T2->OperationSize;
	}
	return Result;
}


bool
DEBUGWriteEntireFile(const char* Filename, UINT32 BufferSize, void* Buffer) {
	HANDLE Filehandle = CreateFileA(Filename,
		GENERIC_WRITE,
		0,
		0,
		CREATE_ALWAYS,
		0,
		0
	);
	ASSERT(Filehandle != INVALID_HANDLE_VALUE);
	bool Result = FALSE;

	if (Filehandle != INVALID_HANDLE_VALUE) {
		DWORD BytesWritten = 0;

		if (WriteFile(Filehandle, Buffer, BufferSize, &BytesWritten, 0)
			&& (BytesWritten == BufferSize)) {
			Result = true;
			// NOTE(Batuhan G.): Success
		}
		else {
			// TODO(Batuhan G.): Log
		}

		CloseHandle(Filehandle);
	}
	else {
		// TODO(Batuhan G.): Log
	}

	return Result;
}



struct thread_parameters {
	HANDLE ConnectionPort;
	nar_record *Data;
	UINT32 DataCount;
	UINT32 MaxDataCount;
};

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

	char* ptr = (char*)malloc(1024);
	GetVolumeInformationA("C:", ptr, 1024, 0, 0, 0, 0, 0);
	
	printf("%s", ptr);

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