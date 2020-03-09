//#define _WIN32_WINNT 0x0501

#include <iostream>
#include <cstdint>

#include <windows.h>
#include <functional>
#include <memory>
#include <string>
#include <fstream>
#include <filesystem>

#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <vsmgmt.h>

// VDS includes
#include <vds.h>

// ATL includes
#include <atlbase.h>


#include <fileapi.h>
#include "DImage.h"

#define Kilobytes(val) ((val)*1024)
#define Megabytes(val) (Kilobytes(1024)*(val))
#define Gigabytes(val) (Megabytes(1024)*(uint64_t)(val))

#define DI_DEVELOPER 1


#if DI_SLOW
#define Assert(expression) if(!(expression)) {*((int*)0) = 0;}
#else
#define Assert(expression)
#endif

#define DI_DEVELOPER 1
#if DI_DEVELOPER
LONGLONG GlobalPerfCountFrequency;

inline LARGE_INTEGER
GetClock() {
	LARGE_INTEGER Result;

	QueryPerformanceCounter(&Result);
	return Result;
}

inline double
GetMSElapsed(LARGE_INTEGER Start, LARGE_INTEGER End) {
	Assert(GlobalPerfCountFrequency != 0);
	double Result = (double)(End.QuadPart - Start.QuadPart) / (double)(GlobalPerfCountFrequency);
	return Result;
}
#endif


