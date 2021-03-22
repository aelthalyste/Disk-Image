#include <iostream>
#include <stdio.h>
#include <windows.h>
#define ZSTD_DLL_IMPORT 1
#include "zstd.h"

struct file_view{
    void* Data;
	size_t Length;
	
    private:
    
    union{
        // windows stuff
        struct{
            void* FHandle;
            void* MHandle
        };
        // some linux stuff
        struct {
            void* stuff;
        };
    };
};


struct file_view {
    void* data;
    size_t len;
    HANDLE FHandle;
    HANDLE MHandle;
}


file_read readfile(const char *fn){
	file_read result = {0};
	FILE *F = fopen(fn, "rb");
	if(F){
		if(0 == fseek(F, 0, SEEK_END)){
			result.len = ftell(F);
			result.data = malloc(result.len);
			fseek(F, 0, SEEK_SET);
			fread(result.data, 1, result.len, F);		
		}
		else{
			printf("unable to seek end of file");
		}
		fclose(F);
	}
	else{
		printf("unable to open file %s\n", fn); 
	}
	return result;
}

HANDLE
NarOpenVolume(char Letter) {
    char VolumePath[512];
    sprintf(VolumePath, "\\\\.\\%c:", Letter);
    
    HANDLE Volume = CreateFileA(VolumePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
    if (Volume != INVALID_HANDLE_VALUE) {
        
#if 1        
        if (DeviceIoControl(Volume, FSCTL_LOCK_VOLUME, 0, 0, 0, 0, 0, 0)) {
            
        }
        else {
            // NOTE(Batuhan): this isnt an error, tho prohibiting volume access for other processes would be great.
            fprintf(stderr, "Couldn't lock volume %c\n", Letter);
        }
#endif
        
        
    }
    else {
        fprintf(stderr, "Couldn't open volume %c\n", Letter);
    }
    
    return Volume;
}

unsigned long long
NarGetVolumeTotalSize(char Letter) {
    char Temp[] = "!:\\";
    Temp[0] = Letter;
    ULARGE_INTEGER L = { 0 };
    GetDiskFreeSpaceExA(Temp, 0, &L, 0);
    return L.QuadPart;
}

inline size_t
NarGetFileSize(const char* Path) {
    ULONGLONG Result = 0;
    HANDLE F = CreateFileA(Path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (F != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER L = { 0 };
        GetFileSizeEx(F, &L);
        Result = L.QuadPart;
    }
    CloseHandle(F);
    return Result;
}

file_read
NarGetFileView(char *fn){
	file_read Result = {0};
	size_t FileSize = NarGetFileSize(fn);
    HANDLE MappingHandle = 0;
	HANDLE FileHandle = CreateFileA(fn, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_ALWAYS, 0, 0);
    
	if(FileHandle != INVALID_HANDLE_VALUE){
		ULARGE_INTEGER vs= {0};
		vs.QuadPart = FileSize;
		MappingHandle = CreateFileMappingA(FileHandle, NULL, PAGE_READWRITE, 0, 0, 0);	
		if(NULL != MappingHandle){
        	void* FileView = MapViewOfFile(MappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        	if(NULL != FileView){
        		Result.data = FileView;
        		Result.len = FileSize;
        		Result.MHandle = MappingHandle;
        		Result.FHandle = FileHandle;
        		fprintf(stdout, "hold me here\n");	
        	}
        	else{
        		fprintf(stderr, "MapViewOfFile failed with code %X\n", GetLastError());
        		return Result;
        	}		
		}
		else{
			fprintf(stderr, "createfilemappinga failed with error code %X\n", GetLastError());
			return Result;
		}
        
	}
	else{
		fprintf(stderr, "naropenvolunme failed with code %X\n", GetLastError());
		return Result;
	}
    
	return Result;
}

void
NarCloseFileView(file_read f){
	UnmapViewOfFile(f.data);
	CloseHandle(f.MHandle);
	CloseHandle(f.FHandle);
}

void
list_frame_sizes(char *src){
	file_read frsrc = NarGetFileView(src);
    
	void* h = frsrc.data;
	void* n = h;
	void* end = (char*)frsrc.data + frsrc.len;
	unsigned long long dcsize = 0;
	size_t frame_id = 0;
	while(n != end){
    	unsigned long long frame_size = ZSTD_getFrameContentSize(n, (size_t)((char*)end - (char*)n));
    	if(ZSTD_CONTENTSIZE_UNKNOWN == frame_size){
    		fprintf(stderr, "Unknown size\n");
    		break;
    	}
    	else if(ZSTD_CONTENTSIZE_ERROR == frame_size){
    		fprintf(stderr, "Contentsize error\n");
    		break;
    	}
    	else if(ZSTD_isError(frame_size)){
    		fprintf(stderr, "zstd error %s\n", ZSTD_getErrorName(frame_size));		
    		break;
    	}
    	else{
    	    fprintf(stdout, "Frame ID: %I64u\tFrameSize : %I64u\tTotalSize : %I64u\n", frame_id, frame_size, dcsize);
    		frame_id++;
    		dcsize += dcsize;
    		size_t frame_uc = ZSTD_findFrameCompressedSize(n, (size_t)((char*)end - (char*)n));
    		n = (char*)n+frame_uc;
    	}
        
	}
    
	return;
}

void
decompress(char* srcfn, char *dstfn){
    
	file_read frsrc = NarGetFileView(srcfn);
	FILE *dest = fopen(dstfn, "wb");
	
	ZSTD_outBuffer output = {0};
	ZSTD_inBuffer input = {0};
	
	size_t bsize = 1024*1024*16;
	void* bf = malloc(bsize);
    
	{
		printf("read file!\n");
		input.src = frsrc.data;
		input.size = frsrc.len;
		input.pos = 0;
		output.dst = bf;
		output.size = bsize;
		output.pos = 0;
	}
	size_t RetCode = 0;
	ZSTD_DStream* DStream = ZSTD_createDStream();
    
    
	while(input.pos != input.size){
		RetCode = ZSTD_decompressStream(DStream, &output, &input);		
		if(ZSTD_isError(RetCode)){
			goto ERR_TER;		
		}
		fwrite(bf, 1, output.pos, dest);
		output.pos = 0;
	}
    
	
	ERR_TER:
	NarCloseFileView(frsrc);	
	fclose(dest);
	RetCode = ZSTD_freeDStream(DStream);
	
	fprintf(stderr, "Err description %s\n", ZSTD_getErrorName(RetCode));
	NarCloseFileView(frsrc);
	
	
    
}

void
test(){
    
	FILE* f = fopen("C:\\Users\\User\\Desktop\\targetfile", "rb");
	size_t bsize = 1024*1024*1;
	void *bf = malloc(1*1024*1024);
    
	if(f){
		fread(bf, 1, bsize, f);
		fclose(f);
		f = 0;
	}
	//ZSTD_getFrameHeader(ZSTD_frameHeader* zfhPtr, const void* src, size_t srcSize);
    
	unsigned long long decompsize = ZSTD_getFrameContentSize(bf, bsize);
	if(ZSTD_CONTENTSIZE_UNKNOWN == decompsize){
		fprintf(stdout, "Unknown size\n");
	}
	if(ZSTD_CONTENTSIZE_ERROR == decompsize){
		fprintf(stderr, "Contentsize error\n");
	}
	if(ZSTD_isError(decompsize)){
		fprintf(stderr, "zstd error %s\n", ZSTD_getErrorName(decompsize));		
	}
	fprintf(stdout, "decompressed file size is %I64u\n", decompsize);
    
	return;
}

#define ASSERT(expression) do{if(!(expression)) *(int*)0 = 0;}while(0);
#define CHECK_TERMINATE(code) do{if(ZSTD_isError(code)) {goto ERR_TERMINATION;}}while(0);
int main(){
    list_frame_sizes("C:\\Users\\User\\Desktop\\targetfile");
	return 0;
    decompress("C:\\Users\\User\\Desktop\\targetfile", "C:\\Users\\User\\Desktop\\anotherfile");
	return 0;
    
	test();
	return 0;
	fprintf(stdout, "%I64u\n", sizeof(long int));
	FILE *FTarget = fopen("C:\\Users\\User\\Desktop\\targetfile", "wb");
	if(FTarget == NULL){
		fprintf(stderr, "Unable to create target file\n");
		return 0;
	}
	
	char fn[] = "F:\\Disk-Image\\build\\minispy_user\\NAR_BACKUP_FULL-C18890713974573029.narbd";
    
    file_read fr = NarGetFileView(fn);
	size_t FileSize = fr.len;
    void *FileView = fr.data;
    
    
	size_t bsize = 16*1024*1024;
	size_t ReturnCode = 0;
	
	ZSTD_outBuffer output = {0};
	ZSTD_inBuffer input = {0};
    
	std::cout<<"ZSTD VERSION : "<<ZSTD_versionNumber()<<"\n";
	fprintf(stdout, "min compression level %d\n", ZSTD_minCLevel());
	fprintf(stdout, "max compression level %d\n", ZSTD_maxCLevel());    
    
	{
		printf("read file!\n");
		input.src = FileView;
		input.size = FileSize;
		input.pos = 0;
		output.dst = malloc(bsize);
		output.size = bsize;
		output.pos = 0;
	}
	
	ZSTD_CCtx* context = ZSTD_createCCtx();
	ZSTD_CStream* stream = ZSTD_createCStream();
    
    
	ZSTD_bounds bounds = ZSTD_cParam_getBounds(ZSTD_c_nbWorkers);
	fprintf(stdout, "%I64u %d %d\n", bounds.error, bounds.lowerBound, bounds.upperBound);
	
	bounds = ZSTD_cParam_getBounds(ZSTD_c_contentSizeFlag);
	fprintf(stdout, "%I64u %d %d\n", bounds.error, bounds.lowerBound, bounds.upperBound);
	ReturnCode = ZSTD_CCtx_setParameter(context, ZSTD_c_contentSizeFlag, 1);
	CHECK_TERMINATE(ReturnCode);	
    
	ReturnCode = ZSTD_CCtx_setParameter(context, ZSTD_c_nbWorkers, 0);
	CHECK_TERMINATE(ReturnCode);
    
    
    //ReturnCode = ZSTD_CCtx_setPledgedSrcSize(context, FileSize);
	//CHECK_TERMINATE(ReturnCode);
	size_t processed = 0;
	size_t block_size = 1024*1024*16;
	while(processed != FileSize){
		input.src = (char*)FileView + processed;
		if(FileSize - processed > block_size)
			input.size = block_size;
		else
			input.size = FileSize - processed;
        
		input.pos = 0;
		while(input.size != input.pos){
			ReturnCode = ZSTD_compressStream2(stream, &output, &input, ZSTD_e_end);
			CHECK_TERMINATE(ReturnCode);
			fwrite(output.dst, 1, output.pos, FTarget);
			output.pos = 0;	
		}
		
		processed += input.size;
	}
	
	
	// tail termination
	while(0!=ReturnCode){
	    fprintf(stdout, "tail termination, src size %I64u, return code %I64u\n", output.pos, ReturnCode);
		
		ReturnCode = ZSTD_compressStream2(stream, &output, &input, ZSTD_e_end);
		CHECK_TERMINATE(ReturnCode);
		fwrite(output.dst, 1, output.pos, FTarget);
		output.pos = 0;
	}
	
	
	//fprintf(stdout, "Ret code %I64u\n", ReturnCode);
	//ReturnCode = ZSTD_CCtx_setPledgedSrcSize(context, FileSize);
	//CHECK_TERMINATE(ReturnCode);
	
	//ReturnCode = ZSTD_compressStream2(stream, &output, &input, ZSTD_e_end);
	//fwrite(output.dst, 1, output.pos, FTarget);
	//CHECK_TERMINATE(ReturnCode);
    
	fclose(FTarget);	
	
	ReturnCode = ZSTD_freeCStream(stream);
	CHECK_TERMINATE(ReturnCode);
	
	printf("done !\n");
	return 0;
    
	ERR_TERMINATION:
	fprintf(stderr, "zstd error %s\n", ZSTD_getErrorName(ReturnCode));
	return 0;	
}

