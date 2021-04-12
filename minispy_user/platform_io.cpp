#include "platform_io.h"
#include <string>	
#include <stdlib.h>


static std::string
wstr2str(const std::wstring& s){
	char *c = (char*)malloc(s.size() + 1);
	memset(c, 0, s.size());
	
	size_t sr = wcstombs(c, s.c_str(), s.size() + 1);
	if(sr == (size_t)-1)
		memset(c, 0, s.size() + 1);
	std::string Result(c);
	free(c);
	return Result;
}

static std::wstring
str2wstr(const std::string& s){
	wchar_t *w = (wchar_t*)malloc((s.size() + 1)*sizeof(wchar_t));
	memset(w, 0, (s.size() + 1)*sizeof(wchar_t));
	
	size_t sr = mbstowcs(w, s.c_str(), s.size() + 1);
	if(sr == (size_t)-1)
		memset(w, 0, (s.size() + 1)*sizeof(wchar_t));
	std::wstring Result(w);
	free(w);
	return Result;
}


// windows implementation
#if _WIN32

#include <windows.h>

struct imp_nar_file_view{
	HANDLE MHandle;
	HANDLE FHandle;
};

inline size_t
NarGetFileSize(const std::string &Path){
	return NarGetFileSize(str2wstr(Path));
}

inline size_t
NarGetFileSize(const std::wstring& Path) {
    ULONGLONG Result = 0;
    HANDLE F = CreateFileW(Path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (F != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER L = { 0 };
        GetFileSizeEx(F, &L);
        Result = L.QuadPart;
    }
    CloseHandle(F);
    return Result;
}

nar_file_view
NarOpenFileView(const std::string &fn){
	return NarOpenFileView(str2wstr(fn));
}

nar_file_view
NarOpenFileView(const std::wstring &fn){
	HANDLE MappingHandle = 	INVALID_HANDLE_VALUE;
	HANDLE FileHandle = 	INVALID_HANDLE_VALUE;
	size_t FileSize = 0;
	void* FileView = NULL;
	ULARGE_INTEGER vs = {0};
	
	nar_file_view Result = {0};
	
	Result.impl = (imp_nar_file_view*)malloc(sizeof(imp_nar_file_view));
	if(NULL == Result.impl) 
		goto FV_ERROR;
    
	FileSize = NarGetFileSize(std::wstring(fn));
    MappingHandle = 0;
	FileHandle = CreateFileW(fn.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_ALWAYS, 0, 0);    
	if(FileHandle != INVALID_HANDLE_VALUE){
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
        		fprintf(stderr, "MapViewOfFile failed with code %X for file %S\n", GetLastError(), fn.c_str());
        		goto FV_ERROR;
        	}		
		}
		else{
			fprintf(stderr, "CreateFileMappingW failed with error code %X for file %S\n", GetLastError(), fn.c_str());
			goto FV_ERROR;
		}
        
	}
	else{
		fprintf(stderr, "CreateFileW failed with code %X for file %S\n", GetLastError(), fn.c_str());
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

std::string
NarGetFileDirectory(const std::string& fn){
    auto indice = fn.rfind("\\");
    if(indice != std::string::npos)
        return fn.substr(0, indice + 1);
    return "";
}

std::wstring
NarGetFileDirectory(const std::wstring& fn){
    auto indice = fn.rfind(L"\\");
    if(indice != std::wstring::npos)
        return fn.substr(0, indice + 1);
    return L"";
}


bool
NarFileReadNBytes(std::string path, void *mem, size_t N){
    return NarFileReadNBytes(str2wstr(path), mem, N);
}

bool
NarFileReadNBytes(std::wstring path, void *mem, size_t N){
    bool Result = false;
    HANDLE File = CreateFileW(path.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if (File != INVALID_HANDLE_VALUE) {
        DWORD BytesRead = 0;
        ReadFile(File, mem, N, &BytesRead, 0);
        if (BytesRead == N) {
            // NOTE success
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
NarReadFile(const char* FileName) {
    file_read Result = { 0 };
    HANDLE File = CreateFileA(FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
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


file_read
NarReadFile(const wchar_t* FileName) {
    file_read Result = { 0 };
    HANDLE File = CreateFileW(FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if (File != INVALID_HANDLE_VALUE) {
        DWORD BytesRead = 0;
        Result.Len = (DWORD)GetFileSize(File, 0); // safe conversion
        if (Result.Len == 0) return Result;
        Result.Data = malloc((size_t)Result.Len);
        if (Result.Data) {
            ReadFile(File, Result.Data, Result.Len, &BytesRead, 0);
            if (BytesRead == (DWORD)Result.Len) {
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
        printf("Can't create file: %S\n", FileName);
    }
    return Result;
}

void
FreeFileRead(file_read FR) {
    free(FR.Data);
}


bool
NarDumpToFile(const char* FileName, void* Data, unsigned int Size) {
    BOOLEAN Result = FALSE;
    HANDLE File = CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
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
        printf("Can't create file: %s\n", FileName);
    }
    //CreateFileA(FNAME, GENERIC_WRITE, 0,0 ,CREATE_NEW, 0,0)
    return Result;
}



#endif // MSVC DEF

#if __linux__

#error linux mmap operations are not supported yet

std::string
NarGetFileDirectory(const char *arg_fn){
    std::string fn(arg_fn);
    auto indice = fn.rfind("/");
    if(indice != std::string::npos)
        return fn.substr(0, indice + 1);
    return "";
}

std::wstring
NarGetFileDirectory(const wchar_t *arg_fn){
    return NarGetFileDirectory(wstr2str(arg_fn));
    std::wstring fn(arg_fn);
    auto indice = fn.rfind(L"/");
    if(indice != std::wstring::npos)
        return fn.substr(0, indice + 1);
    return L"";
}

file_read
NarReadFileNBytes(const char *arg_fn){
	
}

file_read
NarReadFileNBytes(const wchar_t *arg_fn){
	
}


#endif
