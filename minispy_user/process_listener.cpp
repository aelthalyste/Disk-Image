#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <intrin.h>

#define Assert(cond) do { if (!(cond)) __debugbreak(); } while (0)

static void CreateNamedPipePair(
                                HANDLE* read,
                                HANDLE* write,
                                DWORD bufferSize,
                                SECURITY_ATTRIBUTES* sattr)
{
    static DWORD id = 0;
    
    char name[MAX_PATH];
    sprintf(name, "\\\\.\\Pipe\\WhateverUniqueName.%08x.%08x", GetCurrentProcessId(), id++);
    
    *read = CreateNamedPipeA(
                             name,
                             PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                             PIPE_TYPE_BYTE | PIPE_WAIT,
                             1,
                             bufferSize,
                             bufferSize,
                             0,
                             sattr);
    Assert(*read != INVALID_HANDLE_VALUE);
    
    *write = CreateFileA(
                         name,
                         GENERIC_WRITE,
                         0,
                         sattr,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL);
    Assert(*write != INVALID_HANDLE_VALUE);
}

#define BUFFER_SIZE 4096

int main(int argc, char* argv[])
{
    BOOL ok;
    
    SECURITY_ATTRIBUTES sattr = {};
    sattr.nLength = sizeof(sattr);
    sattr.bInheritHandle = TRUE;
    
    HANDLE inRpipe, inWpipe;
    CreateNamedPipePair(&inRpipe, &inWpipe, BUFFER_SIZE, &sattr);
    
    HANDLE outRpipe, outWpipe;
    CreateNamedPipePair(&outRpipe, &outWpipe, BUFFER_SIZE, &sattr);
    
    HANDLE errRpipe, errWpipe;
    CreateNamedPipePair(&errRpipe, &errWpipe, BUFFER_SIZE, &sattr);
    
    STARTUPINFOA sinfo = {};
    sinfo.cb = sizeof(sinfo);
    sinfo.dwFlags = STARTF_USESTDHANDLES;
    sinfo.hStdInput = inRpipe;
    sinfo.hStdOutput = outWpipe;
    sinfo.hStdError = errWpipe;
    
    char cmdline[1024];
    sprintf(cmdline, "%s C:\\", argv[0]);
    
    PROCESS_INFORMATION pinfo;
    ok = CreateProcessA(argv[1], "C:\\", NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, NULL, &sinfo, &pinfo);
    Assert(ok);
    
    DWORD resume = ResumeThread(pinfo.hThread);
    Assert(resume >= 0);
    
    CloseHandle(pinfo.hThread);
    CloseHandle(inRpipe);
    CloseHandle(inWpipe);
    CloseHandle(outWpipe);
    CloseHandle(errWpipe);
    
    HANDLE outEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    Assert(outEvent);
    
    HANDLE errEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    Assert(errEvent);
    
    char outTemp[BUFFER_SIZE];
    char errTemp[BUFFER_SIZE];
    
    OVERLAPPED outO = {};
    OVERLAPPED errO = {};
    
    HANDLE handles[] = { outEvent, errEvent, pinfo.hProcess };
    DWORD count = _countof(handles);
    while (count != 0)
    {
        DWORD wait = WaitForMultipleObjects(count, handles, FALSE, INFINITE);
        Assert(wait >= WAIT_OBJECT_0 && wait < WAIT_OBJECT_0 + count);
        
        DWORD index = wait - WAIT_OBJECT_0;
        HANDLE h = handles[index];
        if (h == outEvent)
        {
            if (outO.hEvent != NULL)
            {
                DWORD read;
                if (GetOverlappedResult(outRpipe, &outO, &read, TRUE))
                {
                    printf("STDOUT received: %.*s\n", (int)read, outTemp);
                    memset(&outO, 0, sizeof(outO));
                }
                else
                {
                    Assert(GetLastError() == ERROR_BROKEN_PIPE);
                    
                    handles[index] = handles[count - 1];
                    count--;
                    
                    CloseHandle(outRpipe);
                    CloseHandle(outEvent);
                    continue;
                }
            }
            
            outO.hEvent = outEvent;
            ReadFile(outRpipe, outTemp, sizeof(outTemp), NULL, &outO);
        }
        else if (h == errEvent)
        {
            if (errO.hEvent != NULL)
            {
                DWORD read;
                if (GetOverlappedResult(errRpipe, &errO, &read, TRUE))
                {
                    printf("STDERR received: %.*s\n", (int)read, errTemp);
                    memset(&errO, 0, sizeof(errO));
                }
                else
                {
                    Assert(GetLastError() == ERROR_BROKEN_PIPE);
                    
                    handles[index] = handles[count - 1];
                    count--;
                    
                    CloseHandle(errRpipe);
                    CloseHandle(errEvent);
                    continue;
                }
            }
            
            errO.hEvent = errEvent;
            ReadFile(errRpipe, errTemp, sizeof(errTemp), NULL, &errO);
        }
        else if (h == pinfo.hProcess)
        {
            handles[index] = handles[count - 1];
            count--;
            
            DWORD exitCode;
            ok = GetExitCodeProcess(pinfo.hProcess, &exitCode);
            Assert(ok);
            CloseHandle(pinfo.hProcess);
            
            printf("exit code = %u\n", exitCode);
        }
    }
    return 0;
}
