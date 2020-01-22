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

#define Kilobytes(val) ((val)*1024)
#define Megabytes(val) (Kilobytes(1024)*(val))
#define Gigabytes(val) (Megabytes(1024)*(uint64_t)(val))

#define DI_DEVELOPER 1


#if DI_SLOW
#define Assert(expression) if(!(expression)) {*((int*)0) = 0;}
#else
#define Assert(expression)
#endif

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


struct debug_read_file {
	ULONGLONG Size;
	void* Contents;
};

struct used_disk_space_info{
	ULONGLONG Offset;
	ULONGLONG Len;
};

std::wstring GetShadowPath(std::wstring Drive, CComPtr<IVssBackupComponents>& ptr);

std::vector<used_disk_space_info> BackupVolume(std::wstring VolumeName, std::wstring OutputPath);
void RestoreVolume(std::wstring VolumeName, std::wstring Source, std::vector<used_disk_space_info> DiskInfo);

bool CreateMetaDataFile(std::vector<used_disk_space_info> DiskInfo, char FilePath[]);
bool ReadMetaDataFile(std::vector<used_disk_space_info>& DiskInfo, char FilePath[]);

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
	char MetaDataFilePath[] = "MetaData.bin";

	std::wcout << std::wstring(argv[2]) << "\n";

	if (std::wstring(argv[1]) == L"Backup") {
		auto GeneratedClusterIndices = BackupVolume(Drive, TargetPath);
		bool Result = CreateMetaDataFile(GeneratedClusterIndices, MetaDataFilePath);
		Assert(Result == true);
	}
	else if (std::wstring(argv[1]) == L"Restore") {
		std::vector<used_disk_space_info> Metadata;
		Metadata.reserve(Kilobytes(1)); //1024 elements, not 1KB
		bool Result = ReadMetaDataFile(Metadata, MetaDataFilePath);
		Assert(Result == true);
		RestoreVolume(Drive, TargetPath, Metadata);
	}
	
	//RestoreVolume(var);

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


std::vector<used_disk_space_info>
BackupVolume(std::wstring VolumeName, std::wstring OutputPath) {
	DWORD BufferSize = Megabytes(64);
	BOOL Result;

	CComPtr<IVssBackupComponents> ptr;
	
	std::wstring Path;

	
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
	std::vector<used_disk_space_info> UsedDiskInfo;
	UsedDiskInfo.reserve(Kilobytes(1));

	{
		//Bitwise hack to find # of clusters used on the volume
		//To speed this up, I can use hammingway algorithm to calculate # of bits set in given integer.
		//Maybe I can find a way to optimise this with SIMD instructions
		std::vector<ULONGLONG> ClusterIndices;
		ClusterIndices.reserve(Kilobytes(1)); // 1024 Elements, not 1KB

		std::ofstream BenchmarkFile("Benchmark.csv", std::ios::binary | std::ios::out);
		Assert(BenchmarkFile.is_open());

		LARGE_INTEGER BitMaskStart = GetClock();

		
		float One = (1 << 20);

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

		DWORD Last = 0;


		LARGE_INTEGER ParseStart = GetClock();

		for (const auto& Iter : ClusterIndices) {
			if (Iter == (Last + 1)) {
				UsedDiskInfo.back().Len += ClusterSize;
			}
			else {
				UsedDiskInfo.push_back({ Iter * ClusterSize,ClusterSize });
			}
			Last = Iter;
		}
		
		LARGE_INTEGER ParseEnd = GetClock();

		double Elapsed = 0.0;
		Elapsed = GetMSElapsed(BitMaskStart,BitMaskEnd);
		BenchmarkFile << "Bitmap mask(ms) ," << Elapsed << "\n";
		Elapsed = GetMSElapsed(ParseStart,ParseEnd);
		BenchmarkFile << "Parse(ms) ," << Elapsed << "\n";
		BenchmarkFile.close();

		std::vector<used_disk_space_info> V;
		std::cout << UsedDiskInfo.size();
		std::cout << MaxClusterCount << "\n";
	} //IF BITMAP

	
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

	int64_t FileBufferSize = Megabytes(2);
	void* DataBuffer = malloc(FileBufferSize);

	for (const auto& Index : UsedDiskInfo) {
		DWORD BytesTransferred = 0;
		//TODO use databuffer with 64MB ceil val, rather than constructing it every time
		if (Index.Len > FileBufferSize) {

			DWORD TempBuffSize = FileBufferSize;
			__int64 TempTotalRead = 0;
			
			Result = SetFilePointer(DiskHandle, Index.Offset, NULL, FILE_BEGIN);
			Assert(Result == Index.Offset);

			while(TempTotalRead + TempBuffSize < Index.Len) {
				Result = ReadFile(DiskHandle, DataBuffer, TempBuffSize, &BytesTransferred, 0);
				Assert(Result == TRUE);
				Assert(BytesTransferred == TempBuffSize);

				Result = WriteFile(OutputHandle, DataBuffer, TempBuffSize, &BytesTransferred, 0);
				Assert(Result == TRUE);
				Assert(BytesTransferred == TempBuffSize);

				TempTotalRead += TempBuffSize;
			}

			TempBuffSize = Index.Len - TempTotalRead;
			
			Result = ReadFile(DiskHandle, DataBuffer, TempBuffSize, &BytesTransferred, 0);
			Assert(Result == TRUE);
			Assert(BytesTransferred == TempBuffSize);

			Result = WriteFile(OutputHandle, DataBuffer, TempBuffSize, &BytesTransferred, 0);
			Assert(Result == TRUE);
			Assert(BytesTransferred == TempBuffSize);

		}
		else {		
			Result = SetFilePointer(DiskHandle, Index.Offset, NULL, FILE_BEGIN);
			Assert(Result == Index.Offset);

			Result = ReadFile(DiskHandle, DataBuffer, Index.Len, &BytesTransferred, 0);
			Assert(Result == TRUE);
			Assert(BytesTransferred == Index.Len);

			Result = WriteFile(OutputHandle, DataBuffer, Index.Len, &BytesTransferred, 0);
			Assert(Result == TRUE);
			Assert(BytesTransferred == Index.Len);
		}
	}


	CloseHandle(DiskHandle);
	CloseHandle(OutputHandle);
	free(DataBuffer);
	std::cout << GetLastError() << "\n";
	
	return UsedDiskInfo;
}

