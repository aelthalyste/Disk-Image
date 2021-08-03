#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <intrin.h>
#include "memory.h"

#define Assert(cond) do { if (!(cond)) __debugbreak(); } while (0)
#define BUFFER_SIZE  (4096)
<<<<<<< HEAD
=======
#define ASSERT(exp) (exp)
>>>>>>> sideup

static void CreateNamedPipePair(
                                HANDLE* PipeHandle,
                                DWORD bufferSize,
                                SECURITY_ATTRIBUTES* sattr,
                                char *OutName = 0)
{
    static DWORD id = 0;
    
    char name[MAX_PATH];
    sprintf(name, "\\\\.\\Pipe\\WhateverUniqueName.%08x.%08x", GetCurrentProcessId(), id++);
    
    *PipeHandle = CreateNamedPipeA(
                                   name,
                                   PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                   PIPE_TYPE_BYTE | PIPE_WAIT,
                                   10,
                                   bufferSize,
                                   bufferSize,
                                   0,
                                   sattr);
    
    Assert(*PipeHandle != INVALID_HANDLE_VALUE);
    
#if 0    
    *write = CreateFileA(
                         name,
                         GENERIC_WRITE,
                         0,
                         sattr,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL);
#endif
    
    if(OutName != 0){
        strcpy(OutName, name);
    }
    
}

<<<<<<< HEAD
int main(int argc, char* argv[])
{
=======


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
        printf("Failed CoInitialize function %d\n", hResult);
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
        printf("Failed CoInitializeSecurity function %d\n", hResult);
        Return = FALSE;
    }
    return Return;
    
}

#if 0
struct process_listen_ctx{
    char *OutBuffer;
    char *ErrBuffer;
    uint32_t BufferSize;
    
    OVERLAPPED OutOverlapped;
    OVERLAPPED ErrOverlapped;
    PROCESS_INFORMATION PInfo;
    
    struct {
        HANDLE Handles[3]; // out, err, process
    };
    struct {
        HANDLE Out;
        HANDLE Err;
        HANDLE Process;
    };
    
    HANDLE InRPipe;
    HANDLE InWPipe;
    
    HANDLE OutRPipe;
    HANDLE OutWPipe;
    
    HANDLE ErrRPipe;
    HANDLE ErrWPipe;
};

inline process_listen_ctx
SetupVSSListen(char Letter){
    
    process_listen_ctx Result = {};
    Result.BufferSize   = 1024;
    Result.OutBuffer = (char*)malloc(Result.BufferSize);
    Result.ErrBuffer = (char*)malloc(Result.BufferSize);
    
>>>>>>> sideup
    BOOL ok;
    
    SECURITY_ATTRIBUTES sattr = {};
    sattr.nLength = sizeof(sattr);
    sattr.bInheritHandle = TRUE;
    
    CreateNamedPipePair(&Result.InRPipe, &Result.InWPipe, Result.BufferSize, &sattr);
    CreateNamedPipePair(&Result.OutRPipe, &Result.OutWPipe, Result.BufferSize, &sattr);
    CreateNamedPipePair(&Result.ErrRPipe, &Result.ErrWPipe, Result.BufferSize, &sattr);
    
    STARTUPINFOA sinfo = {};
    sinfo.cb = sizeof(sinfo);
    sinfo.dwFlags    = STARTF_USESTDHANDLES;
    sinfo.hStdInput  = Result.InRPipe;
    sinfo.hStdOutput = Result.OutWPipe;
    sinfo.hStdError  = Result.ErrWPipe;
    
    char cmdline[4];
    sprintf(cmdline, "%c", Letter);
    
    ok = CreateProcessA("standalone_vss.exe", cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, NULL, &sinfo, &Result.PInfo);
    ASSERT(ok);
    
    DWORD resume = ResumeThread(Result.PInfo.hThread);
    ASSERT(resume >= 0);
    
    CloseHandle(Result.PInfo.hThread);
    CloseHandle(Result.InRPipe);
    CloseHandle(Result.InWPipe);
    CloseHandle(Result.OutWPipe);
    CloseHandle(Result.ErrWPipe);
    
    Result.Out = CreateEvent(NULL, TRUE, TRUE, NULL);
    ASSERT(Result.Out);
    
    Result.Err = CreateEvent(NULL, TRUE, TRUE, NULL);
    ASSERT(Result.Err);
    
#if 1    
    Result.Handles[2] = Result.PInfo.hProcess;
#endif
    
    return Result;
}

inline int
NarGetVSSPath(process_listen_ctx *Ctx){
    return 0;
}

inline void
FreeProcessListen(process_listen_ctx *Ctx){
    free(Ctx->ReadBuffer);
    free(Ctx->eadBuffer);
    return;
}

inline char*
ProcessListenStdOut(){
    
}

