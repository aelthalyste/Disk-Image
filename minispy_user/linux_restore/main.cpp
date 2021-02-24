// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs


#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <stdio.h>

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
#include "mspyUser.cpp"
#include <sstream>
#include <iostream>

enum app_state{
	app_state_select_backup,
	app_state_select_restore_type, // disk or volume
	app_state_select_disk,
	app_state_select_volume,
	app_state_restore
};

struct linux_disks{
	std::string DiskName; // sda, sdb etc..
	struct vol_size_name{
		std::string Name;
		long int Size;
	};
	std::vector<vol_size_name> Volumes; // sda0, sda1, sda3 etc..
};

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

std::vector<linux_disks>
GetDisks(){
	std::vector<linux_disks> Result;
	
	std::vector<std::string> SDList;
	SDList.reserve(100);
	
	system("ls /dev > devls.txt");
	file_read r = NarReadFile("devls.txt");
	
	if(r.Data != 0){
		std::stringstream ss(std::string((char*)r.Data));	
		FreeFileRead(r);

		std::string temp;
		while(ss >> temp)
			if(temp.find("sd", 0) == 0)
				SDList.push_back(temp);
							
	}
	
	if(SDList.size() > 0){
		for(auto &it: SDList)
			if(it.size() == 3)
				Result.push_back({it, {}});
			else
				Result.back().Volumes.push_back({it,GetFileSize(("/dev/" + it).c_str())});
	}
	else{
		// error unable to find disks in system
	}
	return Result;
}

/*
struct restore_thread_params{
	restore_inf RestoreInf;
};

void
RestoreWorkerThread(void* thread_params){	
	restore_inf* RestoreInf = ((restore_thread_params*)thread_params)->RestoreInf;
	OfflineRestoreDisk(RestoreInf);	
}

pthread_t
CallRestoreInThread(restore_thread_params Ri){
	       int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg);
	
	pthread_t RestoreThread; 
	pthread_attr_t ThreadAttributes;
	int tr = pthread_create(&RestoreThread, 0, &ThreadAttributes, RestoreWorkerThead, &RestoreInf);
	if(){
		
	}
	
}
*/


std::string
SelectPartition(){

	static bool init = false;
	static std::vector<linux_disks> disks;			
	static std::string Result;
	
	if(ImGui::Button("Update volume list!\n")) 
		init = false;
	if(false == init){        	        		
	    disks = GetDisks();
        init = true;       		
	}
    
	ImGui::Separator();
    static int SelectedRadio = 0;
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
    if(ImGui::RadioButton("Raw", type == SizeGranularity[3])) type = SizeGranularity[3]; 
    
	
    if(ImGui::BeginTable("Volume table", 3)){
    
		ImGui::TableSetupColumn("##1");
		ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Total size");
        ImGui::TableHeadersRow();
        
        int ID = -1;
        for(auto &d : disks){
        	for(auto&v: d.Volumes){
        		ID++;
        		ImGui::TableNextRow();  				
        		ImGui::PushID(ID);
				
				ImGui::TableNextColumn();
				
        		if(ImGui::RadioButton("##0", &SelectedRadio, ID)){
        			Result = v.Name;
        		}
        		
				ImGui::TableNextColumn();        					
        		ImGui::Text(v.Name.c_str());
        		
        		ImGui::TableNextColumn();
        		ImGui::Text("%.1f", (float)v.Size/type);
        		
        		ImGui::PopID();
        	}
        }
        
        ImGui::EndTable();
    }
    
    ImGui::Separator();
	return Result;
}


