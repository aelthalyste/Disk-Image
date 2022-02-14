// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs


#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <stdio.h>
#include <vector>

#include <pthread.h>

// About Desktop OpenGL function loaders:
//  Modern desktop OpenGL doesn't have a standard portable header file to load OpenGL function pointers.
//  Helper libraries are often used for this purpose! Here we are supporting a few common ones (gl3w, glew, glad).
//  You may use another loader/header of your choice (glext, glLoadGen, etc.), or chose to manually implement your own.
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>            // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>            // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>          // Initialize with gladLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD2)
#include <glad/gl.h>            // Initialize with gladLoadGL(...) or gladLoaderLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/Binding.h>  // Initialize with glbinding::Binding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/glbinding.h>// Initialize with glbinding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

// Include glfw3.h after our OpenGL definitions
#include <GLFW/glfw3.h>

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

#include <cstdlib>
#include "../restore.h"
#include "../restore.cpp"
#include "../platform_io.h"
#include "../platform_io.cpp"
#include <sstream>
#include <iostream>

enum app_state{
	app_state_select_backup,
	app_state_select_restore_type, // disk or volume
	app_state_select_disk,
	app_state_select_volume,
	app_state_restore_preview,
	app_state_restore_in_work,
	app_state_done,
	app_state_count
};

app_state PrevState(app_state State){
	static_assert(app_state_count == 7);
	switch(State){
		case(app_state_select_restore_type):
			return app_state_select_backup;
		case(app_state_select_disk):
		case(app_state_select_volume):
			return app_state_select_restore_type;
		case(app_state_restore_preview):
			return app_state_select_restore_type;		
		default:
			return app_state_select_backup;
	}
}


long int
GetFileSize(const char *c){
	FILE* f = fopen(c, "rb");
	long int Result = 0;
	if(f != NULL)
		if(0 == fseek(f, 0, SEEK_END))
			Result = ftell(f);
	
	if(NULL != f)
		fclose(f);
	else
		printf("Unable to open file %s\n", c);
	return Result;
}



inline uint8_t
NarFileNameExtensionCheck(const char *Path, const char *Extension){
    size_t pl = strlen(Path);
    size_t el = strlen(Extension);
    if(pl <= el) return 0;
    return (strcmp(&Path[pl - el], Extension) == 0);
}



// NAME TYPE UUID SIZE FSTYPE MOUNTPOINT

enum LSBLKSlots{
	LSBLKSlots_NAME   ,
	LSBLKSlots_TYPE   ,
	LSBLKSlots_PTTYPE ,
	LSBLKSlots_SIZE   ,
	LSBLKSlots_FSTYPE ,
	LSBLKSlots_MOUNTPOINT ,
	LSBLKSlots_Count
};

struct NarLSBLKPartition{
	std::string Name;
	std::string FileSystem;
	std::string MountPoint;	
	uint64_t Size;
};

struct NarLSBLKDisk{
	std::string Name; // sda, sdb, sdc
	std::string PType;
	uint64_t Size;
	std::vector<NarLSBLKPartition> Partitions;
};

char**
SplitTokens(char *Input, size_t *OutRLen, nar_arena *Arena, size_t InitialCap);

char**
SplitToLines(char *Input, size_t ILen, size_t *OutRLen, nar_arena *Arena);


