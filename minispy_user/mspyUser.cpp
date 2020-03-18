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
#include <cstdio>
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


	VOID
	DisplayError(
		_In_ DWORD Code
	);

BOOLEAN
InitRestoreTargetInf(restore_target_inf* Inf, wchar_t Letter) {
	Assert(Inf != NULL);
	WCHAR Temp[] = L"!:\\";
	Temp[0] = Letter;
	DWORD BytesPerSector = 0;
	DWORD SectorsPerCluster = 0;

	if (!GetDiskFreeSpaceW(Temp, &SectorsPerCluster, &BytesPerSector, 0, &Inf->ClusterCount)) {
		Inf->ClusterCount = 0;
		Inf->ClusterSize = 0;
	}
	Inf->ClusterSize = BytesPerSector * SectorsPerCluster;
}

BOOLEAN
InitVolumeInf(volume_backup_inf *VolInf, const wchar_t *Filepath) {
	BOOLEAN Return = FALSE;

	HANDLE Handle = CreateFileW(Filepath, GENERIC_READ, 0, 0, OPEN_ALWAYS, 0, 0);
	if (Handle != INVALID_HANDLE_VALUE) {
		DWORD FileSize = GetFileSize(Handle, 0);
		if (FileSize != 0) {
			DWORD BytesRead = 0;
			if (ReadFile(Handle, VolInf, sizeof(volume_backup_inf), &BytesRead, 0)) {
				Return = TRUE;
			}
			else {
				//TODO log
			}
		}
		else {
			//TODO Log
		}
	}
	else {
		//TODO log
	}

}

void InitVolumeInf(volume_backup_inf* VolInf, wchar_t Letter) {
	
	Assert(VolInf != NULL);
	VolInf->Letter = Letter;
	VolInf->IsActive = FALSE;
	VolInf->FullBackupExists = FALSE;
	VolInf->DiffBackupCount = 0;
	
	wchar_t Temp[] = L"!:\\";
	Temp[0] = VolInf->Letter;

	DWORD BytesPerSector = 0;
	DWORD SectorsPerCluster = 0;
	DWORD ClusterCount = 0;
	
	if (GetDiskFreeSpaceW(Temp, &SectorsPerCluster, &BytesPerSector, 0, &ClusterCount)) {
		if (SectorsPerCluster == 0 || BytesPerSector == 0) {
			VolInf->ClusterCount = ClusterCount;
			VolInf->ClusterSize = SectorsPerCluster * BytesPerSector;
		}
		else {
			printf("either sectors per cluster or bytes per sector is zero\n");
			printf("Sectors percluster-> %d, BytesPersector -> %d\n", SectorsPerCluster, BytesPerSector);
			VolInf->ClusterCount = 0;
			VolInf->ClusterSize = 0;
		}
	}
	else {
		//Failed
		VolInf->ClusterCount = 0;
		VolInf->ClusterSize = 0;
	}

	VolInf->ExtraPartitions = { 0,0 };
	VolInf->PartitionName = { 0,0 };
	VolInf->ContextIndex = 0;
	VolInf->ContextIndex = -1; //Access violation if tried to use volume without adding it to tracklist
	VolInf->LogHandle = 0;
	VolInf->ChangeCount = 0;

	//TODO link letter name and full name 
	//TODO HANDLE EXTRA PARTITIONS, SUCH AS BOOT AND RECOVERY

}



BOOLEAN
IsSameVolumes(const WCHAR* OpName, const WCHAR VolumeLetter) {
	return TRUE;//TODO
}

