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
#if _MSC_VER

#include <windows.h>

struct imp_nar_file_view{
	HANDLE MHandle;
	HANDLE FHandle;
};

inline size_t
NarGetFileSize(const char *Path){
	return NarGetFileSize(str2wstr(Path).c_str());
}

inline size_t
NarGetFileSize(const wchar_t* Path) {
    ULONGLONG Result = 0;
    HANDLE F = CreateFileW(Path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (F != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER L = { 0 };
        GetFileSizeEx(F, &L);
        Result = L.QuadPart;
    }
    CloseHandle(F);
    return Result;
}

nar_file_view
NarOpenFileView(const char* fn){
	return NarOpenFileView(str2wstr(fn).c_str());
}

nar_file_view
NarOpenFileView(const wchar_t *fn){
	HANDLE MappingHandle = 	INVALID_HANDLE_VALUE;
	HANDLE FileHandle = 	INVALID_HANDLE_VALUE;
	size_t FileSize = 0;
	void* FileView = NULL;
	ULARGE_INTEGER vs = {0};
	
	nar_file_view Result = {0};
	
	Result.impl = (imp_nar_file_view*)malloc(sizeof(imp_nar_file_view));
	if(NULL == Result.impl) 
		goto FV_ERROR;

	FileSize = NarGetFileSize(fn);
    MappingHandle = 0;
	FileHandle = CreateFileW(fn, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_ALWAYS, 0, 0);    
	if(FileHandle != INVALID_HANDLE_VALUE){
		vs= {0};
		vs.QuadPart = FileSize;
		MappingHandle = CreateFileMappingW(FileHandle, NULL, PAGE_READWRITE, 0, 0, 0);	
		if(NULL != MappingHandle){
        	FileView = MapViewOfFile(MappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        	if(NULL != FileView){
        		Result.Data 			= (unsigned char*)FileView;
        		Result.Len  			= FileSize;
        		Result.impl->MHandle 	= MappingHandle;
        		Result.impl->FHandle 	= FileHandle;
        	}
        	else{
        		fprintf(stderr, "MapViewOfFile failed with code %X for file %S\n", GetLastError(), fn);
        		goto FV_ERROR;
        	}		
		}
		else{
			fprintf(stderr, "CreateFileMappingW failed with error code %X for file %S\n", GetLastError(), fn);
			goto FV_ERROR;
		}
        
	}
	else{
		fprintf(stderr, "CreateFileW failed with code %X for file %S\n", GetLastError(), fn);
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
NarGetFileDirectory(const char *arg_fn){
    std::string fn(arg_fn);
    auto indice = fn.rfind("\\");
    if(indice != std::string::npos)
        return fn.substr(0, indice + 1);
    return "";
}

std::wstring
NarGetFileDirectory(const wchar_t *arg_fn){
    std::wstring fn(arg_fn);
    auto indice = fn.rfind(L"\\");
    if(indice != std::wstring::npos)
        return fn.substr(0, indice + 1);
    return L"";
}


#endif // MSVC DEF

#if __GNUC__

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

nar_file_read
NarReadFileNBytes(const char *arg_fn){
	
}

nar_file_read
NarReadFileNBytes(const wchar_t *arg_fn){
	
}


#endif