void 
RestoreVolume(std::wstring VolumeName, std::wstring Source, std::vector<used_disk_space_info> ClusterIndices) {
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
	
	int64_t FileBufferSize = Megabytes(2);
	void* DataBuffer = malloc(FileBufferSize);
	
	for (const auto& Index : ClusterIndices) {
		DWORD BytesTransferred = 0;

		if (Index.Len > FileBufferSize) {

			DWORD TempBuffSize = FileBufferSize;
			__int64 TempTotalRead = 0;
			Result = SetFilePointer(RestoreTargetHandle, Index.Offset, NULL, FILE_BEGIN);
			Assert(Result == Index.Offset);

			while (TempTotalRead + TempBuffSize < Index.Len) {
				Result = ReadFile(SourceHandle, DataBuffer, TempBuffSize, &BytesTransferred, 0);
				Assert(Result == TRUE);
				Assert(BytesTransferred == TempBuffSize);

				Result = WriteFile(RestoreTargetHandle, DataBuffer, TempBuffSize, &BytesTransferred, 0);
				Assert(Result == TRUE);
				Assert(BytesTransferred == TempBuffSize);

				TempTotalRead += TempBuffSize;
			}

			TempBuffSize = Index.Len - TempTotalRead;
			
			Result = ReadFile(SourceHandle, DataBuffer, TempBuffSize, &BytesTransferred, 0);
			Assert(Result == TRUE);
			Assert(BytesTransferred == TempBuffSize);

			Result = WriteFile(RestoreTargetHandle, DataBuffer, TempBuffSize, &BytesTransferred, 0);
			Assert(Result == TRUE);
			Assert(BytesTransferred == TempBuffSize);
		}
		else {

			Result = SetFilePointer(RestoreTargetHandle, Index.Offset, NULL, FILE_BEGIN);
			Assert(Result == Index.Offset);

			Result = ReadFile(SourceHandle, DataBuffer, Index.Len, &BytesTransferred, 0);
			Assert(Result == TRUE);
			Assert(BytesTransferred == Index.Len);

			Result = WriteFile(RestoreTargetHandle, DataBuffer, Index.Len, &BytesTransferred, 0);
			Assert(Result == TRUE);
			Assert(BytesTransferred == Index.Len);

		}
	}

	free(DataBuffer);

}

bool 
CreateMetaDataFile(std::vector<used_disk_space_info> UsedDiskInfo, char FilePath[]) {
	
	std::ofstream File(FilePath,std::ios::binary|std::ios::out);
	
	if (!File.is_open()) {
		std::wcout << L"Couldn't create metadatafile ";
		return false;
	}

	for (const auto& var : UsedDiskInfo) {
		File << var.Offset << " " << var.Len << "\n";
	}
	File.close();

	return true;
}

bool
ReadMetaDataFile(std::vector<used_disk_space_info>& UsedDiskInfo, char FilePath[]){
	std::ifstream File(FilePath,std::ios::binary);
	if (!File.is_open()) {
		std::wcout << L"Couldn't read metadatafile\n";
		return false;
	}

	std::string F;
	while (std::getline(File, F)) {
		size_t SpacePos = F.find(" ");

		ULONGLONG Offset = std::atoll(F.substr(0,SpacePos).c_str());
		ULONGLONG Len = std::atoll(F.substr(SpacePos + 1, F.size()).c_str());

		UsedDiskInfo.push_back({ Offset,Len });
	}
	File.close();
	return true;
}
