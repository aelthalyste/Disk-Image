#include <iostream>
#include <stdio.h>
#include <windows.h>

#define ZSTD_DLL_IMPORT 1
#include "zstd.h"

#include "platform_io.cpp"

/*void
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
*/

#if 0
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
#endif 

#define ASSERT(expression) do{if(!(expression)) *(int*)0 = 0;}while(0);
#define CHECK_TERMINATE(code) do{if(ZSTD_isError(code)) {goto ERR_TERMINATION;}}while(0);
int main(){
    
	fprintf(stdout, "%I64u\n", sizeof(long int));
	FILE *FTarget = fopen("C:\\Users\\Batuhan\\Desktop\\zliboutput", "wb");
	if(FTarget == NULL){
		fprintf(stderr, "Unable to create target file\n");
		return 0;
	}
	
	char fn[] = "C:\\Users\\Batuhan\\AppData\\Local\\Microsoft\\Terminal Server Client\\Cache\\Cache0002.bin";
    
    nar_file_view fr = NarOpenFileView(fn);
	size_t FileSize = fr.Len;
    void *FileView = fr.Data;
    
    
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
	size_t block_size = 1024*1024*8;
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
	
	fclose(FTarget);	
	
	ReturnCode = ZSTD_freeCStream(stream);
	CHECK_TERMINATE(ReturnCode);
	
	printf("done !\n");
	return 0;
    
	ERR_TERMINATION:
	fprintf(stderr, "zstd error %s\n", ZSTD_getErrorName(ReturnCode));
	return 0;	
}

