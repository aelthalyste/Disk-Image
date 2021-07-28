#pragma once

#include <string>

struct imp_nar_file_view;

struct nar_file_view{
	unsigned char* Data;
	size_t Needle; 				// relative offset from Data
	size_t Len;					// Total size of the Data
	imp_nar_file_view *impl; 	//Platform specific implementation of file view(mmap for linux, FileMapping for windows)
};

struct nar_file_read{
    void* Data;
    int Len;
};


// Up to 2GB
struct file_read {
    void* Data;
    int Len;
};

nar_file_view 
NarOpenFileView(NarUTF8 fn);

nar_file_view 
NarOpenFileView(const std::string &fn);

nar_file_view 
NarOpenFileView(const std::wstring &fn);

void                         
NarFreeFileView(nar_file_view fv);


std::string
NarGetFileDirectory(const std::string& fn);

std::wstring
NarGetFileDirectory(const std::wstring& fn);


bool
NarFileReadNBytes(std::wstring path, void *mem, size_t N);

bool
NarFileReadNBytes(std::string path, void *mem, size_t N);


size_t
NarGetFileSize(const std::string &c);

size_t
NarGetFileSize(const std::wstring &c);




file_read
NarReadFile(const char* FileName) ;

file_read
NarReadFile(const wchar_t* FileName);

void
FreeFileRead(file_read FR);

bool
NarDumpToFile(const char* FileName, void* Data, unsigned int Size);