std::vector<NarLSBLKDisk>
NarGetDiskList(){

	// example coommand output as shown below(without headings)
	// cmd = lsblk -o NAME,TYPE,PTTYPE,SIZE,FSTYPE,MOUNTPOINT -b -r

	/*
		NAME   TYPE UUID                                          SIZE FSTYPE MOUNTPOINT
		sda    disk                                       240057409536        
		├─sda1 part 309869F69869BB4C                         554696704 ntfs   
		├─sda2 part 866A-33B4                                104857600 vfat   /boot/efi
		├─sda3 part                                           16777216        
		└─sda4 part 40B06AEFB06AEB3C                      239379415040 ntfs   /media/bt/40B06AEFB06AEB3C
		sdb    disk                                      1000204886016        
		├─sdb1 part A458-06FD                               1073741824 vfat   
		├─sdb2 part                                           16777216        
		├─sdb3 part 40B06AEFB06AEB3C                      322122547200 ntfs   
		└─sdb4 part 085CDDB45CDD9CAE                      676988977152 ntfs   /media/bt/New Volume
		sdc    disk                                      1000203804160        
		└─sdc1 part 0d676b96-c3de-41a7-8505-c3ccfcffd6ce 1000201224704 ext4   /
	*/

	system("lsblk -o NAME,TYPE,PTTYPE,SIZE,FSTYPE,MOUNTPOINT -b -r > slbk.txt");

	std::vector<NarLSBLKDisk> Result;

	file_read FR = NarReadFile("slbk.txt");
	size_t LineCount = 0;
	nar_arena Arena = ArenaInit(calloc(1024*32,1), 1024*32, 8);

	char **Lines = SplitToLines((char*)FR.Data, FR.Len, &LineCount, &Arena);
	for(size_t ILine = 0; ILine < LineCount; ILine++){
		auto TokenRestore = ArenaGetRestorePoint(&Arena);
		
		size_t TokenCount = 0;
		char **Tokens = SplitTokens(Lines[ILine], &TokenCount, &Arena, 64);
		
		if (strcmp(Tokens[LSBLKSlots_TYPE], "disk") == 0) {
			NarLSBLKDisk Ins = {};
			Ins.Name 	= Tokens[LSBLKSlots_NAME]   ? Tokens[LSBLKSlots_NAME]   : "";
			Ins.PType   = Tokens[LSBLKSlots_PTTYPE] ? Tokens[LSBLKSlots_PTTYPE] : "";
			Ins.Size 	= atoll(Tokens[LSBLKSlots_SIZE] ? Tokens[LSBLKSlots_SIZE] : "");
			Result.emplace_back(Ins);
		}
		else if (strcmp(Tokens[LSBLKSlots_TYPE], "part") == 0) {
			NarLSBLKDisk *Disk = &Result.back();
			NarLSBLKPartition Vol = {};
			Vol.Name       = (Tokens[LSBLKSlots_NAME])       ? Tokens[LSBLKSlots_NAME]       : "";
			Vol.FileSystem = (Tokens[LSBLKSlots_FSTYPE])     ? Tokens[LSBLKSlots_FSTYPE]     : "";
			Vol.MountPoint = (Tokens[LSBLKSlots_MOUNTPOINT]) ? Tokens[LSBLKSlots_MOUNTPOINT] : "";
			Vol.Size       = atoll(Tokens[LSBLKSlots_SIZE] ? Tokens[LSBLKSlots_SIZE] : "");
			Disk->Partitions.emplace_back(Vol);
		}
		else{
			ASSERT(false);
		}

		ArenaRestoreToPoint(&Arena, TokenRestore);
	}

	FreeFileRead(FR);
	free(Arena.Memory);
	return Result;

}


char**
SplitTokens(char *Input, size_t *OutRLen, nar_arena *Arena, size_t InitialCap){
	
	if(InitialCap == 0){
		InitialCap = 128;
	}

	size_t RLen = 0;
	size_t RCap = InitialCap;	
	char **Result = (char**)ArenaAllocateZero(Arena, RCap*8);

	for(;*Input == ' '; Input++); // skip first whitespace


	char *Start = Input;
	for(;; Input++){
		if(*Input == ' ' || *Input == 0){
			size_t LLen = Input - Start;
			char *Ins = (char*)ArenaAllocateZero(Arena, LLen + 2);
			memcpy(Ins, Start, LLen);
			Result[RLen++] = Ins;
			Start = (Input) + 1;
		}
		if(*Input == 0)
			break;
	}

	*OutRLen = RLen;
	return Result;
}


