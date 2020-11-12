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


#include <stdio.h>
#include <ShlObj.h>
#include <Windows.h>


#define TIMED_BLOCK__(Number, ...) timed_block timed_##Number(__COUNTER__, __LINE__, (const char*)__FUNCTION__);
#define TIMED_BLOCK_(Number, ...) TIMED_BLOCK__(Number, ##__VA__ARGS__)
#define TIMED_BLOCK(...)          TIMED_BLOCK_(__LINE__, ##__VA__ARGS__)


#define TIMER_END_TRANSLATION debug_record GlobalDebugRecord_Optimized[__COUNTER__ + 1];

struct debug_record {
    const char* FunctionName;

    uint64_t Clocks;

    uint32_t ThreadIndex;
    uint32_t LineNumber;
};

debug_record* GlobalDebugRecordArray = NULL;

struct timed_block {

    debug_record* mRecord;

    timed_block(int Counter, int LineNumber, const char *FunctionName) {
        
        mRecord = GlobalDebugRecordArray + Counter;
        mRecord->FunctionName = FunctionName;
        mRecord->LineNumber = LineNumber;
        mRecord->ThreadIndex = 0;
        mRecord->Clocks -= __rdtsc();

    }

    ~timed_block() {
        mRecord->Clocks += __rdtsc();
    }

};



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
    

    TIMED_BLOCK();

}

struct nar_record { UINT32 StartPos; UINT32 Len; };
inline BOOL
IsRegionsCollide(nar_record R1, nar_record R2) {
    BOOL Result = FALSE;
    UINT32 R1EndPoint = R1.StartPos + R1.Len;
    UINT32 R2EndPoint = R2.StartPos + R2.Len;

    if (R1.StartPos == R2.StartPos && R1.Len == R2.Len) {
        return TRUE;
    }

    if ((R1EndPoint <= R2EndPoint
        && R1EndPoint >= R2.StartPos)
        || (R2EndPoint <= R1EndPoint
            && R2EndPoint >= R1.StartPos)
        ) {
        Result = TRUE;
    }

    return Result;
}


void
MergeRegions(nar_record* R, int Len, int* NewLen) {

    UINT32 MergedRecordsIndex = 0;
    UINT32 CurrentIter = 0;

    for (;;) {
        if (CurrentIter >= Len) {
            break;
        }

        UINT32 EndPointTemp = R[CurrentIter].StartPos + R[CurrentIter].Len;

        if (IsRegionsCollide(&R[MergedRecordsIndex], &R[CurrentIter])) {
            UINT32 EP1 = R[CurrentIter].StartPos + R[CurrentIter].Len;
            UINT32 EP2 = R[MergedRecordsIndex].StartPos + R[MergedRecordsIndex].Len;

            EndPointTemp = MAX(EP1, EP2);
            R[MergedRecordsIndex].Len = EndPointTemp - R[MergedRecordsIndex].StartPos;

            CurrentIter++;
        }
        else {
            MergedRecordsIndex++;
            R[MergedRecordsIndex] = R[CurrentIter];
        }


    }

    *NewLen = MergedRecordsIndex + 1;
    R = (nar_record*)realloc(R, sizeof(nar_record) * (*NewLen));

}


int
main() {

    NarGlfwErrorCallback(0, 0);

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
    //glfwSwapInterval(1);

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

    static char CharBuffer[64];

    LARGE_INTEGER LastCounter = GetClock();

    while (!glfwWindowShouldClose(winHandle)) {
        

        Sleep(1);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowTestWindow();

#if 0

#pragma region Render available volumes list

        {//Render available volumes list
            ImGui::Begin("Available volumes", (bool*)0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

            if (ImGui::Button("Refresh list")) {

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
                    if (ImGui::Button("Select disk, wipe and restore it", { WindowSize.x , WindowSize.y / 4.f })) {
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

                        ImGui::Text("%I64d", Disks.Data[i].Size);
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

#endif


        LARGE_INTEGER WorkCounter = GetClock();
        float SecondsElapsed = GetSecondsElapsed(LastCounter, WorkCounter);
        LastCounter.QuadPart = WorkCounter.QuadPart;

        ImGui::Begin("testw");
        ImGui::Text("Seconds elapsed %f", SecondsElapsed);
        ImGui::Text("FPS: %f", 1.f / SecondsElapsed);
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

TIMER_END_TRANSLATION