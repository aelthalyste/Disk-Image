#define _CRT_SECURE_NO_WARNINGS

#include "glad\glad.h"
#include "GLFW\glfw3.h"

#include "glad.c"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imstb_textedit.h"
#include "imstb_truetype.h"


#include "minispy.h"
#include "mspyLog.h"
#include "mspyLog.cpp"
#include "mspyUser.cpp"


#include <stdio.h>


int main() {
  

  glfwInit();

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow* winHandle = glfwCreateWindow(800, 600, "demotitle", NULL, NULL);

  glfwMakeContextCurrent(winHandle);
  glfwSwapInterval(1);

  if (winHandle == nullptr) {
    return -1;
  }

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    //ErrorLog("Failed to initialize GLAD\n");
    return -2;
  }

  glViewport(0, 0, 800, 600);


  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(winHandle, true);
  ImGui_ImplOpenGL3_Init("#version 140");

  glClearColor(1.f, 1.0f, 1.0f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  bool cb1 = false;
  bool Var = FALSE;

  while (!glfwWindowShouldClose(winHandle)) {
    Sleep(8);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowTestWindow();


    ImGui::Begin("Custom layout", (bool*)0, ImGuiWindowFlags_MenuBar);
    
    ImGui::Columns(4);
    {
      ImGui::Checkbox("Selected", &Var);
      ImGui::NextColumn();

      ImGui::Text("DiskType");
      ImGui::NextColumn();

      ImGui::Checkbox("DiskID", &Var);
      ImGui::NextColumn();

      ImGui::Text("Size");
      ImGui::Separator();

      ImGui::NextColumn();
      ImGui::Text("Newlinetest");
    }

    


    ImGui::End();




    glClearColor(1.f, 1.0f, 1.0f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ImGui::Render();
    glfwMakeContextCurrent(winHandle);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(winHandle);
    glfwPollEvents();


  }
  return 0;
}