char**
SplitToLines(char *Input, size_t ILen, size_t *OutRLen, nar_arena *Arena){

	size_t RLen = 0;
	size_t RCap = 1024;

	char** Result = 0;
	Result = (char**)ArenaAllocateZero(Arena, RCap * 8);
	memset(Result, 0, RCap * 8);

	char *Start = Input;
	char *End   = Input + ILen;
	
	for(; Input != End; Input++){
		if(*Input == '\n'){
			size_t LLen = Input - Start;
			char *Ins = (char*)ArenaAllocateZero(Arena, LLen + 5);//+1 for null termination
			memcpy(Ins, Start, LLen);

			Result[RLen++] = Ins;
			ASSERT(RLen <= RCap);

			Start = (Input) + 1;
		}
	}

	*OutRLen = RLen;
	return Result;
}



char*
GetNextLine(char *Input, char *Out, size_t MaxBf, char* InpEnd){
    size_t i = 0;
    char *Result = 0;

    for(i =0; Input[i] != '\n' && Input + i < InpEnd; Input++);

    if(i<MaxBf){
        memcpy(Out, Input, i);
        Out[i] = 0;
        if(Input + (i + 2) < InpEnd){
            Result = &Input[i + 2];
        }
    }

    return Result;
}


enum PartitionSelectOption{
	PartitionSelectOption_DiskOnly,
	PartitionSelectOption_Partition,
};

struct PartitionSelectResult{
	std::string Selection;
	std::vector<NarLSBLKDisk> Disks;
};

PartitionSelectResult
SelectPartition(PartitionSelectOption SelectOption){

	static PartitionSelectResult Result = {};
	static bool init = false;
	
	if(ImGui::Button("Update volume list!\n")) {
		init = false;
	}
	if(false == init){        	        		
	    Result.Disks = NarGetDiskList();
        init = true;       		
	}
    

	ImGui::Separator();
    static long int SizeGranularity[] = {
        (1024*1024*1024), (1024*1024), (1024), 1
    };
    static long int type = 1;
    if(ImGui::RadioButton("GB", type == SizeGranularity[0])) type = SizeGranularity[0]; 
    ImGui::SameLine();
    if(ImGui::RadioButton("MB", type == SizeGranularity[1])) type = SizeGranularity[1]; 
    ImGui::SameLine();
    if(ImGui::RadioButton("KB", type == SizeGranularity[2])) type = SizeGranularity[2]; 
    ImGui::SameLine();
    if(ImGui::RadioButton("Unformatted", type == SizeGranularity[3])) type = SizeGranularity[3]; 
    

    static int 	SelectedRadio = 0;
    int ID = -1;
	
	char HeaderBf[256];

    for(auto &Disk : Result.Disks){
        	
      	snprintf(HeaderBf, sizeof(HeaderBf), "name : %s type : %s size : %llu", Disk.Name.c_str(), Disk.PType.c_str(), Disk.Size/type);

       	if(SelectOption == PartitionSelectOption_DiskOnly){
    		ID++;
    		char b[32];
    		sprintf(b, "##%dcheckbox", ID);
       		if(ImGui::RadioButton(b, &SelectedRadio, ID)){
       			Result.Selection = Disk.Name;
       		}

       		ImGui::SameLine();
       	}

       	if(ImGui::CollapsingHeader(HeaderBf)){

		    if(ImGui::BeginTable("Volume table", 6, ImGuiTableFlags_Borders)){

				ImGui::TableSetupColumn("##1");
				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("Partition type");
				ImGui::TableSetupColumn("Size");
				ImGui::TableSetupColumn("File system");
				ImGui::TableSetupColumn("Mount point");
			    ImGui::TableHeadersRow();


				for(auto&Partition: Disk.Partitions){
					
					if(SelectOption == PartitionSelectOption_Partition) ID++;
					
	        		ImGui::TableNextRow();
	        		ImGui::PushID(ID);

					ImGui::TableNextColumn();
					if(SelectOption == PartitionSelectOption_Partition){
		        		char b[32];
		        		sprintf(b, "##%dcheckbox", ID);
		        		if(ImGui::RadioButton(b, &SelectedRadio, ID)){
		        			Result.Selection = Partition.Name;
		        		}					
					}
					else{
						ImGui::Text("--");
					}

					
					ImGui::TableNextColumn();        					
	        		ImGui::Text(Partition.Name.c_str());
	        		
	        		ImGui::TableNextColumn();        		
	        		ImGui::Text(Disk.PType.c_str());
	        		
	        		ImGui::TableNextColumn();
	        		ImGui::Text("%.1f", (float)Partition.Size/type);

	        		ImGui::TableNextColumn();
	        		ImGui::Text(Partition.FileSystem.c_str());
	        		
	        		ImGui::TableNextColumn();
	        		ImGui::Text(Partition.MountPoint.c_str());

	        		ImGui::PopID();
	        		
	        	}

	  	        ImGui::EndTable();

	        }
        }

    }
    
    ImGui::Separator();

	return Result;

}


