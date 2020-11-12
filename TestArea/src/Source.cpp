#include <Windows.h>
#include <stdio.h>
#include <utility>
#include <Windows.h>
#include <rpcdcep.h>
#include <rpcdce.h>
#include <iostream>


class test {
public:
    int pa;

    void print() {
        std::cout << pa << "\n" << pv << "\n";
    }
    
    void set(int x) {
        pv = x;
    }

private:
    int pv;
};


void increment(void* ptr) {
    int* iptr = (int*)ptr;
    *iptr = 250;
}


int foo(test *obj) {


    BOOLEAN Result = FALSE;
    


    
    return 0;
}



static void nlog(const char* str, ...) {  

    const static HANDLE File = CreateFileA("NAR_APP_LOG_FILE.txt", GENERIC_WRITE|GENERIC_READ, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    static SYSTEMTIME Time = {};
    
    va_list ap;
    static char buf[1024];
    static char TimeStrBuf[128];

    memset(&Time, 0, sizeof(Time));
    memset(buf, 0, sizeof(buf));
    memset(TimeStrBuf, 0, sizeof(TimeStrBuf));
    
    GetLocalTime(&Time);
    sprintf(TimeStrBuf, "[%i:%i:%i]: ", Time.wHour, Time.wMinute, Time.wSecond);

    va_start(ap,str);
    vsprintf(buf, str, ap);
    va_end(ap);

    int Len = strlen(buf);
    DWORD H;

    WriteFile(File, TimeStrBuf, strlen(TimeStrBuf), &H, 0);
    WriteFile(File, buf, Len, &H, 0);
    
    FlushFileBuffers(File);

}

//#define printf(fmt, ...)  nlog(fmt, __VA_ARGS__)

struct nar_record{
  UINT32 Start;
  UINT32 Len;
};

struct file_read{
  void *Data;
  DWORD Len;
};

file_read
NarReadFile(const char* FileName) {
    file_read Result = { 0 };
    HANDLE File = CreateFileA(FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if (File != INVALID_HANDLE_VALUE) {
        DWORD BytesRead = 0;
        Result.Len = GetFileSize(File, 0);
        if (Result.Len == 0) return Result;
        Result.Data = malloc(Result.Len);
        
        if (Result.Data) {
            memset(Result.Data, 0, Result.Len);
            ReadFile(File, Result.Data, Result.Len, &BytesRead, 0);
            if (BytesRead == Result.Len) {
                // NOTE success
            }
            else {
                free(Result.Data);
                printf("Read %i bytes instead of %i\n", BytesRead, Result.Len);
            }
        }
        CloseHandle(File);
    }
    else {
        printf("Can't create file: %s\n", FileName);
    }
    //CreateFileA(FNAME, GENERIC_WRITE, 0,0 ,CREATE_NEW, 0,0)
    return Result;
}

HANDLE
NarOpenVolume(char Letter) {
    char VolumePath[512];
    sprintf(VolumePath, "\\\\.\\%c:", Letter);

    HANDLE Volume = CreateFileA(VolumePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
    if (Volume != INVALID_HANDLE_VALUE) {

        if (DeviceIoControl(Volume, FSCTL_LOCK_VOLUME, 0, 0, 0, 0, 0, 0)) {

        }
        else {
            // NOTE(Batuhan): this isnt an error, tho prohibiting volume access for other processes would be great.
            printf("Couldn't lock volume %c, returning INVALID_HANDLE_VALUE\n", Letter);
        }

    }
    else {
        printf("Couldn't open volume %c\n", Letter);
    }

    return Volume;
}


#if 0

/*
  From given volume handle, extracts region information from volume
  Handle can be VSS handle or normal volume handle. VSS is preferred since volume information might change over time
  returns NULL if any error occurs
*/
nar_record*
GetVolumeRegionsFromBitmap(HANDLE VolumeHandle, UINT32* OutRecordCount) {

    if(OutRetCount == NULL) return NULL;

    nar_record* Record = NULL;

    
    UINT* ClusterIndices = 0;

    STARTING_LCN_INPUT_BUFFER StartingLCN;
    StartingLCN.StartingLcn.QuadPart = 0;
    ULONGLONG MaxClusterCount = 0;
    DWORD BufferSize = 1024 * 1024 * 64; //  megabytes
    
    VOLUME_BITMAP_BUFFER* Bitmap = (VOLUME_BITMAP_BUFFER*)malloc(BufferSize);
    
    if (Bitmap != NULL) {
    
        HRESULT R = DeviceIoControl(VolumeHandle, FSCTL_GET_VOLUME_BITMAP, &StartingLCN, sizeof(StartingLCN), Bitmap, BufferSize, 0, 0);
        if (SUCCEEDED(R)) {
            
            MaxClusterCount = Bitmap->BitmapSize.QuadPart;
            DWORD ClustersRead = 0;
            UCHAR* BitmapIndex = Bitmap->Buffer;
            UCHAR BitmapMask = 1;
            UINT32 CurrentIndex = 0;
            UINT32 LastActiveCluster = 0;
            
            UINT32 RecordBufferSize = 128 * 1024 * 1024;
            Records = (nar_record*)malloc(RecordBufferSize);
            UINT32 RecordCount = 0;

            memset(Records, 0, RecordBufferSize);

            Records[0] = { 0,1 };
            RecordCount++; 

            ClustersRead++;
            BitmapMask <<= 1;

            while (ClustersRead < MaxClusterCount) {
           
                if ((*BitmapIndex & BitmapMask) == BitmapMask) {

                    if (LastActiveCluster == ClustersRead - 1) {
                        Records[RecordCount].Len++;
                    }
                    else {
                        Records[++RecordCount] = { ClustersRead, 1 };
                    }

                    LastActiveCluster = ClustersRead;

                }
                BitmapMask <<= 1;
                if (BitmapMask == 0) {
                    BitmapMask = 1;
                    BitmapIndex++;
                }
                ClustersRead++;
            }

            printf("Successfully set fullbackup records\n");


        }
        else {
            // NOTE(Batuhan): DeviceIOControl failed
            printf("Get volume bitmap failed [DeviceIoControl].\n");
            printf("Result was -> %d\n", R);
        }

    }
    else {
        printf("Couldn't allocate memory for bitmap buffer, tried to allocate %u bytes!\n", BufferSize);
    }


    free(Bitmap);
    
    if(Records != NULL){
      Records = (nar_record*)realloc(Records, RecordCount*sizeof(nar_record));
      (*OutRecordCount) = RecordCount;
    }
    else{
      (*OutRecordCount) = 0;
    }

    return Records;
}


#endif

/*
    Buffer = starting of the index allocation's data run offet
    BufferSize = size in bytes of the data run, since data runs represented in clusters, one can use per cluster
    information to detect end of buffer reads, but counting byte style is much easier.

*/
BOOLEAN
GetFileListFromIndexAllocationTable(void *Buffer, int BufferSize){

    Buffer = (BYTE*)Buffer + 64;
    
    return FALSE;
}

#define DH_UNINITIALIZED -1
#define DH_PRIME 1
#define DH_NOT_PRIME 0

inline BOOLEAN 
IsPrime(unsigned int n){

    if(n % 2 == 0) return FALSE;

    unsigned int floor = (unsigned int)sqrt(n);
    for(unsigned int i = 3; i < floor; i+=2){
        if(n % i == 0){
            return FALSE;
        }
    }

    return TRUE;
}

inline unsigned int
ReverseNumber(unsigned int Number){
    unsigned int ReversedNumber = 0;
    while(Number > 0){
        ReversedNumber = ReversedNumber*10 + Number % 10;
        Number = Number / 10;
    }
    return ReversedNumber;
}

#define _CRT_NON_CONFORMING_WCSTOK

inline void
NarGetProductName(char *OutName) {
    
    HKEY Key;
    if (RegOpenKeyA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", &Key) == ERROR_SUCCESS) {
        DWORD H = 0;
        RegGetValueA(Key, 0, "ProductName", RRF_RT_ANY, 0, OutName, &H);
        RegCloseKey(Key);
    }

}

int
foobar(int a, int b) {
    int c = a + b;
    c = c + a - b;
    c = c + b - a;
    return c;
}

int main() {
    
    return 0;

    wchar_t str[] = L"C:\\program files\\randomprogram\\subfolder\\executable.exe";
    wchar_t* pwc = 0;
    wchar_t* State = 0;
    pwc = wcstok(str, L"\\", &State);
    while (pwc != NULL) {
        printf("%S\n", pwc);
        pwc = wcstok(0, L"\\", &State);
    }

    return 0;

    
    

    HANDLE FileHandle = CreateFileA("testfile.txt", GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    
    SetFilePointer(FileHandle, 8192, 0, FILE_BEGIN);

    unsigned int Semirp[6854];
    memset(Semirp, DH_UNINITIALIZED, sizeof(Semirp));

    DWORD H = 0;
    WriteFile(FileHandle, Semirp, sizeof(Semirp), &H, 0);

    CloseHandle(FileHandle);

    int Primes[1000];
    memset(Primes, DH_UNINITIALIZED, sizeof(Primes));

    unsigned int SemirpIndex = 0;

    for(unsigned int n = 11; n < 1000; n+=2){

        if (Primes[n] != DH_UNINITIALIZED) {
            continue;
        }

        if(IsPrime(n)){
            
            Primes[n] = DH_PRIME;

            unsigned int R = ReverseNumber(n);

            Primes[R] = IsPrime(R) ? DH_PRIME : DH_NOT_PRIME;

            if (R > n && Primes[R] == DH_PRIME) {
                Semirp[SemirpIndex++] = n;
                Semirp[SemirpIndex++] = R;
            }

        }
        else{
            Primes[n] = DH_NOT_PRIME;
        }

    }

    for(int i = 0; i< SemirpIndex; i+=2){
        printf("%i\t%i\n", Semirp[i], Semirp[i+1]);
    }
    
    return 0;
  
  file_read F = NarReadFile("MFTLCN_REGIONS_DATA");
  nar_record Records[1024 * 52 / sizeof(nar_record)];

  memset(Records, 0, sizeof(Records));
  memcpy(Records, F.Data, F.Len);

  DWORD SectorsPerCluster   = 0;
  DWORD BytesPerSector      = 0;
  DWORD ClusterCount        = 0;

  if (GetDiskFreeSpaceA("N:\\", &SectorsPerCluster, &BytesPerSector, 0, &ClusterCount)) {
    printf("SectorsPerCluster : %lu\n", SectorsPerCluster);
    printf("BytesPerSector : %lu\n", BytesPerSector);
    printf("ClusterCount : %lu\n", ClusterCount);
  }

  return 0;
  
    DWORD BufferSize = 1024 * 2; // 64KB

    DRIVE_LAYOUT_INFORMATION_EX* DL = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(BufferSize);
    HANDLE Disk = INVALID_HANDLE_VALUE;
    int DiskID = 0;

    {
        char DiskPath[512];
        sprintf(DiskPath, "\\\\?\\PhysicalDrive%i", DiskID);

        Disk = CreateFileA(DiskPath, GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        if (Disk != INVALID_HANDLE_VALUE) {

            DWORD Hell = 0;
            if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DL, BufferSize, &Hell, 0)) {

                if (DL->PartitionStyle == PARTITION_STYLE_MBR) {
                    // TODO

                    
                    for (unsigned int PartitionIndex = 0; PartitionIndex < DL->PartitionCount; ++PartitionIndex) {

                        PARTITION_INFORMATION_EX* PI = &DL->PartitionEntry[PartitionIndex];
                        
                        printf("Partition ID %X\tPartitionType %X\tRecognizedPartition %u\tBootIndicator %u\tPartitionSize %I64d\t\tServicePartition %i\n", PI->Mbr.PartitionId, PI->Mbr.PartitionType, PI->Mbr.RecognizedPartition, PI->Mbr.BootIndicator, PI->PartitionLength.QuadPart, PI->IsServicePartition);

                    }

                }


            }
            else {
                printf("Couldnt get drive layout ex\n");
            }

        }
        else {
            printf("Couldnt open disk %s as file, cant append recovery file to backup metadata\n", DiskPath);
        }


    }

    printf("what t");
    return 0;

    printf("deneme %i\n, %s", 32, "teststr");
    printf("deneme %i", 32);

    test *obj = new test;
    obj->pa = 5;
    obj->set(5);
    obj->print();
    foo(obj);
    obj->print();

    

  return 0;

  //std::vector<int> V;
  ////V.reserve(ELEMENT_COUNT);
  //for (int i = 0; i < ELEMENT_COUNT; i++) {
  //  V.push_back(i * 2);
  //}
  

  return 0;
}