inline void
FreeProcessListen(process_listen_ctx *Ctx){
    
    int count = 3;
    BOOL ok;
    while (count != 0)
    {
        DWORD wait = WaitForMultipleObjects(count, Ctx->Handles, FALSE, 5000);
        
        if(wait >= WAIT_OBJECT_0 && wait < WAIT_OBJECT_0 + count){
            // failed?
        }
        
        DWORD index = wait - WAIT_OBJECT_0;
        HANDLE h = Ctx->Handles[index];
        if (h == Ctx->Out)
        {
            if (Ctx->OutOverlapped.hEvent != NULL)
            {
                DWORD read;
                if (GetOverlappedResult(Ctx->Out, &Ctx->OutOverlapped, &read, TRUE))
                {
                    printf("STDOUT received: %.*s\n", (int)read, Ctx->OutBuffer);
                    memset(&Ctx->OutOverlapped, 0, sizeof(Ctx->OutOverlapped));
                }
                else
                {
                    ASSERT(FALSE);
                }
                
            }
            
            Ctx->OutOverlapped.hEvent = Ctx->Out;
            ReadFile(Ctx->OutRPipe, Ctx->OutBuffer, Ctx->BufferSize, 0, &Ctx->OutOverlapped);
        }
        else if (h == Ctx->Err)
        {
            if (Ctx->ErrOverlapped.hEvent != NULL)
            {
                DWORD read;
                if (GetOverlappedResult(Ctx->ErrRPipe, &Ctx->ErrOverlapped, &read, TRUE))
                {
                    printf("STDERR received: %.*s\n", (int)read, Ctx->ErrBuffer);
                    memset(&Ctx->ErrOverlapped, 0, sizeof(Ctx->ErrOverlapped));
                }
                else
                {
                    
#if 0                    
                    Assert(GetLastError() == ERROR_BROKEN_PIPE);
                    
                    handles[index] = handles[count - 1];
                    count--;
#endif
                    
                    CloseHandle(Ctx->ErrRPipe);
                    CloseHandle(Ctx->Err);
                    continue;
                }
            }
            
            Ctx->ErrOverlapped.hEvent = Ctx->Err;
            ReadFile(Ctx->ErrRPipe, Ctx->ErrBuffer, Ctx->BufferSize, NULL, &Ctx->ErrOverlapped);
        }
        else if (h == Ctx->Process)
        {
            
            DWORD exitCode;
            
            ok = GetExitCodeProcess(Ctx->PInfo.hProcess, &exitCode);
            ASSERT(ok);
            CloseHandle(Ctx->PInfo.hProcess);
            
            printf("exit code = %u\n", exitCode);
        }
    }
}
#endif


int main(int argc, char* argv[])
{
    SetupVSS();
    
    BOOL ok;
    
    SECURITY_ATTRIBUTES sattr = {};
    sattr.nLength = sizeof(sattr);
    sattr.bInheritHandle = TRUE;
    
    char PipeName[MAX_PATH];
    
    HANDLE PipeHandle;
    CreateNamedPipePair(&PipeHandle, BUFFER_SIZE, &sattr, PipeName);
    
    STARTUPINFOA sinfo = {};
    sinfo.cb = sizeof(sinfo);
    
    PROCESS_INFORMATION pinfo;
    ok = CreateProcessA(argv[1], PipeName, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, NULL, &sinfo, &pinfo);
    Assert(ok);
    
    char outTemp[BUFFER_SIZE];
    char errTemp[BUFFER_SIZE];
    
    
    char MyMessage[] = "Helld!!";
    
    DWORD resume = ResumeThread(pinfo.hThread);
    Assert(resume >= 0);
    
    BOOL R;
    
    OVERLAPPED ov = {};
    R = ConnectNamedPipe(PipeHandle, &ov);
    while(1){
        if(HasOverlappedIoCompleted(&ov))
            break;
        Sleep(100);
    }
    
    
    {
        OVERLAPPED ov = {};
        
        char bf[512];
        
        while(1){
            Sleep(50);
            DWORD TargetRead = sizeof(bf);
            DWORD Read;
            ReadFile(PipeHandle, bf, sizeof(bf), &Read, &ov);
            
            while(!GetOverlappedResult(PipeHandle, &ov, &Read, FALSE)){
                Sleep(5);
            }
            
            Assert(Read == TargetRead);
            fprintf(stdout, "%s\n", bf);
        }
    }
    
    return 0;
}



<<<<<<< HEAD

/*
        
        condition 1:
            --------------
        -------                             MUST ITERATE THAT ONE
    
        condition 2:
            -----------------               MUST ITERATE THAT ONE
                        ------------
    
        condition 3:
            ------------------
                --------                    MUST ITERATE THAT ONE
    
        condition 4:
            -----------------
            -----------------
                doesnt really matter which one you iterate, fits in algorithm below
    
        as you can see, if we would like to spot continuous colliding blocks, we must ALWAYS ITERATE THAT HAS LOWER END POINT, and their intersection WILL ALWAYS BE REPRESENTED AS
        HIGH_START - LOW_END of both of them.
*/
if(EEnd >= Base[BI].StartPos && Ex[EI].StartPos <= Base[BI].StartPos){
    Result.StartPos = EEnd;
	Result.Len      = BEnd -EEnd;
	EI++; // SAVE THIS!!!
}
else if(Ex[EI].StartPos <= BEnd && EEnd >= BEnd){
    Result.StartPos = Base[BI].StartPos;
	Result.Len      = BEnd - Ex[EI].StartPos;
	BI++;
}
else if(Ex[EI].StartPos > Base[BI].StartPos && EEnd <= BEnd){
    Result.StartPos = Base[BI].StartPos;
	Result.Len      = Ex[EI].StartPos - Base[BI].StartPos;
	EI++;
}
else if(EEnd == BEnd && Base[BI].StartPos == Ex[EI].StartPos){
	EI++;
	BI++;
}
else{
	ASSERT(FALSE);
}
=======
>>>>>>> sideup