pthread_t Thread;
volatile size_t RestoreBytesCopiedSoFar = 0;
volatile int    CancelStream            = 0;

void*
StreamThread(void *param){
	restore_stream *RestoreStream = (restore_stream*)param;
	
	while (RestoreStream->Error == RestoreStream_Errors::Error_NoError) {
		size_t BytesProcessed = AdvanceStream(RestoreStream);
		__sync_fetch_and_add(&RestoreBytesCopiedSoFar, BytesProcessed);		
		if (CancelStream) {
			CancelStream = 0;
			break;
		}
	} 

	return 0;
}

void
TEST_TOKENIZER(){
	nar_arena Arena = ArenaInit(calloc(1000000,1), 1000000);
	char bf[] = "adsf-asf das dffd\n123 123 123 213\n8a-!sf -=2doia jsoq34ij\nak ufdhi a903 12847\n!#]oas dfas\n  5]#2345 ;#2]3;5#;] ]3#;4 5\n";
	size_t LC = 0;
	char **Lines = SplitToLines(bf, sizeof(bf), &LC, &Arena);	
	for(size_t i = 0; i < LC; i++){
		size_t TokenCount = 0;
		char **Tokens = SplitTokens(Lines[i], &TokenCount, &Arena, 0);
		for(int ti = 0; ti<TokenCount; ti++){
			printf("%s\n",Tokens[ti]);
		}
	}
}


int main(int, char**)
{

	#if 0
	std::vector<NarLSBLKDisk> Disks = NarGetDiskList();
	for(int i =0; i<Disks.size(); i++){
		printf("%s %llu %s\n", Disks[i].Name.c_str(), Disks[i].Size, Disks[i].PType.c_str());
		for(int j = 0; j<Disks[i].Partitions.size(); j++){
			NarLSBLKPartition *P = &Disks[i].Partitions[j];
			printf("-- #PNAME:%s #SIZE:%llu #FS:%s #MP:%s\n",
				P->Name.c_str(), 
				P->Size, 
				P->FileSystem.c_str(), 
				P->MountPoint.c_str()
			);
		}
	}
	#endif


	
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#ifdef __APPLE__
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "NAR BULUT DISK BACKUP SERVICE", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    bool err = gladLoadGL() == 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD2)
    bool err = gladLoadGL(glfwGetProcAddress) == 0; // glad2 recommend using the windowing library loader instead of the (optionally) bundled one.
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
    bool err = false;
    glbinding::Binding::initialize();
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
    bool err = false;
    glbinding::initialize([](const char* name) { return (glbinding::ProcAddress)glfwGetProcAddress(name); });
#else
    bool err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to requires some form of initialization.
