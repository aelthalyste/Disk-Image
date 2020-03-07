/*++

Copyright (c) 1989-2002  Microsoft Corporation

Module Name:

		mspyUser.c

Abstract:

		This file contains the implementation for the main function of the
		user application piece of MiniSpy.  This function is responsible for
		controlling the command mode available to the user to control the
		kernel mode driver.

Environment:

		User mode

--*/

#include <DriverSpecs.h>
_Analysis_mode_(_Analysis_code_type_user_code_)

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <assert.h>
#include "mspyLog.h"
#include <strsafe.h>

#define SUCCESS              0
#define USAGE_ERROR          1
#define EXIT_INTERPRETER     2
#define EXIT_PROGRAM         4

#define INTERPRETER_EXIT_COMMAND1 "go"
#define INTERPRETER_EXIT_COMMAND2 "g"
#define PROGRAM_EXIT_COMMAND      "exit"
#define CMDLINE_SIZE              256
#define NUM_PARAMS                40

#define MINISPY_NAME            L"MiniSpy"

DWORD
InterpretCommand(
	_In_ int argc,
	_In_reads_(argc) char* argv[],
	_In_ PLOG_CONTEXT Context
);

VOID
ListDevices(
	VOID
);

VOID
DisplayError(
	_In_ DWORD Code
)

/*++

Routine Description:

	 This routine will display an error message based off of the Win32 error
	 code that is passed in. This allows the user to see an understandable
	 error message instead of just the code.

Arguments:

	 Code - The error code to be translated.

Return Value:

	 None.

--*/

{
	WCHAR buffer[MAX_PATH] = { 0 };
	DWORD count;
	HMODULE module = NULL;
	HRESULT status;

	count = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		Code,
		0,
		buffer,
		sizeof(buffer) / sizeof(WCHAR),
		NULL);


	if (count == 0) {

		count = GetSystemDirectory(buffer,
			sizeof(buffer) / sizeof(WCHAR));

		if (count == 0 || count > sizeof(buffer) / sizeof(WCHAR)) {

			//
			//  In practice we expect buffer to be large enough to hold the 
			//  system directory path. 
			//

			printf("    Could not translate error: %d\n", Code);
			return;
		}


		status = StringCchCat(buffer,
			sizeof(buffer) / sizeof(WCHAR),
			L"\\fltlib.dll");

		if (status != S_OK) {

			printf("    Could not translate error: %d\n", Code);
			return;
		}

		module = LoadLibraryExW(buffer, NULL, LOAD_LIBRARY_AS_DATAFILE);

		//
		//  Translate the Win32 error code into a useful message.
		//

		count = FormatMessage(FORMAT_MESSAGE_FROM_HMODULE,
			module,
			Code,
			0,
			buffer,
			sizeof(buffer) / sizeof(WCHAR),
			NULL);

		if (module != NULL) {

			FreeLibrary(module);
		}

		//
		//  If we still couldn't resolve the message, generate a string
		//

		if (count == 0) {

			printf("    Could not translate error: %d\n", Code);
			return;
		}
	}

	//
	//  Display the translated error.
	//

	printf("    %ws\n", buffer);
}

//
//  Main uses a loop which has an assignment in the while 
//  conditional statement. Suppress the compiler's warning.
//

#pragma warning(push)
#pragma warning(disable:4706) // assignment within conditional expression

int _cdecl
main(
	_In_ int argc,
	_In_reads_(argc) char* argv[]
)
/*++

Routine Description:

		Main routine for minispy

Arguments:

Return Value:

--*/
{
	HANDLE port = INVALID_HANDLE_VALUE;
	HRESULT hResult = S_OK;
	DWORD result;
	ULONG threadId;
	HANDLE thread = NULL;
	LOG_CONTEXT context;
	CHAR inputChar;

	//
	//Create logging file
	//
	context.OutputFile = CreateFileA("OutputFile",
		GENERIC_WRITE,
		0,
		0,
		CREATE_ALWAYS,
		0,
		0
	);
	if (context.OutputFile == INVALID_HANDLE_VALUE) {
		printf("Couldn't create logging file!\n");
		hResult = GetLastError();
		DisplayError(hResult);
		goto Main_Cleanup;
	}

	//
	//  Initialize handle in case of error
	//

	context.ShutDown = NULL;

	//
	//  Open the port that is used to talk to
	//  MiniSpy.
	//

	printf("Connecting to filter's port...\n");

	hResult = FilterConnectCommunicationPort(MINISPY_PORT_NAME,
		0,
		NULL,
		0,
		NULL,
		&port);

	if (IS_ERROR(hResult)) {

		printf("Could not connect to filter: 0x%08x\n", hResult);
		DisplayError(hResult);
		goto Main_Exit;
	}

	//
	// Initialize the fields of the LOG_CONTEXT
	//

	context.Port = port;
	context.ShutDown = CreateSemaphore(NULL,
		0,
		1,
		L"MiniSpy shut down");
	context.CleaningUp = FALSE;
	context.LogToFile = TRUE;
	context.LogToScreen = TRUE;        //don't start logging yet
	context.ShouldFilter = FALSE;
	context.OutputFile = NULL;

	if (context.ShutDown == NULL) {

		result = GetLastError();
		printf("Could not create semaphore: %d\n", result);
		DisplayError(result);
		goto Main_Exit;
	}


	//
	// Create the thread to read the log records that are gathered
	// by MiniSpy.sys.
	//
	printf("Creating logging thread...\n");
	thread = CreateThread(NULL,
		0,
		RetrieveLogRecords,
		(LPVOID)&context,
		0,
		&threadId);

	if (!thread) {

		result = GetLastError();
		printf("Could not create logging thread: %d\n", result);
		DisplayError(result);
		goto Main_Exit;
	}



	//
	//  set screen logging state
	//


#if 1
#pragma region NAR_TEST
	WCHAR instanceName[INSTANCE_NAME_MAX_CHARS + 1];

	hResult = FilterAttach(MINISPY_NAME,
		L"C:\\",
		NULL, // instance name
		sizeof(instanceName),
		instanceName);

	if (SUCCEEDED(hResult)) {

		printf("    Instance name: %S\n", instanceName);

	}
	else {
		if (hResult != ERROR_FLT_INSTANCE_NAME_COLLISION) {
			printf("\n    Could not attach to device: 0x%08x\n", hResult);
			DisplayError(hResult);
			goto Main_Cleanup;
		}
	}
#pragma endregion
#endif 

	while (inputChar = (CHAR)getchar()) {
		if (inputChar == 'q' || inputChar == 'Q') {
			goto Main_Cleanup;
		}
		if (inputChar == 'u') {
			context.ShouldFilter = TRUE;
		}
		if (inputChar == 'y') {
			context.ShouldFilter = FALSE;
		}
		Sleep(100);
	}

Main_Cleanup:

	//
	// Clean up the threads, then fall through to Main_Exit
	//

	printf("Cleaning up...\n");

	//
	// Set the Cleaning up flag to TRUE to notify other threads
	// that we are cleaning up
	//
	context.CleaningUp = TRUE;

	//
	// Wait for everyone to shut down
	//

	WaitForSingleObject(context.ShutDown, INFINITE);

	if (context.LogToFile) {
		CloseHandle(context.OutputFile);
	}

Main_Exit:

	//
	// Clean up the data that is always around and exit
	//

	if (context.ShutDown) {

		CloseHandle(context.ShutDown);
	}

	if (thread) {

		CloseHandle(thread);
	}

	if (INVALID_HANDLE_VALUE != port) {
		CloseHandle(port);
	}
	return 0;
}

