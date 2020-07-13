#define _CRT_SECURE_NO_WARNINGS

#include "glad\glad.h"
#include "GLFW\glfw3.h"

#include "glad.c"
#include <vector>

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
#include <Windows.h>

enum RestoreWindow {
  RestoreWindow_BehaviourSelect,
  RestoreWindow_WipeDisk,
  RestoreWindow_WipePartition
};

enum BackupPopupState {
  BackupPopup_Type,
  BackupPopup_Task
};

#define NAR_MAX_VOL_COUNT 20
#define NAR_MAX_BACKUP_COUNT 64
#define NAR_BACKUP_BUFFER_SIZE Megabyte(32)

#define NAR_OP_ALLOCATE 1
#define NAR_OP_FREE 2
#define NAR_OP_ZERO 3

#define NAR_IMGUI_LOGWINDOW_NAME "NarLog"
#define ACTIVE_THREAD(thread) ((thread) == INVALID_HANDLE_VALUE)



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


/*
WARNING
this function probably not safe, use with caution. pass Out parameter with len _MAX_PATH to ensure buffer overflow
this is just snipped copied from internet, not reviewed at all
*/
BOOLEAN
NarSelectDirectoryFromWindow(wchar_t* Out) {
  if (!Out) return FALSE;

  BOOLEAN Result = FALSE;

  BROWSEINFOW BI = { 0 };
  BI.ulFlags = BIF_RETURNONLYFSDIRS;
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


LOG_CONTEXT GlobalCtx;


volatile struct BackupThreadParameters{
  float PercentageDone;// 0 - 1.f
  BackupType BT;
  wchar_t Letter;
  wchar_t RootDir[_MAX_PATH]; // Root directory to copy files, if NULL, files will be generated at exe's location  
  BOOLEAN IsThreadActive;
  BOOLEAN CloseFlag; 
};

struct BackupScheduler {
  BackupThreadParameters BTP;
  HANDLE Thread; //
  double TimeRemaining; 
  double TimeBase;
  BOOLEAN Enabled; 
};


struct RestoreThreadParameters {
  restore_inf R;
  float PercentageDone;
  BOOLEAN Succeeded;
  BOOLEAN WipeDisk;
  BOOLEAN DiskID; // If not WipeDisk, this value can be ignored
  BOOLEAN Finished;
};


BackupScheduler
InitBackupScheduler(char Letter, BackupType BT = BackupType::Diff, double TaskTime = 60.0 * 1000.0 * 1000.0, BOOLEAN Start = TRUE) {
  BackupScheduler BSP = { 0 };
  BSP.BTP.BT = BT;
  BSP.BTP.IsThreadActive = FALSE;
  BSP.BTP.Letter = Letter;
  BSP.BTP.PercentageDone = 0.f;
  BSP.BTP.RootDir[0] = '\n';
  BSP.BTP.CloseFlag = FALSE;

  BSP.Enabled = TRUE;
  BSP.Thread = INVALID_HANDLE_VALUE;
  BSP.TimeBase = TaskTime;
  BSP.TimeRemaining = BSP.TimeBase;
  return BSP;
}

BOOLEAN
NarInitContext(LOG_CONTEXT* Ctx) {
  if (!Ctx) return FALSE;

  memset(Ctx, 0, sizeof(*Ctx));
  Ctx->Port = INVALID_HANDLE_VALUE;
  
  return SetupVSS() && ConnectDriver(Ctx);
}


// Problem is, cache locality might fck up entire thread system, so try these with caution

DWORD
RestoreThread(void* Parameter) {
  
  BOOLEAN Result = FALSE;

  RestoreThreadParameters* RTP = (RestoreThreadParameters*)Parameter;
  if (RTP->WipeDisk) {
    if (OfflineRestoreCleanDisk(&RTP->R, RTP->DiskID)) {
      Result = TRUE;
    }
    else {
      // NOTE failed
    }
  }
  else {
    if (OfflineRestoreToVolume(&RTP->R, TRUE)) {
      Result = TRUE;
    }
    else {
      // NOTE failed
    }
  }

  RTP->Succeeded = Result;
  RTP->Finished = TRUE;

  return 0;
}

DWORD
BackupThread(void *Parameter) {
  
  // WaitForSingleObject(context.ShutDown, INFINITE);

  BackupThreadParameters *BTP = (BackupThreadParameters*)Parameter;
  BTP->IsThreadActive = TRUE;
  DotNetStreamInf SI;
  HANDLE BinaryFile = INVALID_HANDLE_VALUE;
  HANDLE MetadataFile = INVALID_HANDLE_VALUE;
  
  wchar_t *MetadataFileName = 0;
  wchar_t *BinaryFileName = 0;
  
  memset(&SI, 0, sizeof(SI));

  if (SetupStream(&GlobalCtx, BTP->Letter, BTP->BT, &SI)) {
    
    int VolId = GetVolumeID(&GlobalCtx, BTP->Letter);
    if (VolId != NAR_INVALID_VOLUME_TRACK_ID) {
      
      volume_backup_inf *V = &GlobalCtx.Volumes.Data[VolId];

      BinaryFileName = (wchar_t*)malloc(sizeof(wchar_t) * (_MAX_PATH + SI.FileName.length()));
      
      wcscpy(BinaryFileName, BTP->RootDir);
      wcscat(BinaryFileName, L"\\");
      wcscat(BinaryFileName, SI.FileName.c_str());
      

      BinaryFile = CreateFileW(BinaryFileName, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
      if (BinaryFile != INVALID_HANDLE_VALUE) {

        void* Buffer = malloc(NAR_BACKUP_BUFFER_SIZE);
        ULONGLONG CopySize = (ULONGLONG)SI.ClusterCount * (ULONGLONG)SI.ClusterSize;
        UINT32 MaxItTime = CopySize / NAR_BACKUP_BUFFER_SIZE;

        while (BTP->CloseFlag != TRUE) {
          ReadStream(V, Buffer, NAR_BACKUP_BUFFER_SIZE);
          MaxItTime--;
        }

        free(Buffer); Buffer = 0;

        if (MaxItTime <= 0) {
          DebugBreak();
          TerminateBackup(V, FALSE);
        }
        else {
          BTP->PercentageDone = 1.0f;
          TerminateBackup(V, TRUE);

          MetadataFileName = (wchar_t*)malloc(sizeof(wchar_t) * (_MAX_PATH + SI.MetadataFileName.length()));
          wcscpy(MetadataFileName, BTP->RootDir);
          wcscat(MetadataFileName, L"\\");
          wcscat(MetadataFileName, SI.FileName.c_str());
          CopyFileW(SI.MetadataFileName.c_str(), MetadataFileName, TRUE);
        }

      }
      else {
        DebugBreak();
      }

      
      
    }
    else {
      // NOTE(Batuhan): That shouldnt be possible
    }

  }
  else {
    // NOTE(Batuhan): That MIGHT be possible
  }
  
  CloseHandle(BinaryFile);
  CloseHandle(MetadataFile);
  free(MetadataFileName);
  free(BinaryFileName);

  
  BTP->IsThreadActive = FALSE;
  
  return 0;
}

BOOLEAN
StartRestore(RestoreThreadParameters *RTP) {
  return CreateThread(0, 0, RestoreThread, RTP, 0, 0) != INVALID_HANDLE_VALUE;
}

BOOLEAN
StartBackup(BackupScheduler* BSP) {
  if (!BSP) return FALSE;

  BOOLEAN Result = FALSE;
  if (BSP->Thread == INVALID_HANDLE_VALUE) {
    BSP->BTP.PercentageDone = 0.f;
    BSP->BTP.IsThreadActive = TRUE;
    BSP->TimeRemaining = BSP->TimeBase;

    BSP->Thread = CreateThread(0, 0, BackupThread, BSP, 0, 0);
    if (BSP->Thread != INVALID_HANDLE_VALUE) {
      Result = TRUE;
    }
  }
  else {
    // thread already exists
  }


  return Result;
}


inline data_array<volume_information>
GetVolumesNotOnTrack() {
  data_array<volume_information> Result = { 0 };

  data_array<volume_information> AllVolumes = NarGetVolumes();
  
  for (int i = 0; i < AllVolumes.Count; i++) {
    if (GetVolumeID(&GlobalCtx, AllVolumes.Data[i].Letter) == NAR_INVALID_VOLUME_TRACK_ID) {
      Result.Insert(AllVolumes.Data[i]);
    }
  }

  return Result;
}

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

void NarGlfwErrorCallback(int error_code, const char* description) {
  printf("Err occured with code %i and description", error_code, description);
  printf(description);
  printf("\n");
}

int
main() {
  //HINSTANCE h1, HINSTANCE h2, LPSTR h3, INT h4
  LARGE_INTEGER PerfCountFreqResult;
  QueryPerformanceFrequency(&PerfCountFreqResult);
  GlobalPerfCountFreq = PerfCountFreqResult.QuadPart;

  glfwSetErrorCallback(NarGlfwErrorCallback);

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
    //ErrorLog("Failed to initialize GLAD\n");
    return -2;
  }

  glViewport(0, 0, 800, 600);


  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(winHandle, true);
  ImGui_ImplOpenGL3_Init("#version 140");

  glClearColor(1.f, 1.0f, 1.0f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


  //Setup variables
  bool RefreshAvailableVolumes = FALSE;
  bool RefreshTrackedVolumes = FALSE;
  bool DiffSelected = FALSE;
  bool IncSelected = FALSE;
  bool Cancel = FALSE;

  bool BackupSelection[NAR_MAX_VOL_COUNT];
  memset(BackupSelection, 0, sizeof(BackupSelection));

  bool TaskSelected[NAR_MAX_BACKUP_COUNT];
  memset(TaskSelected, 0, sizeof(TaskSelected));

  // Init global log context
  NarInitContext(&GlobalCtx);

  wchar_t CurrentDirectory[_MAX_PATH];
  memset(CurrentDirectory, 0, _MAX_PATH * sizeof(wchar_t));

  std::vector<BackupScheduler> BSVector;
  BSVector.reserve(100);

  BackupScheduler a;


  backup_metadata* BackupsInDir = (backup_metadata*)NarAllocate(sizeof(backup_metadata) * NAR_MAX_BACKUP_COUNT);
  int BackupCountInDir = 0;

  data_array<disk_information> Disks = { 0 };
  data_array<volume_information> NonTrackedVolumes = GetVolumesNotOnTrack();

  int RestoreVersionIndex = 0;
  int WipeIndex = 0;

  static char CharBuffer[64];

  LARGE_INTEGER LastCounter = GetClock();

  while (!glfwWindowShouldClose(winHandle)) {

    Sleep(13);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowTestWindow();

#pragma region Render available volumes list

    {//Render available volumes list
      ImGui::Begin("Available volumes", (bool*)0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
      
      if (ImGui::Button("Refresh list")) {
        FreeDataArray(&NonTrackedVolumes);
        NonTrackedVolumes = GetVolumesNotOnTrack();
      }
      static BackupPopupState BPS = BackupPopup_Type;

      if (ImGui::Button("Backup"))
      {
        ImGui::OpenPopup("Backup type selector");
        BPS = BackupPopup_Type;
      }
      else {
        ImGui::CloseCurrentPopup();
      }

      // NOTE(Batuhan): Draw popup
      if (ImGui::BeginPopupModal("Backup type selector", 0, ImGuiWindowFlags_NoResize)) {

        if (BPS == BackupPopup_Type) {

          static ImVec2 WindowSize = { 280, 130 };
          ImGui::SetWindowSize(WindowSize);

          ImGui::TextWrapped("Select backup type for selected volume(s)");
          ImGui::Separator();
          DiffSelected = ImGui::Button("Diff", { WindowSize.x / 4.f, WindowSize.y / 5.f });
          ImGui::SameLine();
          IncSelected = ImGui::Button("Inc", { WindowSize.x / 4.f, WindowSize.y / 5.f });
          ImGui::SameLine();
          Cancel = ImGui::Button("CANCEL", { WindowSize.x / 4.f, WindowSize.y / 5.f });

          if (DiffSelected || IncSelected || Cancel) {
            BPS = BackupPopup_Task;
          }
          if (Cancel) {
            ImGui::CloseCurrentPopup();
          }
        
        }
        
        if (BPS == BackupPopup_Task) {
          static INT32 Time = 0;
          static bool RightNow = false;
          static int HourSet = 0; // 0-24
          static int MinuteSet = 0; // 0-60
          static SYSTEMTIME ST = { 0 };
          GetLocalTime(&ST);
          
          ImGui::SetWindowSize({ 350,200 }); 
          
          ImGui::Text("These settings will effect all selected volumes");
          ImGui::Text("Specify minutes between backups");
          ImGui::Checkbox("Right now", &RightNow);
          if (!RightNow) {

            HourSet = MIN(MAX(HourSet, ST.wHour), 23);
            if (ST.wHour == HourSet) {
              MinuteSet = MIN(MAX(MinuteSet, ST.wMinute), 59);
            }
            else {
              MinuteSet = MIN(MAX(MinuteSet, 0), 59);
            }
            
            ImGui::InputScalar(":", ImGuiDataType_U32, &HourSet);
            ImGui::SameLine();
            ImGui::InputScalar("Minute", ImGuiDataType_U32, &MinuteSet);
          }

          ImGui::InputInt("Task timer(min)", &Time);
          Time = MAX(Time, 0);

          if (ImGui::Button("Start task")) {
            double AheadTime = 0.0;
            if (!RightNow) {
              
              AheadTime = ((double)HourSet - ST.wHour) * (60.0 * 1000.0);
              if (HourSet != ST.wHour) {
                
                if (ST.wMinute > MinuteSet) {
                  AheadTime -= ((double)ST.wMinute - (double)MinuteSet) * 1000.0;
                }
                else {
                  AheadTime += ((double)MinuteSet + (double)ST.wMinute) * 1000.0;
                } 

              }
              else {
                AheadTime += ((double)MinuteSet + (double)ST.wMinute) * 1000.0;
              }
              
            }

            BackupType BT = DiffSelected ? BackupType::Diff : BackupType::Inc;
            if (Time > 0) {
              for (int i = 0; i < NonTrackedVolumes.Count; i++) {
                if (BackupSelection[i]) {
                  BackupScheduler BS = InitBackupScheduler(NonTrackedVolumes.Data[i].Letter, BT, Time);
                  if (!RightNow) {
                    BS.TimeRemaining = AheadTime;
                  }
                  else {
                    StartBackup(&BS);
                  }
                  BSVector.emplace_back(BS);
                }
              }
            }
            ImGui::CloseCurrentPopup();
          }
          if (ImGui::Button("Cancel")) {
            BPS = BackupPopup_Type;
            ImGui::CloseCurrentPopup();
          }

        }

        
        ImGui::EndPopup();
      }

      if (DiffSelected || IncSelected) {
        // NOTE(Batuhan): For all selected element
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

        for (int i = 0; i < NonTrackedVolumes.Count; i++) {
          sprintf(CharBuffer, "##%i", i);

          ImGui::Checkbox(CharBuffer, &BackupSelection[i]);
          ImGui::NextColumn();

          ImGui::Text("%c", NonTrackedVolumes.Data[i].Letter);
          ImGui::NextColumn();

          ImGui::Text("%I64d", NonTrackedVolumes.Data[i].Size);
          ImGui::NextColumn();

          if (NonTrackedVolumes.Data[i].DiskType == NAR_DISKTYPE_GPT) {
            ImGui::Text("GPT");
            ImGui::NextColumn();
          }
          if (NonTrackedVolumes.Data[i].DiskType == NAR_DISKTYPE_MBR) {
            ImGui::Text("MBR");
            ImGui::NextColumn();
          }
          if (NonTrackedVolumes.Data[i].DiskType == NAR_DISKTYPE_RAW) {
            ImGui::Text("RAW");
            ImGui::NextColumn();
          }

          ImGui::Text("%i", NonTrackedVolumes.Data[i].DiskID);
          ImGui::NextColumn();
          ImGui::Separator();

        }

      }
      // Render available volumes list

    } 

    ImGui::End();
#pragma endregion

#pragma region Render backups in directory and restore option

    {
      ImGui::Begin("Restore panel", (bool*)0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
      ImGui::SetWindowSize({ 750,220 });

      ImGui::Text("Current dir : %S", CurrentDirectory);
      if (ImGui::Button("Set directory to store and lookup backups")) {

        if (NarSelectDirectoryFromWindow(CurrentDirectory)) {
          if (NarGetBackupsInDirectory(CurrentDirectory, BackupsInDir, NAR_MAX_BACKUP_COUNT * sizeof(backup_metadata), &BackupCountInDir)) {

          }
          else {
            //NOTE(Batuhan): Error occured while fetching backup metadatas, log this via imgui
          }
        }
        else {
          //NOTE(Batuhan): Error occured while selecting new directory, log this via imgui
        }

      }

      ImGui::SameLine();
      if (ImGui::Button("Restore selected version")) {
        ImGui::OpenPopup("Restore popup");
      }
      else {
        ImGui::CloseCurrentPopup();
      }

      if (ImGui::BeginPopupModal("Restore popup", 0, ImGuiWindowFlags_NoResize)) {
        
        static RestoreWindow RW = RestoreWindow_BehaviourSelect;
        static ImVec2 WindowSize = { 420, 160 };

        ImGui::SetWindowSize(WindowSize);

        if (RW == RestoreWindow_BehaviourSelect) {
          if (ImGui::Button("Select disk, wipe and restore it", { WindowSize.x , WindowSize.y / 4.f})) {
            FreeDataArray(&Disks);
            Disks = NarGetDisks();
            RW = RestoreWindow_WipeDisk;
          }
          if (ImGui::Button("Select partition, wipe and restore it", { WindowSize.x, WindowSize.y / 4.f })) {
            // NOTE exclude currently tracking volumes from wipe list, or make checkbox to include them.
            
            FreeDataArray(&NonTrackedVolumes);
            NonTrackedVolumes = GetVolumesNotOnTrack();

            RW = RestoreWindow_WipePartition;
          }
        }
        if (RW == RestoreWindow_WipeDisk) {
          if (ImGui::Button("Restore to selected")) {
            
          }

          ImGui::Columns(3);
          {
            ImGui::Text("Selected");
            ImGui::NextColumn();

            ImGui::Text("DiskID");
            ImGui::NextColumn();

            ImGui::Text("DiskSize");
            ImGui::NextColumn();
          }
          
          for (int i = 0; i < Disks.Count; i++) {

            sprintf(CharBuffer, "##%i", i);
            ImGui::RadioButton(CharBuffer, &WipeIndex, i);
            ImGui::NextColumn();

            ImGui::Text("%i", Disks.Data[i].ID);
            ImGui::NextColumn();
            
            ImGui::Text("%I64d",Disks.Data[i].Size);
            ImGui::NextColumn();

          }
         

        }
        if (RW == RestoreWindow_WipePartition) {
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

            for (int i = 0; i < NonTrackedVolumes.Count; i++) {
              sprintf(CharBuffer, "##%i", i);

              ImGui::RadioButton(CharBuffer, &WipeIndex, i);
              ImGui::NextColumn();

              ImGui::Text("%c", NonTrackedVolumes.Data[i].Letter);
              ImGui::NextColumn();

              ImGui::Text("%I64d", NonTrackedVolumes.Data[i].Size);
              ImGui::NextColumn();

              if (NonTrackedVolumes.Data[i].DiskType == NAR_DISKTYPE_GPT) {
                ImGui::Text("GPT");
                ImGui::NextColumn();
              }
              if (NonTrackedVolumes.Data[i].DiskType == NAR_DISKTYPE_MBR) {
                ImGui::Text("MBR");
                ImGui::NextColumn();
              }
              if (NonTrackedVolumes.Data[i].DiskType == NAR_DISKTYPE_RAW) {
                ImGui::Text("RAW");
                ImGui::NextColumn();
              }

              ImGui::Text("%i", NonTrackedVolumes.Data[i].DiskID);
              ImGui::NextColumn();
              ImGui::Separator();

            }
          }
        }

         
        if (ImGui::Button("CANCEL")) {
        RESTOREPOPUPCANCELPOINT:

          ImGui::CloseCurrentPopup();
          RW = RestoreWindow_BehaviourSelect;
        }
        
        
        ImGui::EndPopup();

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
        ImGui::RadioButton("##0", &RestoreVersionIndex, 0);
        ImGui::NextColumn();

        ImGui::Text("%c", BackupsInDir[i].Letter);
        ImGui::NextColumn();

        ImGui::Text("%I64d", BackupsInDir[i].Size);
        ImGui::NextColumn();

        ImGui::Text("Version", BackupsInDir[i].Version);
        ImGui::NextColumn();

        if (BackupsInDir[i].BT == BackupType::Diff) {
          ImGui::Text("Differential");
        }
        if (BackupsInDir[i].BT == BackupType::Inc) {
          ImGui::Text("Incremental");
        }
        if (BackupsInDir[i].BT != BackupType::Inc || BackupsInDir[i].BT != BackupType::Diff) {
          ImGui::Text("Error!!");
        }
        ImGui::NextColumn();


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


#pragma region Render progress bar for backups and tasks


    ImGui::Begin("Active operations", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
    if (ImGui::Button("Delete task(s)")) {
      for (int i = 0; i < BSVector.size(); i++) {
        if (TaskSelected[i]) {
          BSVector[i].BTP.CloseFlag = TRUE;
          while (BSVector[i].BTP.IsThreadActive);
          RemoveVolumeFromTrack(&GlobalCtx, BSVector[i].BTP.Letter);
        }
      }
    }

    ImGui::Columns(4);
    {
      ImGui::Text("Selected");
      ImGui::NextColumn();

      ImGui::Text("Volume");
      ImGui::NextColumn();

      ImGui::Text("Progress");
      ImGui::NextColumn();

      //To inform user if task is counting down or operation started
      //float Percentage = 0.3f;
      //float ColumnWidth = ImGui::GetColumnWidth();
      //
      //sprintf(CharBuffer, "%i", (int)(Percentage * 100.f));
      //ImGui::ProgressBar(Percentage, { ColumnWidth,0 }, CharBuffer);

      ImGui::Text("Description");
      ImGui::NextColumn();

      ImGui::Separator();
    }


    for (int i = 0; i < BSVector.size(); i++) {

      static float Percentage = 0.f;
      sprintf(CharBuffer, "##%c", BSVector[i].BTP.Letter);

      ImGui::Checkbox(CharBuffer, &TaskSelected[i]);

      ImGui::Text("%c", BSVector[i].BTP.Letter);
      ImGui::NextColumn();

      float ColumnWidth = ImGui::GetColumnWidth();
      if ((BSVector[i].BTP.IsThreadActive)) {
        // Render backup done in percentage, based on how many bytes transferred so far
        Percentage = (float)((double)1.0 - BSVector[i].TimeRemaining / BSVector[i].TimeBase);
        sprintf(CharBuffer, "%i", (int)(Percentage * 100.f));

        ImGui::ProgressBar(Percentage, { ColumnWidth,0 }, "test overlay");
        ImGui::NextColumn();

        ImGui::Text("Backup in progress");
        ImGui::NextColumn();
      }
      else {
        // Render remaining time for next backup
        Percentage = (float)((double)1.0 - BSVector[i].TimeRemaining / BSVector[i].TimeBase);
        sprintf(CharBuffer, "%i", (int)(Percentage * 100.f));
        ImGui::ProgressBar(Percentage, { ColumnWidth,0 }, CharBuffer);
        ImGui::NextColumn();

        ImGui::Text("Waiting next scheduled time");
        ImGui::NextColumn();
      }

    }

    

    ImGui::End();
#pragma endregion


    LARGE_INTEGER WorkCounter = GetClock();
    float SecondsElapsed = GetSecondsElapsed(LastCounter, WorkCounter);
    LastCounter.QuadPart = WorkCounter.QuadPart;

    
    for (auto& it : BSVector) {
      
      if (!it.BTP.IsThreadActive) {
        it.TimeRemaining -= SecondsElapsed;
      }

      if (it.TimeRemaining <= 0.f) {
        StartBackup(&it);
      }
      
    }

    ImGui::Begin("Testwindow123");

    ImGui::Text("Seconds elapsed %f", SecondsElapsed);

    ImGui::End();



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