#endif
    if (err)
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	app_state AppState = app_state_select_backup;
	//io.Fonts->AddFontFromFileTTF("Inconsolata-Bold.ttf", 12);
	

	char BackupDir[512];
	memset(BackupDir, 0, sizeof(BackupDir));
	std::vector<backup_metadata> Backups;
	int BackupButtonID = -1;
	std::string SelectedVolume = "/dev/!";
	nar_backup_id SelectedID;
	int SelectedVersion;
	std::string TargetPartition;
	nar_arena Arena = {0};
	
	size_t ArenaSize  = Megabyte(400);
	void* ArenaMemory = malloc(ArenaSize);
	ASSERT(ArenaSize);
	Arena = ArenaInit(ArenaMemory, ArenaSize);

	restore_target *Target = NULL;
	restore_stream *RestoreStream = NULL;

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();
		// Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        bool show_demo_window = true;
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        {
        	ImGui::Begin("dev window");
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }
        
        {
        	ImGui::Begin("Dev");
        	if(ImGui::Button("Save state"))
        		ImGui::SaveIniSettingsToDisk("imgui_settings");
        	if(ImGui::Button("Load state"))
        		ImGui::LoadIniSettingsFromDisk("imgui_settings");
        	ImGui::End();
        }
        
        {
			ImGui::Begin("NARBULUT LINUX RESTORE WIZARD");

			if(AppState == app_state_select_backup){

				ImGui::Text("Directory to look backups : ");
				ImGui::InputText("", BackupDir, 512);
				
				if(ImGui::Button("Update")){
					BackupButtonID = -1;
					Backups = NarGetBackupsInDirectory(BackupDir);
				}
				
				// ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit
				if(ImGui::BeginTable("Backup table", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX)){
					
					ImGui::TableSetupColumn("");				
					ImGui::TableSetupColumn("Version");
                	ImGui::TableSetupColumn("Backup Letter");
                	ImGui::TableSetupColumn("Backup Type");
                	ImGui::TableSetupColumn("Total volume size");
                	ImGui::TableHeadersRow();
                	
                	int i =-1;
                	
					for(auto &backup: Backups){
						char bf[32];
						i++;
						
						ImGui::TableNextRow();
						ImGui::PushID(i);
						
						snprintf(bf, sizeof(bf), "##%d", i);
						ImGui::TableNextColumn();
						if(ImGui::RadioButton(bf, &BackupButtonID, i)){
							SelectedID 		= Backups[BackupButtonID].ID;
							SelectedVersion =  Backups[BackupButtonID].Version;
						}
						
						ImGui::TableNextColumn();
						if(backup.Version == -1)
							ImGui::Text("FULL");
						else
							ImGui::Text("%i", backup.Version);
													
						ImGui::TableNextColumn();
						ImGui::Text("%c", backup.Letter);
						
						ImGui::TableNextColumn();
						ImGui::Text((backup.BT == BackupType::Inc ? "Inc" : "Diff"));										
					
						ImGui::TableNextColumn();
						ImGui::Text("%I64llu", backup.VolumeTotalSize);

						ImGui::PopID();
					}
					ImGui::EndTable();
				}
				
				
				static bool popup = false;				
				if(ImGui::Button("Next")){
					if(BackupButtonID != -1){
						AppState = app_state_select_restore_type;									
					}
					else{
						ImGui::OpenPopup("Error!##");
						popup = true;			
					}
				}

				if (ImGui::BeginPopupModal("Error!##", &popup, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)){
                    ImGui::Text("Select backup first!");
                 	if(ImGui::Button("Close"))
                 		popup = false;
                 		   
                    ImGui::EndPopup();
                }
			
				
        	}
        	else if(AppState == app_state_select_restore_type){
				if(ImGui::Button("Restore without formatting the disk")){
					AppState = app_state_select_disk;
				}
				if(ImGui::Button("Fresh install (THIS OPTION CAUSE SELECTED DISK TO BE WIPED")){
					AppState = app_state_select_volume;
				}				
        	}
        	else if(AppState == app_state_select_disk){

				auto SelectionResult = SelectPartition(PartitionSelectOption_Partition);
				TargetPartition      = SelectionResult.Selection;
				static bool SizePopup = false;

				if(ImGui::Button("Restore")){

					long int PartitionSize = 0;
					for(auto &Disk : SelectionResult.Disks){
						if(TargetPartition == Disk.Name){
							PartitionSize = Disk.Size;
						}
					}
					

					if((long long unsigned int)PartitionSize < Backups[BackupButtonID].VolumeTotalSize){
						ImGui::OpenPopup("Size error!##");
						SizePopup = true;	
					}
					else{
						/*
					
						# parted -s /dev/sdb mklabel gpt
						# parted -s /dev/sdb mkpart primary fat32 0% 512MiB
						# parted -s /dev/sdb mkpart primary linux-swap 1048576s 16GiB
						# parted -s /dev/sdb mkpart primary ext4 16GiB 40%
						# parted -s /dev/sdb mkpart primary ext4 40% 60%
						# parted -s /dev/sdb mkpart primary ext4 60% 100%
						# parted -s /dev/sdb name 1 EFI-Boot
						# parted -s /dev/sdb name 2 Swap
						# parted -s /dev/sdb name 3 root
						# parted -s /dev/sdb name 4 /opt
						# parted -s /dev/sdb name 5 /home
						# parted -s /dev/sdb set 1 esp on
						
						*/
						// @TODO(Batuhan) format and create new partitions, then assign targetpartition to newly created one
						AppState = app_state_restore_preview;
					}
                	
				}

        	}
        	else if(AppState == app_state_select_volume){

				//ImGui::Text("%s", RestoreInf.TargetPartition.c_str());
				auto SelectionResult = SelectPartition(PartitionSelectOption_DiskOnly);
				TargetPartition      = SelectionResult.Selection;
				static bool SizePopup = false;				
				static bool BootPopup = false;
				static ImVec2 PopupSize = {350, 120};

				if(ImGui::Button("Restore")){

					long int PartitionSize 	= 0;
					NarLSBLKDisk SelectedDisk = {};
					for(auto &Disk : SelectionResult.Disks){
						for(auto &Partition : Disk.Partitions){
							if(Partition.Name == TargetPartition){
								PartitionSize = Partition.Size;
								SelectedDisk = Disk;
								goto R_CHECK;
							}
						}
					}

					R_CHECK:;
					if((long long unsigned int)PartitionSize < Backups[BackupButtonID].VolumeTotalSize){
						ImGui::OpenPopup("Size error!##");
						SizePopup = true;	
					}
					else{
						bool ValidDisk = false;
						for(auto &Parts : SelectedDisk.Partitions){
							if(Parts.MountPoint == "/boot/efi" && Parts.FileSystem == "fat32"){
								ValidDisk = true;
								break;
							}
						}
						
						if(false == ValidDisk){

						}


						AppState = app_state_restore_preview;
					}
                	
				}
				

				ImGui::SetNextWindowSize(PopupSize);
				if (ImGui::BeginPopupModal("Size error!##", &SizePopup, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)){
            		ImGui::Text("Backup can't fit that partition!");
            		ImGui::Text("Select partition with at least %I64llu bytes.", Backups[BackupButtonID].VolumeTotalSize);
            		if(ImGui::Button("Close##Size error"))
						SizePopup = false;
            		ImGui::EndPopup();
        		}

				ImGui::SetNextWindowSize(PopupSize);
				if (ImGui::BeginPopupModal("Boot partition error!##", &BootPopup, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)){
            		ImGui::Text("Could not detect appropriate boot partition to bind selected partition!");
            		ImGui::Text("Select disk with appropriate configuration.", Backups[BackupButtonID].VolumeTotalSize);
            		if(ImGui::Button("Close##boot part error"))
						BootPopup = false;
            		ImGui::EndPopup();
        		}

        		
        	}
        	else if(AppState == app_state_restore_preview){
        		RestoreBytesCopiedSoFar = 0;

				if(ImGui::Button("Start restore##go to restore in work")){

					Arena 		= ArenaInit(ArenaMemory, ArenaSize);
					Target 		= InitVolumeTarget("/dev/" + TargetPartition, &Arena);
					AppState 	= app_state_restore_in_work;
					std::string Path;
					GenerateMetadataName(Backups[BackupButtonID].ID, Backups[BackupButtonID].Version, Path);
					
					RestoreBytesCopiedSoFar = 0;
					CancelStream            = 0;

					RestoreStream = InitFileRestoreStream(std::string(BackupDir) + "/" + Path, Target, &Arena, Megabyte(8));
    				
    				int evalue = pthread_create(&Thread,
                          NULL,
                          StreamThread,
                          RestoreStream);
    				
    				if(evalue == EAGAIN){
    					fprintf(stderr, "PTHREAD_CREATE EGAIN ERROR\n");
    				}
    				if(evalue == EINVAL){
    					fprintf(stderr, "INVALID PARAMETER PASSED TO PTHREAD_CREATE\n");
    				}
    				if(evalue == EPERM){
    					fprintf(stderr, "PTHREAD_CREATE EPERM ERROR\n");
    				}


				}
        	}
        	else if(AppState == app_state_restore_in_work){
 				
 				if(RestoreStream->Error != RestoreStream_Errors::Error_NoError){
 					char msg_buffer[128];
 					snprintf(msg_buffer, sizeof(msg_buffer), "Error occured, error codes: %d, %d\n", RestoreStream->Error, RestoreStream->SrcError);
 					ImGui::Text(msg_buffer);
 					ImGui::Text("Please report situation to destek@narbulut.com");
 					if(ImGui::Button("Start over")){
 						FreeRestoreStream(RestoreStream);
 						AppState = app_state_select_backup;
 					} 					
 				}

 				ImGui::Text("Bytes copied : %I64llu, left : %I64llu, percentage %.4f", 
 					RestoreBytesCopiedSoFar, 
 					RestoreStream->BytesToBeCopied - RestoreBytesCopiedSoFar, 
 					(double)RestoreBytesCopiedSoFar/(double)RestoreStream->BytesToBeCopied
 					);


 				if (ImGui::Button("Cancel")) {
 					CancelStream = 1;
					int evalue = pthread_join(Thread, 0);

					char *msg = 0;
 					if (EDEADLK == evalue) {
 						msg = "EDEADLK";
 					}
 					if (EINVAL == evalue){
 						msg = "EINVAL";
 					}
 					if(ESRCH == evalue){
 						msg = "ESRCH";
 					}

 					if(msg){
 						fprintf(stderr, "pthread_join failed with code %s\n", msg);
 					}

 					FreeRestoreStream(RestoreStream);
 					AppState = app_state_select_backup;
 				}
           		
        	}
        	else if(AppState == app_state_done){
				FreeRestoreStream(RestoreStream);
				int evalue = pthread_join(Thread, 0);
				char *msg = 0;

				if (EDEADLK == evalue) {
					msg = "EDEADLK";
				}
				if (EINVAL == evalue){
					msg = "EINVAL";
				}
				if(ESRCH == evalue){
					msg = "ESRCH";
				}

				if(msg){
					fprintf(stderr, "pthread_join failed with code %s\n", msg);
				}

        		ImGui::Text("Done\n");
        	}
			
			// TODO but that thing on bottom of the screen, left-bottom probably
			if(ImGui::Button("Back")){
				AppState = PrevState(AppState);
			}


        	ImGui::End();
        }


        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
