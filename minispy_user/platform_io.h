#pragma once

#include "precompiled.h"
#include <string>
#include "nar.h"
#include "narstring.h"
#include "utf8.h"

struct imp_nar_file_view;

struct nar_file_view{
	unsigned char* Data;
	size_t Needle; 				// relative offset from Data
	size_t Len;					// Total size of the Data
	imp_nar_file_view *impl; 	//Platform specific implementation of file view(mmap for linux, FileMapping for windows)
};

// Up to 2GB
struct file_read {
    void* Data;
    int Len;
};

nar_file_view 
NarOpenFileView(const UTF8 *FN);

void                         
NarFreeFileView(nar_file_view fv);


bool NarFileReadNBytes(const UTF8 *FN, void *mem, size_t N);

size_t
NarGetFileSize(const UTF8 *FN);

file_read
NarReadFile(const UTF8 *FN);

void
FreeFileRead(file_read FR);

bool
NarDumpToFile(const char* FileName, void* Data, unsigned int Size);

