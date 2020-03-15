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

#define MAX(v1,v2) ((v1)>(v2) ? (v1) : (v2))
#define MIN(v1,v2) ((v1)<(v2) ? (v1) : (v2))

#define CLEANHANDLE(handle) if((handle)!=NULL) CloseHandle(handle);
#define CLEANMEMORY(memory) if((memory)!=NULL) free(memory);

#define MINISPY_NAME  L"MiniSpy"

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

BOOL
IsRegionsCollide(nar_record* R1, nar_record* R2){
		BOOL Result = FALSE;
		ULONGLONG R1EndPoint = R1->StartPos + R1->Len;
		ULONGLONG R2EndPoint = R2->StartPos + R2->Len;
		
		if((R1EndPoint < R2EndPoint
				&& R1EndPoint >= R2->StartPos)
			 || (R2EndPoint < R1EndPoint
					 && R2EndPoint >= R1->StartPos)
			 ){
				Result = TRUE;
		}
		
		return Result;
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
wmain(
			_In_ int argc,
			_In_reads_(argc) WCHAR* argv[]
			){
		
		/*
		minispy.exe -From -To  one letter
		
ex usage:
minispy.exe D E 
track D: volume, restore to E: volume if requested
*/
		WCHAR FromVolLetter = 0;
		WCHAR ToVolLetter = 0;
		if(argc == 3){
				FromVolLetter = argv[1][0];
				ToVolLetter = argv[2][0];
		}
		else if(argc == 2){
				printf("Connecting to filter's port...\n");
				HANDLE Port;
				
				HRESULT Result = FilterConnectCommunicationPort(MINISPY_PORT_NAME,
																												0,
																												NULL,
																												0,
																												NULL,
																												&Port);
				
				if(!SUCCEEDED(Result)){
						printf("Cant create communication port\n");
						DisplayError(GetLastError());
						goto Arg_Cleanup;
				}
				
				WCHAR TempV[] = L" :\\";
				TempV[0] = argv[1][0];
				Result = FilterDetach(MINISPY_NAME, TempV, 0);
				if(!SUCCEEDED(Result)){
						printf("Cant detach volume %S\n",TempV);
						DisplayError(GetLastError());
						goto Arg_Cleanup;
				}
				
				Arg_Cleanup:
				CLEANHANDLE(Port);
				
		}
		else{
				printf("Invalid usage!\n");
				return 0;
		}
		
		WCHAR TrackedVolumeName[] = L" :\\";
		WCHAR RestoreVolumeName[] = L" :\\";
		TrackedVolumeName[0] = FromVolLetter;
		RestoreVolumeName[0] = ToVolLetter;
		printf("Programm will backup volume %S, and restore it to %S if requested\n", TrackedVolumeName, RestoreVolumeName);

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
		
		//
		// Create the thread to read the log records that are gathered
		// by MiniSpy.sys.
		//
		
		
#if 1
#pragma region NAR_TEST
		WCHAR instanceName[INSTANCE_NAME_MAX_CHARS + 1];
		
#pragma endregion
#endif 
		printf("Press [q] to quit\n");
		printf("Press [f] to get full backup of the volume, [d] to diff, [r] to restore\n");
		while (inputChar = (CHAR)getchar()) {
				if (inputChar == 'q' || inputChar == 'Q') {
						goto Main_Cleanup;
				}
				else if (inputChar == 'd' || inputChar == 'D') {
						nar_record* DiffRecords = 0;
						void* Buffer = 0;
						DWORD FileSize = 0;
						HANDLE MetadataHandle = 0;
						HANDLE DiffDataFile = 0;
						HANDLE ShadowHandle = 0;
						HANDLE MftOutput = 0;
						HANDLE MftHandle = 0;
						DWORD BytesOperated = 0;
						DWORD BufferSize = 1024 * 1024 * 128; // 128 MB;
						HRESULT Result = 0;
						std::wstring ShadowPath = L"";
						CComPtr<IVssBackupComponents> ptr;
						
						Result = FilterDetach(MINISPY_NAME, TrackedVolumeName, 0);
						if(!SUCCEEDED(Result)){
								printf("Cant detach filter.\n");
								DisplayError(GetLastError());
								goto D_Cleanup;
						}
						
						NarCloseThreadCom(&context, &thread);
						
						printf("Minifilter stopped!\n");
						
						ShadowPath = GetShadowPath(TrackedVolumeName, ptr);
						
						MetadataHandle = CreateFileA(
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
								goto D_Cleanup;
						}
						
						
						FileSize = GetFileSize(MetadataHandle, 0);
						DiffRecords = (nar_record*)malloc(FileSize);
						UINT NRecords = FileSize / sizeof(nar_record);
						if (FileSize % sizeof(nar_record) != 0) {
								printf("Metadata is corrupted, it is not aligned to fit nar_record structure\n");
								printf("Filesize => %d\n", FileSize);
								DisplayError(GetLastError());
								goto D_Cleanup;
						}
						
						printf("Metadata reading..\n");
						Result = ReadFile(MetadataHandle, DiffRecords, FileSize, &BytesOperated, 0);
						if (!SUCCEEDED(Result) || BytesOperated != FileSize) {
								printf("Cant read difmetadata \n");
								DisplayError(GetLastError());
								goto D_Cleanup;
						}
						
						printf("Read metadata successfully..\n");
						
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
#if 0
						/*
possible implementation of algorithm above
*/
						UINT32 BufferSize = UINT32((float)NRecords*(0.78f) + 1)*sizeof(nar_record);
						nar_record *MergedDiffRecords = malloc(BufferSize);
						
						UINT32 MergedRecordsIndex = 0;
						UINT32 CurrentIter = 0;
						
						for(;;){
								if(CurrentIter == NRecords - 1){
										//last record in the array
										break;
								}
								
								MergedDiffRecords[MergedRecordsIndex].StartPos = DiffRecords[CurrentIter].StartPos;
								
								ULONGLONG EndPointTemp = 0;
								
								while(DiffRecords[CurrentIter].StartPos == DiffRecords[CurrentIter + 1].StartPos
											|| IsRegionsCollide(&DiffRecords[CurrentIter], &DiffRecords[CurrentIter + 1] )) {
										ULONGLONG EP1 = DiffRecords[CurrentIter].StartPos + DiffRecords[CurrentIter].Len;
										ULONGLONG EP2 = DiffRecords[CurrentIter + 1].StartPos + DiffRecords[CurrentIter + 1].Len;
										
										EndPointTemp = MAX(EP1,EP2);
										
										CurrentIter++;
								}
								
								MergedDiffRecords[MergedRecordsIndex].Len = EndPointTemp - MergedDiffRecords[MergedRecordsIndex].StartPos;
								MergedRecordsIndex++;
								
						}
						
						
#endif
						
						ShadowHandle = CreateFileW(
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
						
						DiffDataFile = CreateFileA(
																			 DB_FILE_NAME,
																			 GENERIC_WRITE,
																			 0, 0,
																			 CREATE_ALWAYS, 0, 0
																			 );
						if (DiffDataFile == INVALID_HANDLE_VALUE) {
								printf("Can not create file to store diff disk data.\n");
								DisplayError(GetLastError());
								goto D_Cleanup;
						}
						
						
						
						Buffer = malloc(BufferSize);
						
						DWORD ClusterSize = 512 * 8;
						LARGE_INTEGER NewFilePointer;
						NewFilePointer.QuadPart = 0;
						
						printf("Saving differential state of the disk, please do not close the program\n");
						for (UINT i = 0; i < NRecords; i++) {
								LARGE_INTEGER MoveTo;
								DWORD OperationSize = DiffRecords[i].Len * ClusterSize;
								MoveTo.QuadPart = ClusterSize * DiffRecords[i].StartPos;
								Result = SetFilePointerEx(ShadowHandle, MoveTo, &NewFilePointer, FILE_BEGIN);
								if (!SUCCEEDED(Result) || NewFilePointer.QuadPart != MoveTo.QuadPart) {
										printf("Failed to set file pointer..\n");
										DisplayError(GetLastError());
										goto D_Cleanup;
								}
								
								if (DiffRecords[i].Len > BufferSize/ClusterSize) {
										printf("Operation length is bigger than buffer size, program can not execute this for demo (OperationLen in # clusters => %I64d)..\n", DiffRecords[i].Len);
										goto D_Cleanup;
								}
								
								
								Result = ReadFile(ShadowHandle, Buffer, OperationSize, &BytesOperated, 0);
								if (!SUCCEEDED(Result) || BytesOperated != OperationSize) {
										printf("Could not read from shadow path.\n");
										printf("BytesReturned => %I64d\n", BytesOperated);
										printf("IterLen*ClusterSize => %I64d\n", OperationSize);
										DisplayError(GetLastError());
										goto D_Cleanup;
								}
								
								
								Result = WriteFile(DiffDataFile, Buffer, OperationSize, &BytesOperated, 0);
								if (!SUCCEEDED(Result) || BytesOperated != OperationSize) {
										printf("Error occured while writing data to file\n");
										printf("BytesReturned => %I64d\n", BytesOperated);
										printf("IterLen*ClusterSize => %I64d\n", OperationSize);
										DisplayError(GetLastError());
										goto D_Cleanup;
								}
								
						}
						printf("Saved diff state!\n");
						
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
						D_Cleanup:
						CLEANMEMORY(Buffer);
						CLEANMEMORY(DiffRecords);
						
						CLEANHANDLE(MetadataHandle);
						CLEANHANDLE(DiffDataFile);
						CLEANHANDLE(ShadowHandle);
						CLEANHANDLE(MftOutput);
						CLEANHANDLE(MftHandle);
						
				}
				else if (inputChar == 'f' || inputChar == 'F') { 
						HRESULT Result = 0;
						VOLUME_BITMAP_BUFFER* Bitmap = 0;
						UINT* ClusterIndices = 0;
						void* DataBuffer = 0;
						
						HANDLE DiskHandle = 0;
						HANDLE OutputHandle = 0;
						HANDLE MetadataFileHandle = 0;
						
						// TODO check if VSS requests could be filtered with driver with some kind of flag
						printf("Waiting VSS!\n");
						CComPtr<IVssBackupComponents> ptr;
						std::wstring ShadowPath = GetShadowPath(TrackedVolumeName, ptr);
						
						Result = FilterAttach(MINISPY_NAME, TrackedVolumeName, 0, 0, 0);
						if(!SUCCEEDED(Result)){
								printf("Cant attach filter.\n");
								DisplayError(GetLastError());
								goto F_Cleanup;
						}
						
						Result = NarCreateThreadCom(&context, &thread);
						if(Result == FALSE){
								printf("Cant create thread to log records\n");
								goto F_Cleanup;
						}
						context.ShouldFilter = TRUE;
						
						printf("Minifilter started!\n");
						
						printf("%S\n", ShadowPath.c_str());
						
						
						DiskHandle = CreateFileW(
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
								goto F_Cleanup;
						}
						
						STARTING_LCN_INPUT_BUFFER StartingLCN;
						StartingLCN.StartingLcn.QuadPart = 0;
						DWORD BytesReturned;
						ULONGLONG MaxClusterCount = 0;
						DWORD BufferSize = 1024 * 1024 * 64; // 64megabytes
						
						
						Bitmap = (VOLUME_BITMAP_BUFFER*)malloc(BufferSize);
						if (Bitmap == NULL) {
								printf("Couldn't allocate bitmap buffer! This fatal error. Terminating..\n");
								goto F_Cleanup;
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
								goto F_Cleanup;
						}
						
						MaxClusterCount = Bitmap->BitmapSize.QuadPart;
						DWORD ClustersRead = 0;
						UCHAR* BitmapIndex = Bitmap->Buffer;
						ULONGLONG UsedClusterCount = 0;
						UCHAR BitmapMask = 1;
						DWORD ToMove = BufferSize;
						DWORD ClusterSize = 8 * 512;
						
						ClusterIndices = (UINT*)malloc(MaxClusterCount * sizeof(UINT));
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
						
						/*
//Early deallocation to save some memory. 
This is not a problem for cleanup procedure since we are checking if memory is null before deallocation anyway.
*/
				
						OutputHandle = CreateFileA(
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
								goto F_Cleanup;
						}
						
						
						UINT64 FileBufferSize = ClusterSize;
						DataBuffer = malloc(FileBufferSize);
						DWORD BytesTransferred = 0;
						LARGE_INTEGER NewFilePointer;
						
						NewFilePointer.QuadPart = 0;
						
						printf("Saving disk state please do not close the program...\n");
						for (UINT i = 0; i < CurrentIndex; i++) {
								LARGE_INTEGER MoveTo;
								MoveTo.QuadPart = ClusterSize * ClusterIndices[i];
								
								Result = SetFilePointerEx(DiskHandle, MoveTo, &NewFilePointer, FILE_BEGIN);
								if (!SUCCEEDED(Result) || (NewFilePointer.QuadPart != MoveTo.QuadPart)) {
										printf("Setfilepointerex failed..\n");
										DisplayError(GetLastError());
										goto F_Cleanup;
								}
								
								Result = ReadFile(DiskHandle, DataBuffer, ClusterSize, &BytesTransferred, 0);
								if(!SUCCEEDED(Result) || BytesTransferred != ClusterSize){
										printf("Readfile failed..\n");
										DisplayError(GetLastError());
										goto F_Cleanup;
								}
								
								Result = WriteFile(OutputHandle, DataBuffer, ClusterSize, &BytesTransferred, 0);
								if (!SUCCEEDED(Result) || BytesTransferred != ClusterSize) {
										printf("WriteFile failed..\n");
										DisplayError(GetLastError());
										goto F_Cleanup;
								}
						}
						printf("Disk stated saved to file " FB_FILE_NAME "\n");
						// save bitmap info
						MetadataFileHandle = CreateFileA(
																						 FB_METADATA_FILE_NAME,
																						 GENERIC_WRITE,
																						 0, 0, 
																						 CREATE_ALWAYS, 0, 0
																						 );
						if (MetadataFileHandle == INVALID_HANDLE_VALUE) {
								printf("Could not create file for metadata information.. \n");
								DisplayError(GetLastError());
								goto F_Cleanup;
						}
						
						
						Result = WriteFile(MetadataFileHandle, ClusterIndices, sizeof(UINT) * CurrentIndex, &BytesTransferred, 0);
						if (!SUCCEEDED(Result) || BytesTransferred != sizeof(UINT) * CurrentIndex) {
								printf("Failed to write metadata information to file..\n");
								printf("BytesTransferred => %d\n", BytesTransferred);
								printf("OpSize => %d\n", sizeof(UINT) * CurrentIndex);
								
								DisplayError(GetLastError());
								goto F_Cleanup;
						}
						printf("Disk bitmap info saved to the file \n");
						
						printf("You can start diff backup operation by pressing [d] and [enter]\n");
						
						
						F_Cleanup:
						CLEANMEMORY(Bitmap);
						CLEANMEMORY(ClusterIndices);
						CLEANMEMORY(DataBuffer);
						printf("Memory freed\n");

						CLEANHANDLE(DiskHandle);
						CLEANHANDLE(OutputHandle);
						CLEANHANDLE(MetadataFileHandle);
						printf("Handles freed\n");
				}
				else if (inputChar == 'r' || inputChar == 'R') {
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
						
						WCHAR VTRNameTemp[] = L"\\\\.\\ :";
						VTRNameTemp[lstrlenW(VTRNameTemp) - 2] = RestoreVolumeName[0];
						
						VolumeToRestore = CreateFileW(
																					VTRNameTemp,
																					GENERIC_READ | GENERIC_WRITE,
																					FILE_SHARE_READ,
																					0,
																					OPEN_EXISTING,
																					0,
																					NULL
																					);
						if (VolumeToRestore == INVALID_HANDLE_VALUE) {
								printf("Can't open volume to restore %S\n",VTRNameTemp);
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
						//printf("Read-> %d\n", BytesOperated);
						//printf("FileSize was-> %d\n", FileSize);
						
						
						MoveTo.QuadPart = 0;
						NewFilePointer.QuadPart = 0;
						printf("RecordCount => %d\n", RecordCount);
						for (UINT i = 0; i < RecordCount; i++) {
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
				else {
					printf("Press [f] to get full backup of the volume, [d] to diff, [r] to restore\n");
					printf("Press [q] to quit\n");
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