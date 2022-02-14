#include "precompiled.h"
#include "platform_io.h"


// windows implementation
#if _WIN32


#include <windows.h>
#include "nar_win32.h"

struct imp_nar_file_view{
	HANDLE MHandle;
	HANDLE FHandle;
};

inline size_t
NarGetFileSize(const UTF8 *Path) {
    wchar_t *FilePath = NarUTF8ToWCHAR(Path);
    defer({free(FilePath);});
    
    ULONGLONG Result = 0;
    HANDLE F = CreateFileW(FilePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (F != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER L = { 0 };
        GetFileSizeEx(F, &L);
        Result = L.QuadPart;
    }
    CloseHandle(F);
    return Result;
}


nar_file_view
NarOpenFileView(const UTF8 *Utf8Path){
    
    wchar_t *FilePath = NarUTF8ToWCHAR(Utf8Path);
    defer({free(FilePath);});


    HANDLE MappingHandle = 	INVALID_HANDLE_VALUE;
	HANDLE FileHandle    = 	INVALID_HANDLE_VALUE;
	size_t FileSize      = 0;
	void* FileView       = NULL;
	ULARGE_INTEGER vs    = {0};
	
	nar_file_view Result = {0};
	
	Result.impl = (imp_nar_file_view*)malloc(sizeof(imp_nar_file_view));
	if(NULL == Result.impl) 
		goto FV_ERROR;
    
    MappingHandle = 0;
	FileHandle = CreateFileW(FilePath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_ALWAYS, 0, 0);    
	if(FileHandle != INVALID_HANDLE_VALUE){
        
        LARGE_INTEGER FSLI;
        FSLI.QuadPart = 0;
        GetFileSizeEx(FileHandle, &FSLI);
        FileSize = FSLI.QuadPart;

		vs= {0};
		vs.QuadPart = FileSize;
		MappingHandle = CreateFileMappingW(FileHandle, NULL, PAGE_READONLY, 0, 0, 0);	
		if(NULL != MappingHandle){
        	FileView = MapViewOfFile(MappingHandle, FILE_MAP_READ, 0, 0, 0);
        	if(NULL != FileView){
        		Result.Data 			= (unsigned char*)FileView;
        		Result.Len  			= FileSize;
        		Result.impl->MHandle 	= MappingHandle;
        		Result.impl->FHandle 	= FileHandle;
        	}
        	else{
        		fprintf(stderr, "MapViewOfFile failed with code %X for file %S\n", GetLastError(), FilePath);
        		goto FV_ERROR;
        	}		
		}
		else{
			fprintf(stderr, "CreateFileMappingW failed with error code %X for file %S\n", GetLastError(), FilePath);
			goto FV_ERROR;
		}
        
	}
	else{
		fprintf(stderr, "CreateFileW failed with code %X for file %S\n", GetLastError(), FilePath);
		goto FV_ERROR;
	}
    
	return Result;
	FV_ERROR:
	
	if(NULL != Result.impl){
		free(Result.impl);
	}
	
	
	return Result;
}

void
NarFreeFileView(nar_file_view FV){
    if(NULL != FV.Data)
		UnmapViewOfFile(FV.Data);
	if(NULL == FV.impl) 
		return;
	CloseHandle(FV.impl->MHandle);
	CloseHandle(FV.impl->FHandle);
}

bool NarFileReadNBytes(const UTF8 *Path, void *mem, size_t N){
    bool Result = false;
    wchar_t *FilePath = NarUTF8ToWCHAR(Path);
    defer({free(FilePath);});

    ASSERT(N < Gigabyte(2));
    HANDLE File = CreateFileW(FilePath, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if (File != INVALID_HANDLE_VALUE) {
        DWORD BytesRead = 0;
        if (ReadFile(File, mem, (DWORD)N, &BytesRead, 0) && BytesRead == N) {
            Result = true;
        }
        else {
            //printf("Read %i bytes instead of %i\n", BytesRead, Result.Len);
        }
        CloseHandle(File);
    }
    else {
        //printf("Can't create file: %s\n", FileName);
    }
    return Result;
}




file_read
NarReadFile(const UTF8* FileName) {
    file_read Result = { 0 };

    wchar_t *FilePath = NarUTF8ToWCHAR(FileName);
    defer({free(FilePath);});

    HANDLE File = CreateFileW(FilePath, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if (File != INVALID_HANDLE_VALUE) {
        DWORD BytesRead = 0;
        Result.Len = (int)GetFileSize(File, 0); // safe to assume file size < 2 GB
        if (Result.Len == 0) return Result;
        Result.Data = malloc((size_t)Result.Len);
        if (Result.Data) {
            ReadFile(File, Result.Data, (DWORD)Result.Len, &BytesRead, 0);
            if (BytesRead == (DWORD)Result.Len) {
                // NOTE success
            }
            else {
                free(Result.Data);
                Result.Data = NULL;
                Result.Len  = 0;
            }
        }
        CloseHandle(File);
    }
    else {
        // err!
    }

    return Result;
}


void
FreeFileRead(file_read FR) {
    free(FR.Data);
}


bool
NarDumpToFile(const UTF8* FileName, void* Data, unsigned int Size) {
    BOOLEAN Result = FALSE;
    wchar_t *FilePath = NarUTF8ToWCHAR(FileName);
    defer({free(FilePath);});

    HANDLE File = CreateFileW(FilePath, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (File != INVALID_HANDLE_VALUE) {
        DWORD BytesWritten = 0;
        WriteFile(File, Data, Size, &BytesWritten, 0);
        if (BytesWritten == Size) {
            Result = TRUE;
        }
        else {
            printf("Written %i bytes instead of %i\n", BytesWritten, Size);
        }
        CloseHandle(File);
    }
    else {
        printf("Can't create file: %s\n", FilePath);
    }
    //CreateFileA(FNAME, GENERIC_WRITE, 0,0 ,CREATE_NEW, 0,0)
    return Result;
}



#endif // MSVC DEF

#if __linux__


#include <sys/mman.h>
#include <stdio.h>
#include <linux/mman.h>

struct imp_nar_file_view{
    FILE* File;
};

nar_file_view
NarOpenFileView(const UTF8 *fn){
    nar_file_view Result = {};
    FILE* File = fopen(fn, "rb");
    ASSERT(File);
    if(File != 0){
        int FD = fileno(File);    
        size_t FileSize = NarGetFileSize(fn);
        ASSERT(FileSize != 0);
        // MAP_HUGETLB | MAP_HUGE_2MB | 
        void *FileMapMemory = mmap(0, FileSize, PROT_READ, MAP_SHARED_VALIDATE, FD, 0);
        ASSERT(MAP_FAILED != FileMapMemory);
        fclose(File);
        
        if(FileMapMemory != 0){
            Result.Data = (uint8_t*)FileMapMemory;
            Result.Len  = FileSize;
        }
        
    }
    
    return Result;
}

void
NarFreeFileView(nar_file_view FV){
    munmap(FV.Data, FV.Len);
}


#define _FILE_OFFSET_BITS 64

inline size_t
NarGetFileSize(const UTF8 *Path){
    FILE *File = fopen(Path, "rb");
    off_t Result = 0;
    if(NULL!=File){
        fseek(File, 0L, SEEK_END);
        Result = ftello(File);
        fclose(File);
    }
    return Result;
}

file_read
NarReadFile(const UTF8* FileName) {
    file_read Result = { 0 };
    size_t FileSize = NarGetFileSize(FileName);
    if(FileSize > 0){
        FILE *File = fopen(FileName, "rb");
        if(File){
            Result.Data = malloc(FileSize);
            Result.Len  = FileSize;
            ASSERT(Result.Data);
            if(NULL != Result.Data){
                if(1 == fread(Result.Data, FileSize, 1, File)){
                    // success
                }
                else{
                    ASSERT(0);
                    FreeFileRead(Result);
                    Result = {};
                }
            }
            
            fclose(File);    
        }
    }
    else{
        
    }
    return Result;
}

void
FreeFileRead(file_read FR){
    free(FR.Data);
}


bool
NarFileReadNBytes(UTF8 *path, void *mem, size_t N){
    FILE* File = fopen(path, "rb");
    bool Result = (1 == fread(mem, N, 1, File));
    fclose(File);
    return Result;
}

#endif
