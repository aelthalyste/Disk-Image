/*++

Copyright (c) 1989-2002  Microsoft Corporation

Module Name:

		mspyUser.c

Abstract:

		This file contains the implementation for the main function of the
		user application piece of MiniSpy.  This function is responsible for
		controlling the command mode available to the user to control the
		kernel mode driver.

Environment:

		User mode

--*/

#include <DriverSpecs.h>
_Analysis_mode_(_Analysis_code_type_user_code_)

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <assert.h>
#include "mspyLog.h"
#include <strsafe.h>
#include <functional>
#include <winioctl.h>

#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <vsmgmt.h>

// VDS includes
#include <vds.h>

// ATL includes
#include <atlbase.h>
#include <string>
#include <vector>
#include <list>

#define SUCCESS              0
#define USAGE_ERROR          1
#define EXIT_INTERPRETER     2
#define EXIT_PROGRAM         4

#define INTERPRETER_EXIT_COMMAND1 "go"
#define INTERPRETER_EXIT_COMMAND2 "g"
#define PROGRAM_EXIT_COMMAND      "exit"
#define CMDLINE_SIZE              256
#define NUM_PARAMS                40

#define CLEANHANDLE(handle) if((handle)!=NULL) CloseHandle(handle);
#define CLEANMEMORY(memory) if((memory)!=NULL) free(memory);

#define MINISPY_NAME            L"MiniSpy"

#define FB_FILE_NAME "FullBackupDisk.nar"
#define DB_FILE_NAME "DiffBackupDisk.nar"

#define FB_METADATA_FILE_NAME "FMetadata"
#define DB_METADATA_FILE_NAME "DMetadata"

#define DB_MFT_FILE_NAME "MFTDATA"

#define Assert(expression) if(!(expression)) {*(int*)0 = 0;}
#define ASSERT_VSS(expression) if(FAILED(expression)) {printf("Err @ %d\n",__LINE__);*(int*)0=0; }

VOID
DisplayError(
	_In_ DWORD Code
);

BOOL CompareNarRecords(const void* v1,const void *v2) {
	nar_record* n1 = (nar_record*)v1;
	nar_record* n2 = (nar_record*)v2;
	if (n1->StartPos == n2->StartPos) {
		return n1->Len < n2->Len;
	}
	return n1->StartPos < n2->StartPos;
}

std::wstring
GetShadowPath(std::wstring Drive, CComPtr<IVssBackupComponents>& ptr) {
	VSS_ID sid;
	HRESULT res;

	res = CreateVssBackupComponents(&ptr);

	res = ptr->InitializeForBackup();
	ASSERT_VSS(res == S_OK);
	res = ptr->SetContext(VSS_CTX_BACKUP);
	ASSERT_VSS(res == S_OK);
	res = ptr->StartSnapshotSet(&sid);
	ASSERT_VSS(res == S_OK);
	res = ptr->SetBackupState(false, false, VSS_BACKUP_TYPE::VSS_BT_FULL, false);
	ASSERT_VSS(res == S_OK);
	res = ptr->AddToSnapshotSet((LPWSTR)Drive.c_str(), GUID_NULL, &sid); // C:\\ ex
	ASSERT_VSS(res == S_OK);

	{
		CComPtr<IVssAsync> Async;
		res = ptr->PrepareForBackup(&Async);
		ASSERT_VSS(res == S_OK);
		Async->Wait();
	}

	{
		CComPtr<IVssAsync> Async;
		res = ptr->DoSnapshotSet(&Async);
		ASSERT_VSS(res == S_OK);
		Async->Wait();
	}

	VSS_SNAPSHOT_PROP SnapshotProp;
	ptr->GetSnapshotProperties(sid, &SnapshotProp);
	std::wstring ShadowPath = std::wstring(SnapshotProp.m_pwszSnapshotDeviceObject);

	return ShadowPath;
}

VOID
NarCloseThreadCom(
	PLOG_CONTEXT Context,
	HANDLE* Thread
) {
	Assert(Thread != NULL);
	Assert(*Thread != NULL);

	Context->CleaningUp = TRUE;
	WaitForSingleObject(Context->ShutDown, INFINITE);
	
	CloseHandle(Context->ShutDown);
	CloseHandle(Context->OutputFile);
	CloseHandle(*Thread);

	Context->ShutDown = NULL;
}

