#define _CRT_SECURE_NO_WARNINGS

#include "glad\glad.h"
#include "GLFW\glfw3.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imstb_textedit.h"
#include "imstb_truetype.h"

#include <stdio.h>
#include <ShlObj.h>
#include <Windows.h>

static ULONGLONG GlobalPerfCountFreq;
inline LARGE_INTEGER
GetClock() {
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return Result;
}

inline float
GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End) {
    float Result = (float)(End.QuadPart - Start.QuadPart) / (float)(GlobalPerfCountFreq);
    return Result;
}

void 
ErrorCallback(int error_code, const char* description) {
    printf("Err occured with code %i and description", error_code, description);
    printf(description);
    printf("\n");
}

#define unsigned char BYTE;
struct example_input_struct{
    BYTE W,A,S,D;
}GlobalInputs; // global olarak input tanimlamak gayet okey

/*
WIN32 ile daha iyi input alirsin, ama ben ugrasmayi sevmiyorum diyorsan(benim gibi), bu fonksiyon yeterli oluyor
*/
void 
PollInput(GLFWwindow *WindowHandle){
    
    static auto IsPressed = [&](int key){return glfwGetKey(WindowHandle,key) == GLFW_PRESS;};
    
    /*
Example usage:
    IsPressed(GLFW_KEY_W);
    IsPressed(GLFW_KEY_A);
    IsPressed(GLFW_KEY_S);
    IsPressed(GLFW_KEY_D);
    */
    
    GlobalInputs.W = IsPressed(GLFW_KEY_W);
    GlobalInputs.A = IsPressed(GLFW_KEY_A);
    GlobalInputs.S = IsPressed(GLFW_KEY_S);
    GlobalInputs.D = IsPressed(GLFW_KEY_D);
    
}


#define UI_WindowF(rect, flags, open_ptr, ...)  _UI_DeferLoop(UI_BeginWindowF(rect, flags, open_ptr, __VA_ARGS__), UI_EndWindow())
#define _UI_DeferLoop(begin, end)               for(int _i_ = (begin, 0); !_i_; ++_i_, end)


int
main() {
    foo();
    
    int TargetFPS = 60;
    float TargetFPSInSec = 1.f / TargetFPS;
    
    LARGE_INTEGER PerfCountFreqResult;
    QueryPerformanceFrequency(&PerfCountFreqResult);
    GlobalPerfCountFreq = PerfCountFreqResult.QuadPart;
    
    glfwSetErrorCallback(ErrorCallback);
    
    int R = glfwInit();
    if (R != GLFW_TRUE) {
        printf("Couldnt init glfw, code = %i\n", R);
        return 0;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* winHandle = glfwCreateWindow(1280, 720, "demotitle", NULL, NULL);
    
    glfwMakeContextCurrent(winHandle);
    glfwSwapInterval(1);
    
    if (winHandle == nullptr) {
        R = glfwGetError(0);
        printf("Couldnt create window, code = %i", R);
        return -1;
    }
    
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Failed to initialize GLAD\n");
        return -2;
    }
    
    glViewport(0, 0, 800, 600);
    
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(winHandle, true);
    ImGui_ImplOpenGL3_Init("#version 140");
    
    glClearColor(1.f, 1.0f, 1.0f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    
    LARGE_INTEGER LastCounter = GetClock();
    float SecondsElapsed = 0.f;
    
    while (!glfwWindowShouldClose(winHandle)) {
        
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        ImGui::ShowTestWindow();  
        
        glfwPollEvents();
        PollInput(winHandle);
        
        
        /*
            PUT YOUR CODE HERE
        */
        
        
        
        glClearColor(.2f, .2f, .2f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        ImGui::Begin("testw");
        ImGui::Text("Ms elapsed %.2f", SecondsElapsed * 1000);
        ImGui::Text("FPS: %.2f", 1.f / SecondsElapsed);
        ImGui::End();
        
        ImGui::Render();
        glfwMakeContextCurrent(winHandle);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(winHandle);
        
        LARGE_INTEGER WorkCounter = GetClock();
        SecondsElapsed = GetSecondsElapsed(LastCounter, WorkCounter);
        LastCounter.QuadPart = WorkCounter.QuadPart;
        
        if (TargetFPSInSec > SecondsElapsed) {
            DWORD Delta = (DWORD)((1000.0) * (TargetFPSInSec - SecondsElapsed));
            if (Delta > 0) {
                Sleep(Delta);
            }
        }
        
        
        
        
    }
    return 0;
}
