#include <Windows.h>
#include <stdio.h>
#include <string>
#include <atlbase.h>
#include <vds.h>
#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <vsmgmt.h>

#include "iphlpapi.h"

char G_PipeBuffer[512];
FILE *Log = 0;
#define LOG(fmt, ...) do{fprintf(Log, fmt, __VA_ARGS__); fflush(Log); }while(0);
#define WINAPI_LOG(str) do{fprintf(Log, "%s, Err code %d\n", str, GetLastError());}while(0);

BOOLEAN
SetupVSS() {
    /* 
        NOTE(Batuhan): in managed code we dont need to initialize these stuff. since i am shipping code as textual to wrapper, i can detect clr compilation and switch to correct way to initialize
        vss stuff
     */
    
    
    
    BOOLEAN Return = TRUE;
    HRESULT hResult = 0;
    
    // TODO (Batuhan): Remove that thing if 
    hResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (!SUCCEEDED(hResult)) {
        LOG("Failed CoInitialize function %d\n", hResult);
        Return = FALSE;
    }
    
    hResult = CoInitializeSecurity(
                                   NULL,                           //  Allow *all* VSS writers to communicate back!
                                   -1,                             //  Default COM authentication service
                                   NULL,                           //  Default COM authorization service
                                   NULL,                           //  reserved parameter
                                   RPC_C_AUTHN_LEVEL_PKT_PRIVACY,  //  Strongest COM authentication level
                                   RPC_C_IMP_LEVEL_IMPERSONATE,    //  Minimal impersonation abilities
                                   NULL,                           //  Default COM authentication settings
                                   EOAC_DYNAMIC_CLOAKING,          //  Cloaking
                                   NULL                            //  Reserved parameter
                                   );
    
    if (!SUCCEEDED(hResult)) {
        LOG("Failed CoInitializeSecurity function %d\n", hResult);
        Return = FALSE;
    }
    return Return;
    
}


VSS_ID
GetShadowPath(std::wstring Drive, CComPtr<IVssBackupComponents>& ptr, wchar_t* OutShadowPath, size_t MaxOutCh) {
    
    BOOLEAN Error = TRUE;
    VSS_ID sid = { 0 };
    HRESULT res;
    
    res = CreateVssBackupComponents(&ptr);
    
    if(nullptr == ptr){
        LOG("createbackup components returned %X\n", res);
        return sid;
        // early termination
    }
    
    if (S_OK == ptr->InitializeForBackup()) {
        if (S_OK == ptr->SetContext(VSS_CTX_BACKUP)) {
            if(S_OK == ptr->SetBackupState(false, false, VSS_BACKUP_TYPE::VSS_BT_COPY, false)){
                CComPtr<IVssAsync> async3;
                if(S_OK == ptr->GatherWriterMetadata(&async3)){
                    async3->Wait();
                    if (S_OK == ptr->StartSnapshotSet(&sid)) {
                        if (S_OK == ptr->AddToSnapshotSet((LPWSTR)Drive.c_str(), GUID_NULL, &sid)) {
                            CComPtr<IVssAsync> Async;
                            if (S_OK == ptr->PrepareForBackup(&Async)) {
                                Async->Wait();
                                CComPtr<IVssAsync> Async2;
                                if (S_OK == ptr->DoSnapshotSet(&Async2)) {
                                    Async2->Wait();
                                    Error = FALSE;
                                }
                                else{
                                    WINAPI_LOG("DoSnapshotSet failed\n");
                                }
                            }
                            else{
                                WINAPI_LOG("PrepareForBackup failed\n");
                            }
                        }
                        else{
                            WINAPI_LOG("AddToSnapshotSet failed\n");
                        }
                    }
                    else{
                        WINAPI_LOG("StartSnapshotSet failed\n");
                    }
                }
                else{
                    WINAPI_LOG("GatherWriterMetadata failed\n");
                }
            }
            else{
                WINAPI_LOG("SetBackupState failed\n");
            }
        }
        else{
            WINAPI_LOG("SetContext failed\n");
        }
    }
    else{
        WINAPI_LOG("InitializeForBackup failed\n");
    }
    
    if (!Error) {
        VSS_SNAPSHOT_PROP SnapshotProp;
        ptr->GetSnapshotProperties(sid, &SnapshotProp);
        
        size_t l = wcslen(SnapshotProp.m_pwszSnapshotDeviceObject);
        if(MaxOutCh > l){
            wcscpy(OutShadowPath, SnapshotProp.m_pwszSnapshotDeviceObject);
        }
        else{
            LOG("Shadow path can't fit given string buffer\n");
        }
    }
    
    
    return sid;
}


