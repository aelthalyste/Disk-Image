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
#include <ShlObj.h>


#define NAR_MAX_VOL_COUNT 20
#define NAR_MAX_BACKUP_COUNT 64

#define NAR_OP_ALLOCATE 1
#define NAR_OP_FREE 2
#define NAR_OP_ZERO 3

static inline void*
_InternalNarMemoryOp(int OpCode, size_t Size) {
  struct {
    void* P;
    size_t ReserveSize;
    size_t Used;
  }static MemArena = { 0 };

  if (!MemArena.P) {
    MemArena.ReserveSize = 1024LL * 1024LL * 1024LL * 64LL; // Reserve 64GB
    MemArena.Used = 0;
    MemArena.P = VirtualAlloc(0, MemArena.ReserveSize, MEM_RESERVE, PAGE_READWRITE);
  }

  void* Result = 0;

  if (OpCode == NAR_OP_ALLOCATE) {
    VirtualAlloc(MemArena.P, Size + MemArena.Used, MEM_COMMIT, PAGE_READWRITE);
    Result = (char*)MemArena.P + MemArena.Used;
    MemArena.Used += Size;
  }
  if (OpCode == NAR_OP_FREE) {

    if (VirtualFree(MemArena.P, MemArena.Used, MEM_DECOMMIT) == 0) {
      printf("Cant free scratch memory\n");
    }
    MemArena.Used = 0;

  }
  if (OpCode == NAR_OP_ZERO) {
    memset(MemArena.P, 0, MemArena.Used);
  }

  return Result;
}

static inline void*
NarAllocate(size_t Size) {
  return _InternalNarMemoryOp(NAR_OP_ALLOCATE, Size);
}

static inline void
NarClear() {
  _InternalNarMemoryOp(NAR_OP_FREE, 0);
}

BOOLEAN
NarSelectDirectoryFromWindow(wchar_t* Out) {
  if (!Out) return FALSE;
  
  BOOLEAN Result = FALSE;
  
  BROWSEINFOW BI = { 0 };
  BI.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
  BI.hwndOwner = 0;

  ::OleInitialize(NULL);

  LPITEMIDLIST pIDL = ::SHBrowseForFolder(&BI);
  
  if (pIDL != NULL)
  {
    // Create a buffer to store the path, then 
    // get the path.
    if (::SHGetPathFromIDListW(pIDL, Out) != 0)
    {
      Result = TRUE;
    }

    // free the item id list
    CoTaskMemFree(pIDL);
  }

  ::OleUninitialize();

  return Result;
}