int main(int, char**)
{
	{
		const char fn[] = "/media/lubuntu/New Volume/Disk-Image/build/minispy_user/NAR_M_0-F19704356773431269.narmd";	
		FILE *F = fopen(fn, "rb");
		if(F) std::cout<<"succ opened file"<<fn<<"\n";
		else std::cout<<"unable to open file "<<fn<<"\n";
		fclose(F);
	}
	std::cout<<sizeof(backup_metadata)<<std::endl;
	
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
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 example", NULL, NULL);
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
	io.Fonts->AddFontFromFileTTF("Inconsolata-Bold.ttf", 12);
	
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
        //if (show_demo_window)
        //    ImGui::ShowDemoWindow(&show_demo_window);

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
        	static char BackupDir[512];
			static std::vector<backup_metadata> Backups;
			static int BackupButtonID = -1;
			static std::string SelectedVolume = "/dev/!";
			static restore_inf RestoreInf = {};
			if(AppState == app_state_select_backup){
				static bool debug_init = false;
				if(false == debug_init){
					Backups.push_back({0, 1, 2});
					Backups.push_back({0, 2, 3});
					Backups.push_back({0, 4, 5});
					Backups.push_back({0, 0, 52});
					Backups.push_back({0, 253, 253});
					Backups.push_back({0, 253, 253});
					Backups.push_back({0, 253, 253});
					Backups.push_back({0, 253, 253});
					Backups.push_back({0, 253, 253});
					Backups.push_back({0, 253, 253});
					Backups.push_back({0, 253, 253});
					Backups.push_back({0, 253, 253});
					Backups.push_back({0, 253, 253});
					Backups.push_back({0, 253, 253});
					Backups.push_back({0, 253, 253});
					Backups.push_back({0, 253, 253});
					
					debug_init = true;
				}
				

				ImGui::Text("Directory to look backups : ");
				ImGui::InputText("", BackupDir, 512);
				
				if(ImGui::Button("Update")){
					BackupButtonID = -1;
					RestoreInf.RootDir = str2wstr(BackupDir);
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
						
						sprintf(bf, "##%d", i);
						ImGui::TableNextColumn();
						if(ImGui::RadioButton(bf, &BackupButtonID, i)){
							RestoreInf.BackupID = Backups[BackupButtonID].ID;
							RestoreInf.Version = Backups[BackupButtonID].Version;
							std::cout<<wstr2str(GenerateMetadataName(Backups[BackupButtonID].ID, Backups[BackupButtonID].Version))<<"\n";
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
				
				if(Backups[BackupButtonID].IsOSVolume){
					
					if(ImGui::Button("Restore as bootable partition")){
						AppState = app_state_select_disk;					
					}
					if(ImGui::Button("Restore data only")){
						AppState = app_state_select_volume;
					}
									
				}
				else{
					AppState = app_state_select_volume;
				}
				
        	}
        	else if(AppState == app_state_select_disk){
				
				if(Backups[BackupButtonID].DiskType == NAR_DISKTYPE_MBR){

					RestoreInf.TargetPartition = SelectPartition();
					ImGui::Text("%s", RestoreInf.TargetPartition.c_str());
					long int PartitionSize = GetFileSize(("/dev/" + RestoreInf.TargetPartition).c_str());
					static bool popup = false;				
							
					if(ImGui::Button("Restore")){
						
						if((long long unsigned int)PartitionSize < Backups[BackupButtonID].VolumeTotalSize){
							ImGui::OpenPopup("Size error!##");
							popup = true;	
						}
						else{
							AppState = app_state_restore;
						}
                		
					}
					
					static ImVec2 PopupSize = {350, 120}; 
					ImGui::SetNextWindowSize(PopupSize);
					if (ImGui::BeginPopupModal("Size error!##", &popup, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)){
            			ImGui::Text("Backup can't fit that partition!");
            			ImGui::Text("Select partition with at least %I64llu bytes.", Backups[BackupButtonID].VolumeTotalSize);
            			if(ImGui::Button("Close##Size error"))
							popup = false;
            			ImGui::EndPopup();
        			}
	
					
					
				}
				else if(Backups[BackupButtonID].DiskType == NAR_DISKTYPE_GPT){
					
				}
				
				
				
				
				
        	}
        	else if(AppState == app_state_select_volume){
				RestoreInf.TargetPartition = SelectPartition();
				ImGui::Text("%s", RestoreInf.TargetPartition.c_str());
				long int PartitionSize = GetFileSize(("/dev/" + RestoreInf.TargetPartition).c_str());
				static bool popup = false;				
						
				if(ImGui::Button("Restore")){
					
					if((long long unsigned int)PartitionSize < Backups[BackupButtonID].VolumeTotalSize){
						ImGui::OpenPopup("Size error!##");
						popup = true;	
					}
					else{
						AppState = app_state_restore;
					}
                	
				}
				
				static ImVec2 PopupSize = {350, 120}; 
				ImGui::SetNextWindowSize(PopupSize);
				if (ImGui::BeginPopupModal("Size error!##", &popup, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)){
            		ImGui::Text("Backup can't fit that partition!");
            		ImGui::Text("Select partition with at least %I64llu bytes.", Backups[BackupButtonID].VolumeTotalSize);
            		if(ImGui::Button("Close##Size error"))
						popup = false;
            		ImGui::EndPopup();
        		}
        		
        	}
        	else if(AppState == app_state_restore){
				
				
				OfflineRestore(&RestoreInf);
				if(Backups[BackupButtonID].IsOSVolume && Backups[BackupButtonID].DiskType == NAR_DISKTYPE_MBR){
					std::string cmd;
					cmd = "sudo dd if=/usr/lib/syslinux/mbr.bin of=/dev/" + RestoreInf.TargetPartition;
					system(cmd.c_str());
					
					cmd = "sudo install-mbr -i n -p D -t 0 /dev/" + RestoreInf.TargetPartition;
					system(cmd.c_str());	
				}
				else if(Backups[BackupButtonID].IsOSVolume && Backups[BackupButtonID].DiskType == NAR_DISKTYPE_GPT){
					// TODO(Batuhan)
				}

				AppState = app_state_select_backup;
				
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