enum ProcessCommandType{
    ProcessCommandType_GetVSSPath,
    ProcessCommandType_TerminateVSS,
    ProcessCommandType_TerminateProcess
};



int AlternateMain(int argc, char *argv[]){
    
    wchar_t Drive[] = L"!:\\";
    HANDLE PipeHandle = INVALID_HANDLE_VALUE;
    CComPtr<IVssBackupComponents> VSSPTR;
    char MsgBuffer[512];
    LOG("Started!\n");
    if(argc != 2){
        LOG("Argument count must be 2\n");
        return 0;
    }
    
    
    Drive[0] = argv[1][0];
    SetupVSS();
    
    PipeHandle = CreateFileA(
                             argv[0],
                             GENERIC_READ | GENERIC_WRITE,
                             0,
                             NULL,
                             OPEN_EXISTING,
                             0,
                             NULL);
    
    if(PipeHandle == INVALID_HANDLE_VALUE){
        LOG("Unable to open pipe\n");
        return 0;
    }
    
    
    DWORD Garbage = 0;
    VSS_ID VSSID = {};
    
    while(1){
        
        if(ReadFile(PipeHandle, MsgBuffer, sizeof(MsgBuffer), &Garbage, 0) && Garbage == sizeof(MsgBuffer) ){
            
            
            uint32_t CType = *((uint32_t*)&MsgBuffer[0]);
            
            if(CType == ProcessCommandType_GetVSSPath){
                wchar_t OutShadowPath[1024] = {0};
                
                VSSID = GetShadowPath(Drive, VSSPTR, OutShadowPath, sizeof(OutShadowPath)/2);
                memset(MsgBuffer, 0, sizeof(MsgBuffer));
                memcpy(MsgBuffer, OutShadowPath, wcslen(OutShadowPath)*2);
                if(WriteFile(PipeHandle, MsgBuffer, sizeof(MsgBuffer), &Garbage, 0) &&  Garbage == sizeof(MsgBuffer)){
                    LOG("Succesfully send VSS path to server\n");
                    // success!
                }
                else{
                    LOG("Unable to send vss path to server!\n");
                }
                
            }
            if(CType == ProcessCommandType_TerminateVSS){
                
                LONG Deleted=0;
                VSS_ID NonDeleted;
                HRESULT hr;
                CComPtr<IVssAsync> async;
                hr = VSSPTR->BackupComplete(&async);
                if(hr == S_OK){
                    async->Wait();
                    LOG("VSS backup complete successfull!\n");
                }
                else{
                    LOG("VSS backup complete failed\n");
                }

                hr = VSSPTR->DeleteSnapshots(VSSID, VSS_OBJECT_SNAPSHOT, TRUE, &Deleted, &NonDeleted);
                VSSPTR.Release();
                WriteFile(PipeHandle, MsgBuffer, sizeof(MsgBuffer), &Garbage, 0);
                fclose(Log);
                return 0;
            }
            
        }
        
    }
    
    LOG("Terminating process!\n");
    return 0;
}

int main(int argc, char *argv[]){
    Log = fopen("C:\\ProgramData\\NarDiskBackup\\standalonevss.txt", "wb");
    fprintf(Log, "STARTED!\n");
    AlternateMain(argc, argv);
    fprintf(Log, "DONE!\n");
    fclose(Log);
}