int
WinMain(HINSTANCE h1, HINSTANCE h2, LPSTR h3, INT h4) {
  glfwInit();

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow* winHandle = glfwCreateWindow(1280, 720, "demotitle", NULL, NULL);

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

  bool RefreshAvailableVolumes = FALSE;
  bool RefreshTrackedVolumes = FALSE;
  bool BackupTypePopUp = FALSE;
  bool DiffSelected = FALSE;
  bool IncSelected = FALSE;
  bool Cancel = FALSE;
  bool Bs[NAR_MAX_VOL_COUNT];
  wchar_t CurrentDirectory[_MAX_PATH];
  memset(CurrentDirectory, 0, _MAX_PATH * sizeof(wchar_t));

  backup_metadata* BackupsInDir = (backup_metadata*)NarAllocate(sizeof(backup_metadata) * NAR_MAX_BACKUP_COUNT);
  int BackupCountInDir = 0;

  memset(Bs, 0, sizeof(Bs));

  int ButtonClicked = 0;
  
  auto volumes = NarGetVolumes();
  volume_information v;
  v.Bootable = false;
  v.DiskID = 2;
  v.DiskType = NAR_DISKTYPE_MBR;
  v.Letter = 'P';
  v.Size = 123123;
  volumes.Insert(v);

  v.Bootable = false;
  v.DiskID = 4;
  v.DiskType = NAR_DISKTYPE_RAW;
  v.Letter = 'O';
  v.Size = 4322;
  volumes.Insert(v);

  v.Bootable = true;
  v.DiskID = 1;
  v.DiskType = NAR_DISKTYPE_GPT;
  v.Letter = 'W';
  v.Size = 98723212;
  volumes.Insert(v);
  static char CharBuffer[64];

  while (!glfwWindowShouldClose(winHandle)) {

    Sleep(5);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowTestWindow();

#pragma region Render available volumes list

    {//Render available volumes list
      ImGui::Begin("Available volumes", (bool*)0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
      BackupTypePopUp = ImGui::Button("Backup");

      if (BackupTypePopUp)
      {
        ImGui::OpenPopup("Backup type selector");
      }
      else {
        ImGui::CloseCurrentPopup();
      }

      // NOTE(Batuhan): Draw popup
      if (ImGui::BeginPopupModal("Backup type selector", 0, ImGuiWindowFlags_NoResize))
      {
        static ImVec2 WindowSize = { 250, 120 };
        ImGui::SetWindowSize(WindowSize);


        ImGui::TextWrapped("Select backup type for selected volume(s)");
        ImGui::Separator();
        DiffSelected = ImGui::Button("Diff", { WindowSize.x / 4.f, WindowSize.y / 5.f });
        ImGui::SameLine();
        IncSelected = ImGui::Button("Inc", { WindowSize.x / 4.f, WindowSize.y / 5.f });
        ImGui::SameLine();
        Cancel = ImGui::Button("CANCEL", { WindowSize.x / 4.f, WindowSize.y / 5.f });

        if (DiffSelected || IncSelected || Cancel) {
          BackupTypePopUp = FALSE;
          ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
      }

      if (DiffSelected || IncSelected) {
        // TODO do things
      }


      ImGui::Columns(5);
      {
        // Entry values for table 
        ImGui::Text("Selected");
        ImGui::NextColumn();

        ImGui::Text("Volume Letter");
        ImGui::NextColumn();

        ImGui::Text("Size");
        ImGui::NextColumn();

        ImGui::Text("DiskType");
        ImGui::NextColumn();

        ImGui::Text("DiskID");
        ImGui::NextColumn();

        ImGui::Separator();

        //Render table
        
        for (int i = 0; i < volumes.Count; i++) {
          sprintf(CharBuffer, "##%i", i);
          
          ImGui::Checkbox(CharBuffer, &Bs[i]);
          ImGui::NextColumn();

          ImGui::Text("%c", volumes.Data[i].Letter);
          ImGui::NextColumn();

          ImGui::Text("%I64d", volumes.Data[i].Size);
          ImGui::NextColumn();

          if (volumes.Data[i].DiskType == NAR_DISKTYPE_GPT) {
            ImGui::Text("GPT");
            ImGui::NextColumn();
          }
          if (volumes.Data[i].DiskType == NAR_DISKTYPE_MBR) {
            ImGui::Text("MBR");
            ImGui::NextColumn();
          }
          if (volumes.Data[i].DiskType == NAR_DISKTYPE_RAW) {
            ImGui::Text("RAW");
            ImGui::NextColumn();
          }

          ImGui::Text("%i", volumes.Data[i].DiskID);
          ImGui::NextColumn();
          ImGui::Separator();

        }

      }
      ImGui::End();
    } // Render available volumes list

#pragma endregion

#pragma region Render backups in directory

    {
      ImGui::Begin("Restore panel", (bool*)0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
      ImGui::SetWindowSize({ 750,220 });

      ImGui::Text("Current dir : %S", CurrentDirectory);
      if (ImGui::Button("Set directory to store and lookup backups")) {
        
        if (NarSelectDirectoryFromWindow(CurrentDirectory)) {
          if (NarGetBackupsInDirectory(CurrentDirectory, BackupsInDir, NAR_MAX_BACKUP_COUNT * sizeof(backup_metadata), &BackupCountInDir)) {
            
          }
          else {
            //NOTE(BATUHAN): Error occured while fetching backup metadatas, log this via imgui
          }
        }
        else {
          //NOTE(BATUHAN): Error occured while selecting new directory, log this via imgui
        }

      }
      
      ImGui::SameLine();
      if (ImGui::Button("Restore selected")) {

      }
      ImGui::Separator();

#pragma region render property table header

      ImGui::Columns(6);
      ImGui::Text("Selected");
      ImGui::NextColumn();

      ImGui::Text("Volume");
      ImGui::NextColumn();

      ImGui::Text("Size");
      ImGui::NextColumn();

      ImGui::Text("Version");
      ImGui::NextColumn();

      ImGui::Text("Backup Type");
      ImGui::NextColumn();

      ImGui::Text("Botable");
      ImGui::NextColumn();
      
      ImGui::Separator();

#pragma endregion

      for (int i = 0; i < BackupCountInDir; i++) {
        sprintf(CharBuffer, "  ##%i", i);
        ImGui::RadioButton("##0", &ButtonClicked, 0);
        ImGui::NextColumn();

        ImGui::Text("%c", BackupsInDir[i].Letter);
        ImGui::NextColumn();

        ImGui::Text("%I64d", BackupsInDir[i].Size);
        ImGui::NextColumn();

        ImGui::Text("Version", BackupsInDir[i].Version);
        ImGui::NextColumn();

        if (BackupsInDir[i].BT == BackupType::Diff) {
          ImGui::Text("Differential");
          ImGui::NextColumn();
        }
        if (BackupsInDir[i].BT == BackupType::Inc) {
          ImGui::Text("Incremental");
          ImGui::NextColumn();
        }

        if (BackupsInDir[i].IsOSVolume) {
          ImGui::Text("Yes");  
        }
        else {
          ImGui::Text("No");
        }
        ImGui::NextColumn();

        
        ImGui::Text("TestText1");
        ImGui::NextColumn();

      }


      ImGui::End();
    }

#pragma endregion
    

    glClearColor(.2f, .2f, .2f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ImGui::Render();
    glfwMakeContextCurrent(winHandle);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(winHandle);
    glfwPollEvents();


  }
  return 0;
}