int wmain(int argc, wchar_t **argv) {

#pragma region INIT

	HRESULT rr = 0;//::CoInitializeEx(NULL, COINIT_MULTITHREADED);

	rr = CoInitialize(NULL);
	Assert(!rr);

	rr = CoInitializeSecurity(
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
	Assert(!rr);
	
#pragma endregion

	//TODO parse arg list
	if (argc != 4) {
		std::cout<<"Program creates some log files in order to restore volumes."<<std::endl;
		std::cout<<"Please do not delete anything it creates or you can not restore previous state."<<std::endl;
		std::cout<<"Usage:"<<std::endl;
		std::cout<<"[operation] [volume] [path to copy contents]"<<std::endl;
		std::cout<<"-Backup   D:\\ PathToBackupVolume"<<std::endl;
		std::cout<<"-Restore  D:\\ PathToBackupFile"<<std::endl;
		std::cout<<"Ex:"<<std::endl;
		std::cout<<"-Backup  D:\\ D:\\workspace\\dimage.bin"<<std::endl;
		std::cout<<"-Restore D:\\ D:\\workspace\\dimage.bin"<<std::endl;
		return 1;
	}

	
#if DI_DEVELOPER
	LARGE_INTEGER PerfCountFreqResult;
	QueryPerformanceFrequency(&PerfCountFreqResult);
	GlobalPerfCountFrequency = PerfCountFreqResult.QuadPart;
#endif
	
	std::ios_base::sync_with_stdio(false);
	std::wstring Drive      = argv[2];
	std::wstring TargetPath = argv[3];
	char MetaDataFilePath[] = "HashData.bin";

	cluster_hash_map HashMap;
	ReadMetaDataHash(HashMap, MetaDataFilePath);
	FindDiff(Drive, HashMap);
	return 0;

	if (std::wstring(argv[1]) == L"Backup") {
		cluster_hash_map GeneratedClusterIndices = BackupVolume(Drive, TargetPath);
	}
	else if (std::wstring(argv[1]) == L"Restore") {
		linear_cluster_map ClusterMap;
		ReadMetaDataLinear(ClusterMap,MetaDataFilePath);
		RestoreVolume(Drive, TargetPath, ClusterMap);
	}
	
	return 1;
}


std::wstring 
GetShadowPath(std::wstring Drive, CComPtr<IVssBackupComponents>& ptr) {
	VSS_ID sid;
	HRESULT res;

	CreateVssBackupComponents(&ptr);

	res = ptr->InitializeForBackup();
	Assert(res == S_OK);
	res = ptr->SetContext(VSS_CTX_BACKUP);
	Assert(res == S_OK);
	res = ptr->StartSnapshotSet(&sid);
	Assert(res == S_OK);
	res = ptr->SetBackupState(false, false, VSS_BACKUP_TYPE::VSS_BT_FULL, false);
	Assert(res == S_OK);
	res = ptr->AddToSnapshotSet((LPWSTR)Drive.c_str(), GUID_NULL, &sid); // C:\\ ex
	Assert(res == S_OK);

	{
		CComPtr<IVssAsync> Async;
		res = ptr->PrepareForBackup(&Async);
		Assert(res == S_OK);
		Async->Wait();
	}

	{
		CComPtr<IVssAsync> Async;
		res = ptr->DoSnapshotSet(&Async);
		Assert(res == S_OK);
		Async->Wait();
	}

	VSS_SNAPSHOT_PROP SnapshotProp;
	ptr->GetSnapshotProperties(sid, &SnapshotProp);
	std::wstring ShadowPath = std::wstring(SnapshotProp.m_pwszSnapshotDeviceObject);

	return ShadowPath;
}


cluster_hash_map
BackupVolume(std::wstring VolumeName, std::wstring OutputPath) {
	DWORD BufferSize = Megabytes(64);
	BOOL Result;
	CComPtr<IVssBackupComponents> ptr;
	std::wstring Path;
	cluster_hash_map ClusterHashMap;
	Path = GetShadowPath(VolumeName,ptr);

	HANDLE DiskHandle = CreateFileW(
		Path.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
	);
	
	Assert(DiskHandle != INVALID_HANDLE_VALUE);


	STARTING_LCN_INPUT_BUFFER StartingLCN;
	VOLUME_BITMAP_BUFFER* Bitmap = NULL;
	DWORD BytesReturned;
	
	ULONGLONG MaxClusterCount;
	BufferSize = Megabytes(64);

	StartingLCN.StartingLcn.QuadPart = 0;
	Bitmap = (VOLUME_BITMAP_BUFFER*)malloc(BufferSize);
	Assert(Bitmap);

	Result = DeviceIoControl(
		DiskHandle,
		FSCTL_GET_VOLUME_BITMAP,
		&StartingLCN,
		sizeof(StartingLCN),
		Bitmap,
		BufferSize,
		&BytesReturned,
		NULL
	);
	if (Result == FALSE) {
		DWORD err = GetLastError();
		std::cout << err << "\n";
		Assert(false);
	}
	
	MaxClusterCount = Bitmap->BitmapSize.QuadPart;
	DWORD ClustersRead = 0;

	unsigned char* BitmapIndex = Bitmap->Buffer;

	unsigned __int64 UsedClusterCount = 0;
	unsigned char BitmapMask = 1;

	DWORD ToMove = BufferSize;
	std::cout << sizeof(LARGE_INTEGER) << "\n";
	DWORD ClusterSize = 8 * 512;
	
	std::vector<uint32_t> ClusterIndices;
	ClusterIndices.reserve(MaxClusterCount);
	
	std::ofstream BenchmarkFile("Benchmark.csv", std::ios::binary | std::ios::out);
	Assert(BenchmarkFile.is_open());

	LARGE_INTEGER BitMaskStart = GetClock();

	Assert(MaxClusterCount < UINT32_MAX);
	while (ClustersRead < MaxClusterCount) {
		if ((*BitmapIndex & BitmapMask) == BitmapMask) {
			ClusterIndices.push_back(ClustersRead);
			UsedClusterCount++;
		}
		BitmapMask <<= 1;
		if (BitmapMask == 0) {
			BitmapMask = 1;
			BitmapIndex++;
		}
		ClustersRead++;
	}
		
	LARGE_INTEGER BitMaskEnd = GetClock();

	ClusterIndices.shrink_to_fit();
	free(Bitmap);

	double Elapsed = 0.0;
	Elapsed = GetMSElapsed(BitMaskStart,BitMaskEnd);
	BenchmarkFile << "Bitmap mask(ms) ," << Elapsed << "\n";
	BenchmarkFile.close();

	std::cout << MaxClusterCount << "\n";
	
	
	HANDLE OutputHandle = CreateFileW(
		OutputPath.c_str(),
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		NULL,
		NULL
	);
	Assert(OutputHandle != INVALID_HANDLE_VALUE);

	HANDLE HashFileHandle = CreateFile(
		"HashData.bin",
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		NULL,
		NULL
	);
	Assert(HashFileHandle != INVALID_HANDLE_VALUE);

	if (!DeviceIoControl(
		DiskHandle,
		FSCTL_LOCK_VOLUME,
		NULL,
		0,
		NULL,
		0,
		NULL,
		NULL
	)) {
		std::cout << GetLastError() << "\n";
		Assert(false);
	}

	
	int64_t FileBufferSize = ClusterSize;
	void* DataBuffer = malloc(FileBufferSize);
	DWORD BytesTransferred = 0;
	
	LARGE_INTEGER NewFilePointer;
	NewFilePointer.QuadPart = 0;

	char TextBuffer[256];
	int TBLen = 0;
	XXH64_hash_t HashResult = 0;
	for (const auto& Index : ClusterIndices) {
		LARGE_INTEGER MoveTo;
		MoveTo.QuadPart = ClusterSize * Index;

		Result = SetFilePointerEx(DiskHandle, MoveTo, &NewFilePointer, FILE_BEGIN);
		Assert(Result != FALSE);
		Assert(NewFilePointer.QuadPart == MoveTo.QuadPart);

		Result = ReadFile(DiskHandle, DataBuffer, ClusterSize, &BytesTransferred, 0);
		Assert(Result == TRUE);
		Assert(BytesTransferred == ClusterSize);

		Result = WriteFile(OutputHandle, DataBuffer, ClusterSize, &BytesTransferred, 0);
		Assert(Result == TRUE);
		Assert(BytesTransferred == ClusterSize);

		HashResult = XXH3_64bits(DataBuffer, ClusterSize);
		sprintf(TextBuffer, "%i %llu\n", Index, HashResult);
		TBLen = strlen(TextBuffer);
		Result = WriteFile(HashFileHandle, TextBuffer, TBLen, &BytesTransferred, 0);
		Assert(Result == TRUE);
		Assert(BytesTransferred == TBLen);

		ClusterHashMap[Index] = HashResult;
	}
	
	CloseHandle(DiskHandle);
	CloseHandle(OutputHandle);
	CloseHandle(HashFileHandle);
	free(DataBuffer);
	std::cout << GetLastError() << "\n";
	
	return ClusterHashMap;
}



void 
RestoreVolume(std::wstring VolumeName, std::wstring Source, const linear_cluster_map& LinearClusterMap) {
	BOOL Result;
	std::wstring DumpVolName = L"\\\\.\\" + VolumeName.substr(0, 2);
	HANDLE RestoreTargetHandle = CreateFileW(
		DumpVolName.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		OPEN_ALWAYS,
		0,
		NULL
	);
	Assert(RestoreTargetHandle != INVALID_HANDLE_VALUE);

	HANDLE SourceHandle = CreateFileW(
		Source.c_str(),
		GENERIC_READ,
		0,
		NULL,
		OPEN_ALWAYS,
		NULL,
		NULL
	);
	Assert(SourceHandle != INVALID_HANDLE_VALUE);


	if (!DeviceIoControl(
		RestoreTargetHandle,
		FSCTL_LOCK_VOLUME,
		NULL,
		0,
		NULL,
		0,
		NULL,
		NULL
	)) {
		std::cout << GetLastError() << "\n";
		Assert(false);
	}

	int64_t FileBufferSize = Kilobytes(4); //TODO may change from disk to disk
	int ClusterSize = Kilobytes(4);
	void* DataBuffer = malloc(FileBufferSize);

	LARGE_INTEGER OffsetResult;
	OffsetResult.QuadPart = 0;
	LARGE_INTEGER NewFilePointerVal;
	NewFilePointerVal.QuadPart = 0;
	DWORD BytesTransferred = 0;

	for (const auto& [ClusterIndex,HashValue]: LinearClusterMap) {
		Result = SetFilePointerEx(RestoreTargetHandle, NewFilePointerVal, &OffsetResult, FILE_BEGIN);
		Assert(Result != FALSE);
		Assert(OffsetResult.QuadPart == NewFilePointerVal.QuadPart);

		Result = ReadFile(SourceHandle, DataBuffer, ClusterSize, &BytesTransferred, 0);
		Assert(Result == TRUE);
		Assert(BytesTransferred == ClusterSize);

		Result = ReadFile(RestoreTargetHandle, DataBuffer, ClusterSize, &BytesTransferred, 0);
		Assert(Result == TRUE);
		Assert(BytesTransferred == ClusterSize);
	}

	free(DataBuffer);
	CloseHandle(RestoreTargetHandle);
	CloseHandle(SourceHandle);

}


void 
ReadMetaDataLinear(linear_cluster_map& Result, char FilePath[]){
	std::ifstream File(FilePath,std::ios::binary);

	if (!File.is_open()) {
		std::wcout << L"Couldn't read metadatafile\n";
		return;
	}

	
	std::string Dump;
	Result.reserve(Kilobytes(1024)); // TODO change constant

	while (std::getline(File, Dump)) {
		cluster_tuple Tuple;
		size_t SpacePos = Dump.find(" ");

		// std::atoll(F.substr(SpacePos + 1, F.size()).c_str())
		Tuple.Index = std::atol(Dump.substr(0, SpacePos).c_str());
		Tuple.Hash = std::atoll(Dump.substr(SpacePos + 1, Dump.size()).c_str());

		Result.emplace_back(Tuple);
	}

	Result.shrink_to_fit();
	
	File.close();
}

void
ReadMetaDataHash(cluster_hash_map& Result, char FilePath[]) {
	std::ifstream File(FilePath, std::ios::binary);

	if (!File.is_open()) {
		std::wcout << L"Couldn't read metadatafile\n";
		return;
	}


	std::string Dump;
	Result.reserve(Kilobytes(1024)); // TODO change constant
	uint32_t Index;
	XXH64_hash_t Hash;
	while (std::getline(File, Dump)) {
		cluster_tuple Tuple;
		size_t SpacePos = Dump.find(" ");

		// std::atoll(F.substr(SpacePos + 1, F.size()).c_str())
		Index = std::atol(Dump.substr(0, SpacePos).c_str());
		Hash = std::atoll(Dump.substr(SpacePos + 1, Dump.size()).c_str());

		Result[Index] = Hash;
	}

	File.close();
}

cluster_indices
GetVolumeClusterIndices(std::wstring VolumeName) {
	DWORD BufferSize = 0;
	BOOL Result = FALSE;

	HANDLE DiskHandle = CreateFileW(
		VolumeName.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
	);
	Assert(DiskHandle != INVALID_HANDLE_VALUE);
	
	STARTING_LCN_INPUT_BUFFER StartingLCN;
	VOLUME_BITMAP_BUFFER* Bitmap = NULL;
	DWORD BytesReturned;

	uint32_t MaxClusterCount;
	BufferSize = Megabytes(64);

	StartingLCN.StartingLcn.QuadPart = 0;
	Bitmap = (VOLUME_BITMAP_BUFFER*)malloc(BufferSize);
	Assert(Bitmap);

	Result = DeviceIoControl(
		DiskHandle,
		FSCTL_GET_VOLUME_BITMAP,
		&StartingLCN,
		sizeof(StartingLCN),
		Bitmap,
		BufferSize,
		&BytesReturned,
		NULL
	);
	if (Result == FALSE) {
		DWORD err = GetLastError();
		std::cout << "Couldn't get bitmap infor, MSDN Error code " << err << "\n";
		Assert(false);
	}

	MaxClusterCount = Bitmap->BitmapSize.QuadPart;
	DWORD ClustersRead = 0;

	unsigned char* BitmapIndex = Bitmap->Buffer;

	unsigned __int64 UsedClusterCount = 0;
	unsigned char BitmapMask = 1;

	DWORD ToMove = BufferSize;
	
	cluster_indices ClusterIndices;
	ClusterIndices.reserve(MaxClusterCount);

	std::ofstream BenchmarkFile("Benchmark.csv", std::ios::binary | std::ios::out);
	Assert(BenchmarkFile.is_open());

	Assert(MaxClusterCount < UINT32_MAX);
	while (ClustersRead < MaxClusterCount) {
		if ((*BitmapIndex & BitmapMask) == BitmapMask) {
			ClusterIndices.push_back(ClustersRead);
			UsedClusterCount++;
		}
		BitmapMask <<= 1;
		if (BitmapMask == 0) {
			BitmapMask = 1;
			BitmapIndex++;
		}
		ClustersRead++;
	}
	
	ClusterIndices.shrink_to_fit();
	CloseHandle(DiskHandle);

	free(Bitmap);
	return ClusterIndices;
}


cluster_indices 
FindDiff(std::wstring VolumeName, cluster_hash_map &ClusterHashMap) {
	BOOL Result = FALSE;
	DWORD BytesTransferred = 0;
	static auto IsExistsFn = [&](const cluster_hash_map& Map, uint32_t Key) {return (Map.find(Key) != Map.end()); };
	using namespace std::placeholders;
	auto Exist = std::bind(IsExistsFn, ClusterHashMap, _1);

	CComPtr<IVssBackupComponents> ptr;
	std::wstring ShadowPath = GetShadowPath(VolumeName, ptr);
	cluster_indices CurrentIndices = GetVolumeClusterIndices(ShadowPath);
	cluster_indices DiffIndices;
	DiffIndices.reserve(CurrentIndices.size());
	LARGE_INTEGER NewFilePointer;
	LARGE_INTEGER DistanceToMove;

	HANDLE DiskHandle = CreateFileW(
		ShadowPath.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
	);
	Assert(DiskHandle != INVALID_HANDLE_VALUE);
	
	DWORD ClusterSize = Kilobytes(4);
	XXH64_hash_t HashVal = 0;
	void* DataBuffer = malloc(ClusterSize);

	for (const auto& NewIndex : CurrentIndices) {
		if (!Exist(NewIndex)) {
			DiffIndices.push_back(NewIndex);
			continue;
		}

		DistanceToMove.QuadPart = ClusterSize * NewIndex;

		Result = SetFilePointerEx(DiskHandle, DistanceToMove, &NewFilePointer, FILE_BEGIN);
		Assert(Result != FALSE);
		Assert(DistanceToMove.QuadPart == NewFilePointer.QuadPart);
		
		Result = ReadFile(DiskHandle, DataBuffer, ClusterSize, &BytesTransferred, 0);
		Assert(Result == TRUE);
		Assert(BytesTransferred == ClusterSize);

		HashVal = XXH3_64bits(DataBuffer, ClusterSize);
		if (HashVal != ClusterHashMap[NewIndex]) {
			DiffIndices.push_back(NewIndex);
		}
	}
	
	free(DataBuffer);
	CloseHandle(DiskHandle);
	return DiffIndices;
}


inline cluster_hash_map 
ClusterLinearToHashMap(const linear_cluster_map& LinearClusters) {
	cluster_hash_map Result;
	Result.reserve(LinearClusters.size());

	for (const auto& Indx : LinearClusters) {
		Result[Indx.Index] = Indx.Hash;
	}
	return Result;
}