BOOL
NarCreateThreadCom(
	PLOG_CONTEXT Context,
	HANDLE* Thread
) {
	BOOL Result = TRUE;
	Assert(Thread != NULL);
	Assert((*Thread) == NULL);
	Assert(Context->ShutDown == NULL);

	Context->CleaningUp = FALSE;
	Context->ShouldFilter = FALSE;
	Context->LogToFile = TRUE;

	Context->ShutDown = CreateSemaphoreW(NULL,
		0,
		1,
		L"MiniSpy shut down"
	);
	if (Context->ShutDown != NULL) {	
		
		Context->OutputFile = CreateFileA(DB_METADATA_FILE_NAME,
			GENERIC_WRITE,
			0,
			0,
			CREATE_ALWAYS,
			0,
			0
		);
		
		if (Context->OutputFile != INVALID_HANDLE_VALUE) {
			
			*Thread = CreateThread(NULL,
				0,
				RetrieveLogRecords,
				(LPVOID)Context,
				0,
				0);
			if (*Thread == NULL) {
				Result = FALSE; //Unable to create thread
			}
		
		}
		else {//Unable to create file
			printf("Couldn't create logging file!\n");
			DisplayError(GetLastError());
			Result = FALSE;
		}

	}
	else {
		//Unable to create semaphore
		printf("Could not create semaphore for thread..\n");
		DisplayError(GetLastError());
		Result = FALSE;
	}
	
	return Result;
}

//
//  Main uses a loop which has an assignment in the while 
//  conditional statement. Suppress the compiler's warning.
//

#pragma warning(push)
#pragma warning(disable:4706) // assignment within conditional expression

