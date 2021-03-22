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

nar_file_view 
NarOpenFileView(std::string fn);

nar_file_view 
NarOpenFileView(std::wstring fn);

void                         
NarFreeFileView(nar_file_view* fv);


std::string
NarGetFileDirectory(std::string fn);

std::wstring
NarGetFileDirectory(std::wstring fn);


nar_file_read
NarReadFileNBytes(std::string arg_fn);

nar_file_read
NarReadFileNBytes(std::wstring arg_fn);


size_t
NarGetFileSize(std::string c);

size_t
NarGetFileSize(std::wstring c);