BOOL CompareNarRecords(const void* v1, const void* v2) {
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
IsRegionsCollide(nar_record* R1, nar_record* R2) {
	BOOL Result = FALSE;
	ULONGLONG R1EndPoint = R1->StartPos + R1->Len;
	ULONGLONG R2EndPoint = R2->StartPos + R2->Len;

	if ((R1EndPoint < R2EndPoint
		&& R1EndPoint >= R2->StartPos)
		|| (R2EndPoint < R1EndPoint
			&& R2EndPoint >= R1->StartPos)
		) {
		Result = TRUE;
	}

	return Result;
}

VOID
NarCloseThreadCom(
	PLOG_CONTEXT Context
) {
	//TODO termination of the whole program, probably..
	Assert(Context->Thread != NULL);

	Context->CleaningUp = TRUE;
	WaitForSingleObject(Context->ShutDown, INFINITE);

	CloseHandle(Context->ShutDown);
	CloseHandle(Context->Thread);

	Context->ShutDown = NULL;
}

BOOL
NarCreateThreadCom(
	PLOG_CONTEXT Context
) {
	Assert(Context->Thread == NULL);
	Assert(Context->ShutDown == NULL);

	BOOL Result = FALSE;
	
	Context->CleaningUp = FALSE;
	
	Context->ShutDown = CreateSemaphoreW(NULL,
		0,
		1,
		L"MiniSpy shut down"
	);
	if (Context->ShutDown != NULL) {
		Context->Thread = CreateThread(NULL,
			0,
			RetrieveLogRecords,
			(LPVOID)Context,
			0,
			0);
		if (Context->Thread != NULL) {
			Result = TRUE;
		}
		else {
			printf("Can't create thread to communicate with driver\n");
			DisplayError(GetLastError());
		}
		
	}
	else {
		//Unable to create semaphore
		printf("Could not create semaphore for thread..\n");
		DisplayError(GetLastError());
	}


	return Result;
}


std::string 
NarExecuteCommand(const char* cmd) {
	//TODO return char* instead of std::string

	char buffer[128];
	std::string result = "";
	FILE* pipe = _popen(cmd, "r");
	if (pipe) {
		while (fgets(buffer, sizeof buffer, pipe) != NULL) {
			result += buffer;
		}

		_pclose(pipe);
	}
	else {
		printf("popen() failed!\n");
	}

	return result;
}

/*
Pass by value, might be slow if input str is too big ?? 
*/
std::vector<std::string>
Split(std::string str, std::string delimiter) {
	UINT SizeAssumed = 100;

	std::vector<std::string> Result;	
	Result.reserve(SizeAssumed);

	size_t pos = 0;
	std::string token;
	while ((pos = str.find(delimiter)) != std::string::npos) {
		token = str.substr(0, pos);
		str.erase(0, pos + delimiter.length());
		Result.emplace_back(std::move(token));
	}
	if (str.length() > 0) Result.emplace_back(std::move(str));
	Result.shrink_to_fit();

	return Result;
}

data_array<nar_record>
GetMFTLCN(char VolumeLetter) {
	//TODO check if input falls in a-z || A-Z
	data_array<nar_record> Result = { 0,0 };
	UINT SizeAssumed = 256;
	Result.Data = (nar_record*)malloc(sizeof(nar_record) * 256);
	Result.Count = 0;

	static char Command[] = "fsutil file queryextents C:\\$mft csv";
	Command[25] = VolumeLetter;

	std::string Answer = NarExecuteCommand(Command);
	
	if (Answer[0] == '\n') Answer[0] = '#';
	Answer = Answer.substr(Answer.find("\n") + 1, Answer.size());
	
	size_t NewLinePos = 0;
	
	auto Rows = Split(Answer, "\n");
	
	for (const auto& Iter : Rows) {
		auto Vals = Split(Iter, ",");

		Result.Data[Result.Count].StartPos = std::stoull(Vals[2], 0, 16);
		Result.Data[Result.Count].Len = std::stoull(Vals[1], 0, 16);
		Result.Count++;
		if (Result.Count == SizeAssumed) {
			SizeAssumed *= 2;
			realloc(Result.Data, sizeof(Result.Data[0]) * SizeAssumed);
		}
	}

	realloc(Result.Data, sizeof(Result.Data[0]) * Result.Count);
	
	return Result;
}


std::wstring
GenerateMetadataFileName(volume_backup_inf* Inf) {
	std::wstring Result = (DB_METADATA_FILE_NAME);
	printf("TEST===> %S\n", std::wstring(1, Inf->Letter).c_str());
	Result += Inf->Letter;
	Result += std::to_wstring(Inf->DiffBackupCount);
	return Result;
}

std::wstring
GenerateDBFileName(volume_backup_inf* Inf) {
	std::wstring Return = DB_FILE_NAME; 
	Return += Inf->Letter;
	Return += std::to_wstring(Inf->DiffBackupCount);
	return Return;
}

std::wstring
GenerateFBFileName(volume_backup_inf* Inf) {
	std::wstring Return = FB_FILE_NAME;
	Return += Inf->Letter;
	return Return;
}

std::wstring
GenerateFBMetadataFileName(volume_backup_inf* Inf) {
	std::wstring Return = FB_METADATA_FILE_NAME;
	Return += Inf->Letter;
	return Return;
}

/*
This operation just adds volume to list, does not starts to filter it,
until it's fullbackup is requested.
*/
BOOLEAN
AddVolumeToTrack(PLOG_CONTEXT Context, wchar_t Letter) {
	BOOLEAN ErrorOccured = TRUE;
	HRESULT Result = 0;


	if (Context->Volumes.Data == NULL) {
		printf("Context's volume data was null, allocating memory\n");
		Context->Volumes.Data = (volume_backup_inf*)malloc(sizeof(volume_backup_inf));
		Context->Volumes.Count = 1;
	}
	else {
		for (UINT i = 0; i < Context->Volumes.Count; i++) {
			if (Context->Volumes.Data[i].Letter == Letter) {
				return FALSE;
			}
		}
		Context->Volumes.Data = (volume_backup_inf*)realloc(Context->Volumes.Data, (Context->Volumes.Count + 1) * sizeof(volume_backup_inf));
		Context->Volumes.Count += 1;

		if (Context->Volumes.Data == NULL) {
			printf("Can't reallocate memory\n"); //TODO fatal error
			DisplayError(GetLastError());
			return FALSE;
		}

	}
	printf("Memory job done\n");
	
	volume_backup_inf *VolInf = &Context->Volumes.Data[Context->Volumes.Count - 1];
	VolInf->Letter = Letter;
	VolInf->FullBackupExists = FALSE;
	VolInf->ExtraPartitions = { 0, 0 };
	VolInf->DiffBackupCount = 0;
	VolInf->IsActive = FALSE;
	VolInf->ContextIndex = Context->Volumes.Count - 1;
	
	std::wstring MetadataFileName = GenerateMetadataFileName(VolInf);
	
	VolInf->LogHandle = 0;
	
	std::wstring VolName = L"C:\\";
	VolName[0] = VolInf->Letter;
	Result = FilterAttach(MINISPY_NAME, VolName.c_str(), 0, 0, 0);
	if (!SUCCEEDED(Result)) {
		printf("Can't attach volume to filter, %S", VolName.c_str());
		DisplayError(GetLastError());
		goto Cleanup;
	}

	ErrorOccured = FALSE;
Cleanup:
	if (ErrorOccured) {
		CLEANHANDLE(VolInf->LogHandle);
		Context->Volumes.Data = (volume_backup_inf*)realloc(Context->Volumes.Data, (Context->Volumes.Count - 1) * sizeof(volume_backup_inf));
		Context->Volumes.Count -= 1;
	}
	
	return !ErrorOccured;
}

BOOLEAN 
DetachVolume(PLOG_CONTEXT Context, UINT VolInfIndex) {
	BOOLEAN Return = FALSE;
	HRESULT Result = 0;
	std::wstring Temp = L"!:\\";
	Temp[0] = Context->Volumes.Data[VolInfIndex].Letter;

	volume_backup_inf *VolInf = &Context->Volumes.Data[VolInfIndex];

	Result = FilterDetach(MINISPY_NAME, Temp.c_str(), 0);
	if (SUCCEEDED(Result)) {
		VolInf->IsActive = FALSE;
		Result = CloseHandle(VolInf->LogHandle);
		if (SUCCEEDED(Result)) {
			Return = TRUE;
		}
		else {
			printf("Can't close handle\n");
			DisplayError(GetLastError());
		}

	}
	else {
		printf("Can't detach filter\n");
		DisplayError(GetLastError());
	}
	return Return;
}

BOOLEAN
AttachVolume(PLOG_CONTEXT Context, UINT VolInfIndex) {
	BOOLEAN Return = FALSE;
	HRESULT Result = 0;

	WCHAR Temp[] = L"!:\\";
	Temp[0] = Context->Volumes.Data[VolInfIndex].Letter;
	volume_backup_inf* VolInf = &Context->Volumes.Data[VolInfIndex];

	Result = FilterAttach(MINISPY_NAME, Temp, 0, 0, 0);
	if (SUCCEEDED(Result) || Result == ERROR_FLT_INSTANCE_NAME_COLLISION) {
		std::wstring FileName = GenerateMetadataFileName(VolInf);
		VolInf->LogHandle = CreateFileW(FileName.c_str(), GENERIC_WRITE|GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0);
		if (VolInf->LogHandle != INVALID_HANDLE_VALUE) {
			printf("File named %S created\n", FileName.c_str());
			VolInf->IsActive = TRUE;
			Return = TRUE;
		}
		else {
			printf("Can't create file to log volume changes\n");
			DisplayError(GetLastError());
		}

	}
	else {
		printf("Can't attach filter\n");
		DisplayError(GetLastError());
	}

	return Return;
}

BOOLEAN
DiffBackupVolume(PLOG_CONTEXT Context, UINT VolInfIndex) {
	/*
	TODO eğer diffbackup başarılı olmazsa, oluşturulan dosyaları sil ve diffbackup sayısını bir düşür?
	*/
	BOOLEAN ErrorOccured = FALSE;

	data_array<nar_record> DiffRecords = { 0,0 };
	void* Buffer = 0;
	DWORD FileSize = 0;
	HANDLE DiffDataFile = 0;
	HANDLE ShadowHandle = 0;
	HANDLE MftOutput = 0;
	HANDLE MftHandle = 0;
	HANDLE MetadataFileHandle = 0;
	DWORD BytesOperated = 0;
	DWORD BufferSize = 1024 * 1024 * 128; // 128 MB;
	HRESULT Result = 0;
	std::wstring ShadowPath = L"";
	CComPtr<IVssBackupComponents> ptr;
	std::wstring dbfname = L"";
	volume_backup_inf* VolInf = 0;
	std::wstring temp = L"";

	
	VolInf = &Context->Volumes.Data[VolInfIndex];
	VolInf->IsActive = FALSE;
	FileSize = VolInf->ChangeCount * sizeof(nar_record);
	DiffRecords.Data = (nar_record*)malloc(FileSize);
	DiffRecords.Count = FileSize / sizeof(nar_record);

	SetFilePointer(VolInf->LogHandle, 0, 0, FILE_BEGIN);

	Result = ReadFile(VolInf->LogHandle, DiffRecords.Data, FileSize, &BytesOperated, 0);
	if (!SUCCEEDED(Result) || FileSize != BytesOperated) {
		printf("Cant read difmetadata \n");
		printf("Filesize-> %d, BytesOperated -> %d", FileSize, BytesOperated);
		DisplayError(GetLastError());
		goto D_Cleanup;
	}

	if (!DetachVolume(Context, VolInfIndex)) {
		printf("Detach volume failed\n");
		goto D_Cleanup;
	}
	printf("Filtering stopped!\n");

	//temp = GenerateMetadataFileName(VolInf).c_str();
	//MetadataFileHandle = CreateFileW(
	//	temp.c_str(),
	//	GENERIC_READ,
	//	0, 0, OPEN_EXISTING, 0, 0
	//);
	//if (MetadataFileHandle == INVALID_HANDLE_VALUE) {
	//	printf("Cant open metadata file, %S\n", temp.c_str());
	//	goto D_Cleanup;
	//}
	//
	//FileSize = GetFileSize(MetadataFileHandle, 0);
	//DiffRecords.Data = (nar_record*)malloc(FileSize);
	//DiffRecords.Count = FileSize / sizeof(nar_record);
	//if (FileSize == 0 || FileSize % sizeof(nar_record) != 0) {
	//	printf("Filesize was zero or not multiple of sizeof(nar_record)\n");
	//	printf("Filesize => %d", FileSize);
	//	goto D_Cleanup;
	//}
	//
	//Result = ReadFile(MetadataFileHandle, DiffRecords.Data, FileSize, &BytesOperated, 0);
	//if (!SUCCEEDED(Result) || BytesOperated != FileSize) {
	//	printf("Cant read difmetadata \n");
	//	DisplayError(GetLastError());
	//	goto D_Cleanup;
	//}
	//CloseHandle(MetadataFileHandle);
	
	WCHAR Temp[] = L"!:\\";
	Temp[0] = Context->Volumes.Data[VolInfIndex].Letter;

	ShadowPath = GetShadowPath(Temp, ptr);

	VolInf->DiffBackupCount++;
	if (!AttachVolume(Context, VolInfIndex)) {
		printf("Cant attach volume\n");
		goto D_Cleanup;
	}

	printf("Filtering started again!\n");
	

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

	data_array<nar_record> MergedDiffRecords = { 0,0 };
	MergedDiffRecords.Data = (nar_record*)malloc(DiffRecords.Count * sizeof(nar_record));

	UINT32 MergedRecordsIndex = 0;
	UINT32 CurrentIter = 0;

	for (;;) {
		if (CurrentIter == DiffRecords.Count - 1) {
			//last record in the array
			break;
		}

		MergedDiffRecords.Data[MergedRecordsIndex].StartPos = DiffRecords[CurrentIter].StartPos;

		ULONGLONG EndPointTemp = 0;

		while (DiffRecords.Data[CurrentIter].StartPos == DiffRecords.Data[CurrentIter + 1].StartPos
			|| IsRegionsCollide(&DiffRecords.Data[CurrentIter], &DiffRecords.Data[CurrentIter + 1])) {
			ULONGLONG EP1 = DiffRecords.Data[CurrentIter].StartPos + DiffRecords.Data[CurrentIter].Len;
			ULONGLONG EP2 = DiffRecords.Data[CurrentIter + 1].StartPos + DiffRecords.Data[CurrentIter + 1].Len;

			EndPointTemp = MAX(EP1, EP2);

			CurrentIter++;
		}

		MergedDiffRecords.Data[MergedRecordsIndex].Len = EndPointTemp - MergedDiffRecords.Data[MergedRecordsIndex].StartPos;
		MergedRecordsIndex++;
		CurrentIter++;
	}

	MergedDiffRecords.Count = MergedRecordsIndex;
	realloc(MergedDiffRecords.Data, MergedDiffRecords.Count * sizeof(MergedDiffRecords.Data[0]));

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

	dbfname = GenerateDBFileName(&Context->Volumes.Data[VolInfIndex]);
	DiffDataFile = CreateFileW(
		dbfname.c_str(),
		GENERIC_WRITE,
		0, 0,
		CREATE_ALWAYS, 0, 0
	);
	if (DiffDataFile == INVALID_HANDLE_VALUE) {
		printf("Can not create file to store diff disk data, %S\n",dbfname.c_str());
		DisplayError(GetLastError());
		goto D_Cleanup;
	}

	Buffer = malloc(BufferSize);
	if (Buffer == NULL) {
		goto D_Cleanup;
	}

	DWORD ClusterSize = 512 * 8;
	LARGE_INTEGER NewFilePointer;
	NewFilePointer.QuadPart = 0;

	printf("Saving differential state of the disk, please do not close the program\n");
	for (UINT i = 0; i < DiffRecords.Count; i++) {
		LARGE_INTEGER MoveTo;
		DWORD OperationSize = DiffRecords.Data[i].Len * ClusterSize;
		MoveTo.QuadPart = ClusterSize * DiffRecords.Data[i].StartPos;
		Result = SetFilePointerEx(ShadowHandle, MoveTo, &NewFilePointer, FILE_BEGIN);
		if (!SUCCEEDED(Result) || NewFilePointer.QuadPart != MoveTo.QuadPart) {
			printf("Failed to set file pointer..\n");
			DisplayError(GetLastError());
			goto D_Cleanup;
		}

		if (DiffRecords.Data[i].Len > BufferSize / ClusterSize) {
			printf("Operation length is bigger than buffer size, program can not execute this for demo (OperationLen in # clusters => %I64d)..\n", DiffRecords.Data[i].Len);
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
	ErrorOccured = TRUE;
D_Cleanup:
	CLEANMEMORY(Buffer);
	CLEANMEMORY(DiffRecords.Data);

	CLEANHANDLE(DiffDataFile);
	CLEANHANDLE(ShadowHandle);
	CLEANHANDLE(MftOutput);
	CLEANHANDLE(MftHandle);
	
	return ErrorOccured;
}

BOOLEAN
FullBackupVolume(PLOG_CONTEXT Context, UINT VolInfIndex) {
	BOOLEAN Return = FALSE;
	HRESULT Result = 0;
	VOLUME_BITMAP_BUFFER* Bitmap = 0;
	UINT* ClusterIndices = 0;
	void* DataBuffer = 0;

	HANDLE DiskHandle = 0;
	HANDLE OutputHandle = 0;
	HANDLE MetadataFileHandle = 0;
	std::wstring ShadowPath;
	// TODO check if VSS requests could be filtered with driver with some kind of flag
	printf("Waiting VSS!\n");
	CComPtr<IVssBackupComponents> ptr;
	std::wstring fbmetadatafname;
	std::wstring FBFname = L"";
	ShadowPath = L"!:\\";
	ShadowPath[0] = Context->Volumes.Data[VolInfIndex].Letter;

	ShadowPath = GetShadowPath(ShadowPath.c_str(), ptr);

	AttachVolume(Context, VolInfIndex);

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
	FBFname =GenerateFBFileName(&Context->Volumes.Data[VolInfIndex]);
	OutputHandle = CreateFileW(
		FBFname.c_str(),
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		0, 0
	);
	if (OutputHandle == INVALID_HANDLE_VALUE) {
		printf("Could not create file %S\n",FBFname.c_str());
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
		if (!SUCCEEDED(Result) || BytesTransferred != ClusterSize) {
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
	printf("Disk stated saved to file %S\n",FBFname.c_str());
	// save bitmap info
	
	fbmetadatafname = GenerateFBMetadataFileName(&Context->Volumes.Data[VolInfIndex]);
	MetadataFileHandle = CreateFileW(
		fbmetadatafname.c_str(),
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

	Return = TRUE;
F_Cleanup:
	CLEANMEMORY(Bitmap);
	CLEANMEMORY(ClusterIndices);
	CLEANMEMORY(DataBuffer);
	printf("Memory freed\n");

	CLEANHANDLE(DiskHandle);
	CLEANHANDLE(OutputHandle);
	CLEANHANDLE(MetadataFileHandle);
	printf("Handles freed\n");
	return Return;
}



/*
Change RestoreTarget to some struct, containing information needed for restore operation
*/
BOOLEAN
RestoreVolume(PLOG_CONTEXT Context, UINT VolInfIndex, WCHAR RestoreTargetLetter) {
	BOOLEAN Return = FALSE;
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
	data_array<UINT> FBClusterIndices = { 0,0 };
	data_array<nar_record> DBRecords = { 0, 0 };

	std::wstring dbfname = L"";
	std::wstring fbfname = L"";
	std::wstring fbmdfname = L"";
	std::wstring DMDFileName = L"";
	void* Buffer = malloc(BufferSize);

	if (!DetachVolume(Context, VolInfIndex)) {
		printf("Can't detach volume\n");
		goto R_Cleanup;
	}

	if (Buffer == NULL) {
		printf("Can't allocate buffer\n");
		DisplayError(GetLastError());
		goto R_Cleanup;
	}

	//Opening handles first
	fbfname = GenerateFBFileName(&Context->Volumes.Data[VolInfIndex]);
	FBackupFile = CreateFileW(
		fbfname.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		0, OPEN_EXISTING, 0, 0
	);
	if (FBackupFile == INVALID_HANDLE_VALUE) {
		printf("Can't open fullbackup file for restore operation\n");
		DisplayError(GetLastError());
		goto R_Cleanup;
	}

	fbmdfname = GenerateFBMetadataFileName(&Context->Volumes.Data[VolInfIndex]);
	FBackupMDFile = CreateFileW(
		fbmdfname.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		0, OPEN_EXISTING, 0, 0
	);
	if (FBackupMDFile == INVALID_HANDLE_VALUE) {
		printf("Can't open fullbackup metadata file for restore operation\n");
		DisplayError(GetLastError());
		goto R_Cleanup;
	}

	dbfname = GenerateDBFileName(&Context->Volumes.Data[VolInfIndex]);
	DBackupFile = CreateFileW(
		dbfname.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ,
		0, OPEN_EXISTING, 0, 0
	);
	if (DBackupFile == INVALID_HANDLE_VALUE) {
		printf("Can't open diffbackup file for restore operation\n");
		DisplayError(GetLastError());
		goto R_Cleanup;
	}

	DMDFileName = GenerateMetadataFileName(&Context->Volumes.Data[VolInfIndex]);

	DBackupMDFile = CreateFileW(
		DMDFileName.c_str(),
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
	VTRNameTemp[lstrlenW(VTRNameTemp) - 2] = RestoreTargetLetter;

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
		printf("Can't open volume to restore %S\n", VTRNameTemp);
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
	FBClusterIndices.Data = (UINT*)malloc(FileSize);
	FBClusterIndices.Count = FileSize / sizeof(UINT);
	Result = ReadFile(FBackupMDFile, FBClusterIndices.Data, FileSize, &BytesOperated, 0);
	if (!SUCCEEDED(Result) || BytesOperated != FileSize) {
		printf("Error occured while reading diff metadata file.. \n");
		printf("BytesRead => %d\n", BytesOperated);
		DisplayError(GetLastError());
		goto R_Cleanup;
	}


	LARGE_INTEGER MoveTo;
	LARGE_INTEGER NewFilePointer;
	MoveTo.QuadPart = 0;
	NewFilePointer.QuadPart = 0;

	for (UINT i = 0; i < FBClusterIndices.Count; i++) {
		MoveTo.QuadPart = ClusterSize * FBClusterIndices.Data[i];
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
	DBRecords.Data = (nar_record*)malloc(FileSize);
	DBRecords.Count = FileSize / sizeof(nar_record);

	Result = ReadFile(DBackupMDFile, DBRecords.Data, FileSize, &BytesOperated, 0);
	if (!SUCCEEDED(Result) || BytesOperated != FileSize) {
		printf("Can't read diff metadata\n");
		DisplayError(GetLastError());
		goto R_Cleanup;
	}
	//printf("Read-> %d\n", BytesOperated);
	//printf("FileSize was-> %d\n", FileSize);


	MoveTo.QuadPart = 0;
	NewFilePointer.QuadPart = 0;
	for (UINT i = 0; i < DBRecords.Count; i++) {
		MoveTo.QuadPart = DBRecords.Data[i].StartPos * ClusterSize;
		Result = SetFilePointerEx(VolumeToRestore, MoveTo, &NewFilePointer, FILE_BEGIN);
		if (!SUCCEEDED(Result) || NewFilePointer.QuadPart != MoveTo.QuadPart) {
			printf("SetFilePointerEx failed\n");
			printf("Iter was -> %i\n", i);
			DisplayError(GetLastError());
			goto R_Cleanup;
		}

		DWORD ReadSize = DBRecords.Data[i].Len * ClusterSize;
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

	Return = TRUE;
R_Cleanup:
	printf("Cleaning up restore operation..\n");
	CLEANHANDLE(FBackupFile);
	CLEANHANDLE(FBackupMDFile);
	CLEANHANDLE(DBackupFile);
	CLEANHANDLE(DBackupMDFile);
	CLEANHANDLE(VolumeToRestore);

	CLEANMEMORY(Buffer);
	CLEANMEMORY(FBClusterIndices.Data);
	CLEANMEMORY(DBRecords.Data);

	printf("Cleaned!\n");
	return Return;
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
) {

	/*
	minispy.exe -From -To  one letter

ex usage:
minispy.exe D E
track D: volume, restore to E: volume if requested
*/
	/*
	WCHAR FromVolLetter = 0;
	WCHAR ToVolLetter = 0;
	if (argc == 3) {
		FromVolLetter = argv[1][0];
		ToVolLetter = argv[2][0];
	}
	else if (argc == 2) {
		printf("Connecting to filter's port...\n");
		HANDLE Port;

		HRESULT Result = FilterConnectCommunicationPort(MINISPY_PORT_NAME,
			0,
			NULL,
			0,
			NULL,
			&Port);

		if (!SUCCEEDED(Result)) {
			printf("Cant create communication port\n");
			DisplayError(GetLastError());
			goto Arg_Cleanup;
		}

		WCHAR TempV[] = L" :\\";
		TempV[0] = argv[1][0];
		Result = FilterDetach(MINISPY_NAME, TempV, 0);
		if (!SUCCEEDED(Result)) {
			printf("Cant detach volume %S\n", TempV);
			DisplayError(GetLastError());
			goto Arg_Cleanup;
		}

	Arg_Cleanup:
		CLEANHANDLE(Port);

	}
	else {
		printf("Invalid usage!\n");
		return 0;
	}
	WCHAR TrackedVolumeName[] = L" :\\";
	WCHAR RestoreVolumeName[] = L" :\\";
	TrackedVolumeName[0] = FromVolLetter;
	RestoreVolumeName[0] = ToVolLetter;
	printf("Programm will backup volume %S, and restore it to %S if requested\n", TrackedVolumeName, RestoreVolumeName);

	*/
	WCHAR TrackedVolumeName[] = L"ASDFASF";
	WCHAR RestoreVolumeName[] = L"ASDFASDFAS";

	
	HANDLE port = INVALID_HANDLE_VALUE;
	HRESULT hResult = S_OK;
	DWORD result;
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
	context.Thread = 0;
	context.Volumes = { 0,0 };
	//
	// Create the thread to read the log records that are gathered
	// by MiniSpy.sys.
	//

	NarCreateThreadCom(&context);

#if 1
#pragma region NAR_TEST
	WCHAR instanceName[INSTANCE_NAME_MAX_CHARS + 1];

#pragma endregion
#endif 
	UINT NDiffBackedUp = 0;
	printf("Volume adding to track list\n");
	AddVolumeToTrack(&context, 'E');

	printf("Press [q] to quit\n");
	printf("Press [f] to get full backup of the volume, [d] to diff, [r] to restore\n");
	while (inputChar = (CHAR)getchar()) {
		if (inputChar == 'q' || inputChar == 'Q') {
			goto Main_Cleanup;
		}
		else if (inputChar == 'd' || inputChar == 'D') {
			DiffBackupVolume(&context, 0); //Hard coded
		}
		else if (inputChar == 'f' || inputChar == 'F') {
			FullBackupVolume(&context, 0);
		}
		else if (inputChar == 'r' || inputChar == 'R') {
			RestoreVolume(&context, 0,'E');
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


Main_Exit:

	//
	// Clean up the data that is always around and exit
	//

	if (context.ShutDown) {

		CloseHandle(context.ShutDown);
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