int _cdecl
main(
	_In_ int argc,
	_In_reads_(argc) char* argv[]
){
	HANDLE port = INVALID_HANDLE_VALUE;
	HRESULT hResult = S_OK;
	DWORD result;
	ULONG threadId;
	HANDLE thread = NULL;
	LOG_CONTEXT context;
	CHAR inputChar;

	
	hResult = CoInitialize(NULL);
	if (!SUCCEEDED(hResult)) {
		printf("Failed CoInitialize function \n");
		DisplayError(GetLastError());
		goto Main_Exit;
	}
	hResult = CoInitializeSecurity(
		NULL,                           //  Allow *all* VSS writers to communicate back!
		-1,                             //  Default COM authentication service
		NULL,                           //  Default COM authorization service
		NULL,                           //  reserved parameter
		RPC_C_AUTHN_LEVEL_PKT_PRIVACY,  //  Strongest COM authentication level
		RPC_C_IMP_LEVEL_IMPERSONATE,    //  Minimal impersonation abilities
		NULL,                           //  Default COM authentication settings
		EOAC_DYNAMIC_CLOAKING,          //  Cloaking
		NULL                            //  Reserved parameter
	);
	if (!SUCCEEDED(hResult)) {
		printf("Failed CoInitializeSecurity function \n");
		DisplayError(GetLastError());
		goto Main_Exit;
	}


	//
	//  Initialize handle in case of error
	//

	context.ShutDown = NULL;

	//
	//  Open the port that is used to talk to
	//  MiniSpy.
	//

	printf("Connecting to filter's port...\n");

	hResult = FilterConnectCommunicationPort(MINISPY_PORT_NAME,
		0,
		NULL,
		0,
		NULL,
		&port);

	if (IS_ERROR(hResult)) {

		printf("Could not connect to filter: 0x%08x\n", hResult);
		DisplayError(hResult);
		goto Main_Exit;
	}

	//
	// Initialize the fields of the LOG_CONTEXT
	//

	context.Port = port;
	context.ShutDown = NULL;
	context.CleaningUp = FALSE;
	context.LogToFile = TRUE;
	context.LogToScreen = TRUE;        //don't start logging yet
	context.ShouldFilter = FALSE;
	context.OutputFile = NULL;
	
	FilterDetach(MINISPY_NAME, L"C:\\", 0);
	FilterDetach(MINISPY_NAME, L"E:\\", 0);

	//
	// Create the thread to read the log records that are gathered
	// by MiniSpy.sys.
	//


#if 1
#pragma region NAR_TEST
	WCHAR instanceName[INSTANCE_NAME_MAX_CHARS + 1];

#pragma endregion
#endif 

	while (inputChar = (CHAR)getchar()) {
		if (inputChar == 'q' || inputChar == 'Q') {
			goto Main_Cleanup;
		}
		if (inputChar == 'd' || inputChar == 'D') {
			nar_record* DiffRecords = 0;
			DWORD FileSize = 0;

			FilterDetach(MINISPY_NAME, L"E:\\", 0);
			NarCloseThreadCom(&context, &thread);

			CComPtr<IVssBackupComponents> ptr;
			std::wstring ShadowPath = GetShadowPath(L"E:\\", ptr);
			printf("ShadowPath => %S\n", ShadowPath.c_str());

			
			HANDLE MetadataHandle = CreateFileA(
				DB_METADATA_FILE_NAME,
				GENERIC_READ,
				FILE_SHARE_READ,
				NULL,
				OPEN_EXISTING,
				NULL,
				NULL
			);
			if (MetadataHandle == INVALID_HANDLE_VALUE) {
				printf("Could not open diff metadata file ..\n");
				DisplayError(GetLastError());
				goto Main_Cleanup;
			}


			FileSize = GetFileSize(MetadataHandle, 0);
			DiffRecords = (nar_record*)malloc(FileSize);
			UINT NRecords = FileSize / sizeof(nar_record);
			if (FileSize % sizeof(nar_record) != 0) {
				printf("Metadata is corrupted, it is not aligned to fit nar_record structure\n");
				printf("Filesize => %d\n", FileSize);
				DisplayError(GetLastError());
				goto Main_Cleanup;
			}
			DWORD BytesRead;
			HRESULT Result;
			printf("Metadata reading..\n");
			Result = ReadFile(MetadataHandle, DiffRecords, FileSize, &BytesRead, 0);
			if (!SUCCEEDED(Result) || BytesRead != FileSize) {
				printf("Cant read difmetadata \n");
				DisplayError(GetLastError());
			}

			printf("Read metadata successfully..\n");

#if 0
			{
				//Store mft sperately
				HANDLE MftHandle = CreateFile((ShadowPath + L"\\$(MFT)").c_str(),
					GENERIC_READ,
					FILE_SHARE_WRITE | FILE_SHARE_DELETE | FILE_SHARE_READ,
					0, OPEN_EXISTING, 0, 0);
				if (MftHandle == INVALID_HANDLE_VALUE) {
					printf("Can not open mft table in VSS\n");
					DisplayError(GetLastError());
					goto Main_Cleanup;
				}

				HANDLE MftOutput = CreateFileA(DB_MFT_FILE_NAME, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
				if (MftOutput == INVALID_HANDLE_VALUE) {
					printf("Can not open file to store mft table\n");
					DisplayError(GetLastError());
					goto Main_Cleanup;
				}

				DWORD BytesOperated = 0;
				DWORD BufferSize = 1024; // 1 KB
				void* Buffer = malloc(BufferSize);

				for (;;) {
					Result = ReadFile(MftHandle, Buffer, BufferSize, &BytesOperated, 0);
					
					if (Result = ERROR_HANDLE_EOF) {
						if (BytesOperated != 0) {
							DWORD Temp = 0;
							Result = WriteFile(MftOutput, Buffer, BytesOperated, &Temp, 0);
							if (!SUCCEEDED(Result) || Temp != BytesOperated) {
								printf("ERR844\n");
								DisplayError(GetLastError());
								goto Main_Cleanup;
							}
						}
						break;
					}

					if (BytesOperated == 0) {
						printf("ERR833\n");
						DisplayError(GetLastError());
						goto Main_Cleanup;
					}
					Result = WriteFile(MftOutput, Buffer, BufferSize, &BytesOperated, 0);
					if (!SUCCEEDED(Result) || BytesOperated != BufferSize) {
						printf("Writefile failed @ mftoutput\n");
						DisplayError(GetLastError());
						goto Main_Cleanup;
					}
				}//saved mft file
				CloseHandle(MftOutput);
				CloseHandle(MftHandle);
				free(Buffer);
			}
#endif

			/*
			NOT IMPLEMENTED
				So problem is, which I can not describe properly but at least can show it
				Let Rn be the record with at the index of N, so R0 becomes first record, R1 second and so on

				Let assume there are three records, at least one is overlapping with other records regions

				R1 =>                      |-----------------|
				R2 =>         |--------------|
				R3 =>                                |------------------|
				We can shrink that information such that, only storing new record like start => R2.start and len = R3.Len + R3.Start

				If such collisions ever occurs, we can shrink them. Algorithm below does that.
				But in order to process that, list MUST be sorted so we can compare consequent list elements
			NOT IMPLEMENTED
			*/
			HANDLE ShadowHandle = CreateFileW(
				ShadowPath.c_str(),
				GENERIC_READ,
				0,
				NULL,
				OPEN_EXISTING,
				NULL,
				NULL
			);
			if (ShadowHandle == INVALID_HANDLE_VALUE) {
				printf("Can not open shadow path ..\n");
				DisplayError(GetLastError());
			}

			HANDLE FileToStore = CreateFileA(
				DB_FILE_NAME,
				GENERIC_WRITE,
				0, 0,
				CREATE_ALWAYS, 0, 0
			);
			if (FileToStore == INVALID_HANDLE_VALUE) {
				printf("Can not create file to store diff disk data.\n");
				DisplayError(GetLastError());
				goto Main_Cleanup;
			}


			DWORD BytesOperated;
			DWORD BufferSize = 1024 * 1024 * 128;
			void* Buffer = malloc(BufferSize);

			DWORD ClusterSize = 512 * 8;
			LARGE_INTEGER NewFilePointer;
			NewFilePointer.QuadPart = 0;

			printf("Saving differential state of the disk, please do not close the program\n");
			for (UINT i = 0; i < NRecords; i++) {
				LARGE_INTEGER MoveTo;
				DWORD OperationSize = DiffRecords[i].Len * ClusterSize;
				MoveTo.QuadPart = ClusterSize * DiffRecords[i].StartPos;
				Result = SetFilePointerEx(ShadowHandle, MoveTo, &NewFilePointer, FILE_BEGIN);
				if (Result == FALSE || NewFilePointer.QuadPart != MoveTo.QuadPart) {
					printf("Failed to set file pointer..\n");
					DisplayError(GetLastError());
					goto Main_Cleanup;
				}

				if (DiffRecords[i].Len > BufferSize/ClusterSize) {
					printf("Operation length is bigger than buffer size, program can not execute this for demo (OperationLen in # clusters => %I64d)..\n", DiffRecords[i].Len);
					goto Main_Cleanup;
				}


				Result = ReadFile(ShadowHandle, Buffer, OperationSize, &BytesOperated, 0);
				if (Result == FALSE || BytesOperated != OperationSize) {
					printf("Could not read from shadow path.\n");
					printf("BytesReturned => %I64d\n", BytesOperated);
					printf("IterLen*ClusterSize => %I64d\n", OperationSize);
					DisplayError(GetLastError());
					goto Main_Cleanup;
				}


				Result = WriteFile(FileToStore, Buffer, OperationSize, &BytesOperated, 0);
				if (!SUCCEEDED(Result) || BytesOperated != OperationSize) {
					printf("Error occured while writing data to file\n");
					printf("BytesReturned => %I64d\n", BytesOperated);
					printf("IterLen*ClusterSize => %I64d\n", OperationSize);
					DisplayError(GetLastError());
					goto Main_Cleanup;
				}

			}
			printf("Saved diff state!\n");
			
			printf("Rewriting metadata\n");
			
			//Result = SetFilePointer(MetadataHandle, 0, 0, FILE_BEGIN);
			//if (Result != 0) {
			//	printf("Cant set metadata file pointer\n");
			//	DisplayError(GetLastError());
			//	goto Main_Cleanup;
			//}
			//
			//Result = WriteFile(MetadataHandle, DiffRecords, FileSize, &BytesOperated, 0);
			//if (!SUCCEEDED(Result) || BytesOperated != FileSize) {
			//	printf("Cant write new metadata file\n");
			//	printf("BytesOperated => %d\n",BytesOperated);
			//	DisplayError(GetLastError());
			//	goto Main_Cleanup;
			//}
			printf("Completed diff \n");
			CloseHandle(MetadataHandle);
			CloseHandle(FileToStore);
			CloseHandle(ShadowHandle);
			free(Buffer);

		}
		if (inputChar == 'f' || inputChar == 'F') { 
			// TODO check if VSS requests could be filtered with driver with some kind of flag
			printf("Waiting shadowpath func!\n");
			CComPtr<IVssBackupComponents> ptr;
			std::wstring ShadowPath = GetShadowPath(L"E:\\", ptr);
			
			FilterAttach(MINISPY_NAME, L"E:\\", 0, 0, 0);
			NarCreateThreadCom(&context, &thread);
			context.ShouldFilter = TRUE;

			printf("%S\n", ShadowPath.c_str());
			
			hResult = FilterAttach(MINISPY_NAME,
				L"E:\\",
				NULL, // instance name
				sizeof(instanceName),
				instanceName);

			if (SUCCEEDED(hResult)) {
				printf("    Instance name: %S\n", instanceName);
			}
			else {
				if (hResult != ERROR_FLT_INSTANCE_NAME_COLLISION) {
					printf("\n    Could not attach to device: 0x%08x\n", hResult);
					DisplayError(hResult);
					goto Main_Cleanup;
				}
			}


			HANDLE DiskHandle = CreateFileW(
				ShadowPath.c_str(),
				GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				NULL,
				OPEN_EXISTING,
				0,
				NULL
			);
			if (DiskHandle == INVALID_HANDLE_VALUE) {
				printf("Couldn't open shadow disk. Terminating now!\n");
				goto Main_Cleanup;
			}

			STARTING_LCN_INPUT_BUFFER StartingLCN;
			StartingLCN.StartingLcn.QuadPart = 0;
			VOLUME_BITMAP_BUFFER* Bitmap = NULL;
			DWORD BytesReturned;
			ULONGLONG MaxClusterCount = 0;
			DWORD BufferSize = 1024 * 1024 * 64; // 64megabytes
			HRESULT Result = 0;

			Bitmap = (VOLUME_BITMAP_BUFFER*)malloc(BufferSize);
			if (Bitmap == NULL) {
				printf("Couldn't allocate bitmap buffer! This fatal error. Terminating..\n");
				goto Main_Cleanup;
			}

			Result = DeviceIoControl(
				DiskHandle,
				FSCTL_GET_VOLUME_BITMAP,
				&StartingLCN,
				sizeof(StartingLCN),
				Bitmap,
				BufferSize,
				&BytesReturned,
				0
			);
			if (!SUCCEEDED(Result)) {
				printf("Get volume bitmap failed..\n");
				Result = GetLastError();
				DisplayError(Result);
				goto Main_Cleanup;
			}

			MaxClusterCount = Bitmap->BitmapSize.QuadPart;
			DWORD ClustersRead = 0;
			UCHAR* BitmapIndex = Bitmap->Buffer;
			ULONGLONG UsedClusterCount = 0;
			UCHAR BitmapMask = 1;
			DWORD ToMove = BufferSize;
			DWORD ClusterSize = 8 * 512;
			
			UINT* ClusterIndices = (UINT*)malloc(MaxClusterCount * sizeof(UINT));
			UINT CurrentIndex = 0;
			while (ClustersRead < MaxClusterCount) {
				if ((*BitmapIndex & BitmapMask) == BitmapMask) {
					ClusterIndices[CurrentIndex++] = (ClustersRead);
					UsedClusterCount++;
				}
				BitmapMask <<= 1;
				if (BitmapMask == 0) {
					BitmapMask = 1;
					BitmapIndex++;
				}
				ClustersRead++;
			}

			free(Bitmap);
			
			HANDLE OutputHandle = CreateFileA(
				FB_FILE_NAME,
				GENERIC_WRITE,
				0,
				NULL,
				CREATE_ALWAYS,
				0, 0
			);
			if (OutputHandle == INVALID_HANDLE_VALUE) {
				printf("Could not create file " FB_FILE_NAME);
				Result = GetLastError();
				DisplayError(Result);
				goto Main_Cleanup;
			}


			UINT64 FileBufferSize = ClusterSize;
			void* DataBuffer = malloc(FileBufferSize);
			DWORD BytesTransferred = 0;
			LARGE_INTEGER NewFilePointer;

			NewFilePointer.QuadPart = 0;
			
			printf("Saving disk state please do not close the program...\n");
			for (UINT i = 0; i < CurrentIndex; i++) {
				LARGE_INTEGER MoveTo;
				MoveTo.QuadPart = ClusterSize * ClusterIndices[i];

				Result = SetFilePointerEx(DiskHandle, MoveTo, &NewFilePointer, FILE_BEGIN);
				if ((Result != TRUE) || (NewFilePointer.QuadPart != MoveTo.QuadPart)) {
					printf("Setfilepointerex failed..\n");
					DisplayError(GetLastError());
					goto Main_Cleanup;
				}

				Result = ReadFile(DiskHandle, DataBuffer, ClusterSize, &BytesTransferred, 0);
				if(Result != TRUE || BytesTransferred != ClusterSize){
					printf("Readfile failed..\n");
					DisplayError(GetLastError());
					goto Main_Cleanup;
				}

				Result = WriteFile(OutputHandle, DataBuffer, ClusterSize, &BytesTransferred, 0);
				if (Result != TRUE || BytesTransferred != ClusterSize) {
					printf("WriteFile failed..\n");
					DisplayError(GetLastError());
					goto Main_Cleanup;
				}
			}
			printf("Disk stated saved to file " FB_FILE_NAME "\n");
			// save bitmap info
			HANDLE MetadataFileHandle = CreateFileA(
				FB_METADATA_FILE_NAME,
				GENERIC_WRITE,
				0, 0, 
				CREATE_ALWAYS, 0, 0
			);
			if (MetadataFileHandle == INVALID_HANDLE_VALUE) {
				printf("Could not create file for metadata information.. \n");
				DisplayError(GetLastError());
				goto Main_Cleanup;
			}


			Result = WriteFile(MetadataFileHandle, ClusterIndices, sizeof(UINT) * CurrentIndex, &BytesTransferred, 0);
			if (!SUCCEEDED(Result) || BytesTransferred != sizeof(UINT) * CurrentIndex) {
				printf("Failed to write metadata information to file..\n");
				printf("BytesTransferred => %d\n", BytesTransferred);
				printf("OpSize => %d\n", sizeof(UINT) * CurrentIndex);

				DisplayError(GetLastError());
				goto Main_Cleanup;
			}
			printf("Disk bitmap info saved to the file \n");

			printf("You can start diff backup operation by pressing [d] and [enter]\n");
			
			CloseHandle(MetadataFileHandle);
			CloseHandle(DiskHandle);
			CloseHandle(OutputHandle);
			free(DataBuffer);
			free(ClusterIndices);

		}
		
		if (inputChar == 'r' || inputChar == 'R') {
			//Restore to diff.
			HRESULT Result = 0;
			HANDLE FBackupFile = 0; //full backup data
			HANDLE FBackupMDFile = 0; //metadata for full backup
			HANDLE DBackupFile = 0; // diff backup data
			HANDLE DBackupMDFile = 0; //metadata for diff backup
			HANDLE VolumeToRestore = 0; 
			DWORD BufferSize = 1024 * 1024 * 128; //128MB
			DWORD ClusterSize = 8 * 512;
			DWORD BytesOperated = 0;
			DWORD FileSize = 0;
			UINT* FBClusterIndices = 0;
			nar_record* DBRecords = 0;

			void* Buffer = malloc(BufferSize);

			if (Buffer == NULL) {
				printf("Can't allocate buffer\n");
				DisplayError(GetLastError());
				goto R_Cleanup;
			}

			//Opening handles first
			FBackupFile = CreateFileA(
				FB_FILE_NAME,
				GENERIC_READ,
				FILE_SHARE_READ,
				0, OPEN_EXISTING, 0, 0
			);
			if (FBackupFile == INVALID_HANDLE_VALUE) {
				printf("Can't open fullbackup file for restore operation\n");
				DisplayError(GetLastError());
				goto R_Cleanup;
			}

			FBackupMDFile = CreateFileA(
				FB_METADATA_FILE_NAME,
				GENERIC_READ,
				FILE_SHARE_READ,
				0, OPEN_EXISTING, 0, 0
			);
			if (FBackupMDFile == INVALID_HANDLE_VALUE) {
				printf("Can't open fullbackup metadata file for restore operation\n");
				DisplayError(GetLastError());
				goto R_Cleanup;
			}

			DBackupFile = CreateFileA(
				DB_FILE_NAME,
				GENERIC_READ,
				FILE_SHARE_READ,
				0, OPEN_EXISTING, 0, 0
			);
			if (DBackupFile == INVALID_HANDLE_VALUE) {
				printf("Can't open diffbackup file for restore operation\n");
				DisplayError(GetLastError());
				goto R_Cleanup;
			}

			DBackupMDFile = CreateFileA(
				DB_METADATA_FILE_NAME,
				GENERIC_READ,
				FILE_SHARE_READ,
				0, OPEN_EXISTING, 0, 0
			);
			if (FBackupMDFile == INVALID_HANDLE_VALUE) {
				printf("Can't open diffbackup metadata file for restore operation\n");
				DisplayError(GetLastError());
				goto R_Cleanup;
			}

			VolumeToRestore = CreateFileA(
				"\\\\.\\E:",
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ,
				0,
				OPEN_EXISTING,
				0,
				NULL
			);
			if (VolumeToRestore == INVALID_HANDLE_VALUE) {
				printf("Can't open volume to restore\n");
				DisplayError(GetLastError());
				goto R_Cleanup;
			}

			Result = DeviceIoControl(
				VolumeToRestore,
				FSCTL_LOCK_VOLUME,
				0, 0, 0, 0, 0, 0
			);
			if (!SUCCEEDED(Result)) {
				printf("Can't lock volume\n");
				DisplayError(GetLastError());
				goto R_Cleanup;
			}
			
			FileSize = GetFileSize(FBackupMDFile, 0);
			FBClusterIndices = (UINT*)malloc(FileSize);
			Result = ReadFile(FBackupMDFile, FBClusterIndices, FileSize, &BytesOperated, 0);
			if (!SUCCEEDED(Result) || BytesOperated != FileSize) {
				printf("Error occured while reading diff metadata file.. \n");
				printf("BytesRead => %d\n", BytesOperated);
				DisplayError(GetLastError());
				goto R_Cleanup;
			}
			printf("Read-> %d\n", BytesOperated);
			printf("FileSize was-> %d\n", FileSize);

			UINT FBClusterCount = FileSize / sizeof(UINT);
			LARGE_INTEGER MoveTo;
			LARGE_INTEGER NewFilePointer;
			MoveTo.QuadPart = 0;
			NewFilePointer.QuadPart = 0;
			printf("FBClusterCount => %d\n", FBClusterCount);

			for (UINT i = 0; i < FBClusterCount; i++) {
				MoveTo.QuadPart = ClusterSize * FBClusterIndices[i];
				Result = SetFilePointerEx(VolumeToRestore, MoveTo, &NewFilePointer, FILE_BEGIN);
				if (!SUCCEEDED(Result) || MoveTo.QuadPart != NewFilePointer.QuadPart) {
					printf("Setfilepointerex failed!\n");
					printf("Iter was -> %i\n", i);
					DisplayError(GetLastError());
					goto R_Cleanup;
				}

				Result = ReadFile(FBackupFile, Buffer, ClusterSize, &BytesOperated, 0);
				if (!SUCCEEDED(Result) || BytesOperated != ClusterSize) {
					printf("ReadFile failed!\n");
					printf("Iter was -> %i\n", i);
					DisplayError(GetLastError());
					goto R_Cleanup;
				}

				Result = WriteFile(VolumeToRestore, Buffer, ClusterSize, &BytesOperated, 0);
				if (!SUCCEEDED(Result) || BytesOperated != ClusterSize) {
					printf("WriteFile failed!\n");
					printf("Iter was -> %i\n", i);
					DisplayError(GetLastError());
					goto R_Cleanup;
				}
			}
			printf("Restored to full, continuing to diff restore\n");
			
			FileSize = GetFileSize(DBackupMDFile, 0);
			DBRecords = (nar_record*)malloc(FileSize);
			UINT RecordCount = FileSize / sizeof(nar_record);
			Result = ReadFile(DBackupMDFile, DBRecords, FileSize, &BytesOperated, 0);
			if (!SUCCEEDED(Result) || BytesOperated != FileSize) {
				printf("Can't read diff metadata\n");
				DisplayError(GetLastError());
				goto R_Cleanup;
			}
			printf("Read-> %d\n", BytesOperated);
			printf("FileSize was-> %d\n", FileSize);


			MoveTo.QuadPart = 0;
			NewFilePointer.QuadPart = 0;
			printf("RecordCount => %d\n", RecordCount);
			for (UINT i = 0; i < RecordCount; i++) {
				printf("Test-> %d\t%d\n", DBRecords[i].StartPos, DBRecords[i].Len);
				MoveTo.QuadPart = DBRecords[i].StartPos * ClusterSize;
				Result = SetFilePointerEx(VolumeToRestore, MoveTo, &NewFilePointer, FILE_BEGIN);
				if (!SUCCEEDED(Result) || NewFilePointer.QuadPart != MoveTo.QuadPart) {
					printf("SetFilePointerEx failed\n");
					printf("Iter was -> %i\n", i);
					DisplayError(GetLastError());
					goto R_Cleanup;
				}
				
				DWORD ReadSize = DBRecords[i].Len * ClusterSize;
				Result = ReadFile(DBackupFile, Buffer, ReadSize, &BytesOperated, 0);
				if (!SUCCEEDED(Result) || BytesOperated != ReadSize) {
					printf("ReadSize failed\n");
					printf("Iter was -> %i\n", i);
					DisplayError(GetLastError());
					goto R_Cleanup;
				}

				Result = WriteFile(VolumeToRestore, Buffer, ReadSize, &BytesOperated, 0);
				if (!SUCCEEDED(Result) || BytesOperated != ReadSize) {
					printf("WriteFile failed!\n");
					printf("Iter was -> %i\n", i);
					DisplayError(GetLastError());
					goto R_Cleanup;
				}

			}

			printf("Restored to diff\n");
			Result = DeviceIoControl(
				VolumeToRestore,
				FSCTL_UNLOCK_VOLUME,
				0, 0, 0, 0, 0, 0
			);
			if (!SUCCEEDED(Result)) {
				printf("Can't unlock volume\n");
				DisplayError(GetLastError());
				goto R_Cleanup;
			}

		R_Cleanup:
			printf("Cleaning up restore operation..\n");
			CLEANHANDLE(FBackupFile);
			CLEANHANDLE(FBackupMDFile);
			CLEANHANDLE(DBackupFile);
			CLEANHANDLE(DBackupMDFile);
			CLEANHANDLE(VolumeToRestore);
			
			CLEANMEMORY(Buffer);
			CLEANMEMORY(FBClusterIndices);
			CLEANMEMORY(DBRecords);

			printf("Cleaned!\n");

		}


		Sleep(50);
	}

Main_Cleanup:

	//
	// Clean up the threads, then fall through to Main_Exit
	//

	printf("Cleaning up...\n");

	//
	// Set the Cleaning up flag to TRUE to notify other threads
	// that we are cleaning up
	//
	context.CleaningUp = TRUE;

	//
	// Wait for everyone to shut down
	//

	WaitForSingleObject(context.ShutDown, INFINITE);

	if (context.LogToFile) {
		CloseHandle(context.OutputFile);
	}

Main_Exit:

	//
	// Clean up the data that is always around and exit
	//

	if (context.ShutDown) {

		CloseHandle(context.ShutDown);
	}

	if (thread) {

		CloseHandle(thread);
	}

	if (INVALID_HANDLE_VALUE != port) {
		CloseHandle(port);
	}
	return 0;
}


VOID
DisplayError(
	_In_ DWORD Code
)

/*++

Routine Description:

	 This routine will display an error message based off of the Win32 error
	 code that is passed in. This allows the user to see an understandable
	 error message instead of just the code.

Arguments:

	 Code - The error code to be translated.

Return Value:

	 None.

--*/

{
	WCHAR buffer[MAX_PATH] = { 0 };
	DWORD count;
	HMODULE module = NULL;
	HRESULT status;

	count = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		Code,
		0,
		buffer,
		sizeof(buffer) / sizeof(WCHAR),
		NULL);


	if (count == 0) {

		count = GetSystemDirectory(buffer,
			sizeof(buffer) / sizeof(WCHAR));

		if (count == 0 || count > sizeof(buffer) / sizeof(WCHAR)) {

			//
			//  In practice we expect buffer to be large enough to hold the 
			//  system directory path. 
			//

			printf("    Could not translate error: %d\n", Code);
			return;
		}


		status = StringCchCat(buffer,
			sizeof(buffer) / sizeof(WCHAR),
			L"\\fltlib.dll");

		if (status != S_OK) {

			printf("    Could not translate error: %d\n", Code);
			return;
		}

		module = LoadLibraryExW(buffer, NULL, LOAD_LIBRARY_AS_DATAFILE);

		//
		//  Translate the Win32 error code into a useful message.
		//

		count = FormatMessage(FORMAT_MESSAGE_FROM_HMODULE,
			module,
			Code,
			0,
			buffer,
			sizeof(buffer) / sizeof(WCHAR),
			NULL);

		if (module != NULL) {

			FreeLibrary(module);
		}

		//
		//  If we still couldn't resolve the message, generate a string
		//

		if (count == 0) {

			printf("    Could not translate error: %d\n", Code);
			return;
		}
	}

	//
	//  Display the translated error.
	//

	printf("    %ws\n", buffer);
}