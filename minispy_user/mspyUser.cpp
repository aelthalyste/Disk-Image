﻿/*++

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
#include <iostream> //TODO remove this

#include <DriverSpecs.h>
_Analysis_mode_(_Analysis_code_type_user_code_)

#include <stdlib.h>
#include <stdio.h>
#include <cstdio>
#include <windows.h>
#include <assert.h>
#include "mspyLog.h"
#include <strsafe.h>
#include <functional>
#include <winioctl.h>

#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <vsmgmt.h>

// VDS includes
#include <vds.h>

// ATL includes
#include <atlbase.h>
#include <string>
#include <vector>
#include <list>

#include <fstream>
#include <streambuf>
#include <sstream>


VOID
DisplayError(
						 _In_ DWORD Code
						 );

/*Inserts element to given chain*/
void
InsertToList(region_chain* Root, nar_record Rec) {

	region_chain* New = new region_chain;

	{

		if (Root->Next == NULL) {
			New->Back = Root;
			New->Next = NULL;
			New->Back->Next = New;
		}
		else {
			New->Back = Root;
			New->Next = Root->Next;

			New->Back->Next = New;
			New->Next->Back = New;
		}

		New->Rec = Rec;

	}

	/*Not checking Root->Back since it is impossible to replace head with Insert*/

}

void
AppendToList(region_chain* AnyPoint, nar_record Rec) {
	/*Find tail of the list*/
	while (AnyPoint->Next) AnyPoint = AnyPoint->Next;
	//Append new data
	AnyPoint->Next = new region_chain;
	AnyPoint->Next->Back = AnyPoint;
	AnyPoint->Next->Next = 0;
	AnyPoint->Next->Rec = Rec;
}

void
RemoveFromList(region_chain* R) {
	if (R->Next != NULL) {
		R->Next->Back = R->Back;
	}
	if (R->Back != NULL) {
		R->Back->Next = R->Next;
	}

	//CLEANMEMORY(R); //TODO maybe not clean memory here ?
}

void
PrintList(region_chain* Temp) {
	printf("######\n");
	while (Temp->Back) Temp = Temp->Back; //Go to start of the list
	while (Temp != NULL) {
		printf("%I64d\t%I64d\n", Temp->Rec.StartPos, Temp->Rec.Len);
		Temp = Temp->Next;
	}
	printf("######");
}

void
PrintListReverse(region_chain* Temp) {
	do {
		printf("%I64d\t%I64d\n", Temp->Rec.StartPos, Temp->Rec.Len);
		Temp = Temp->Back;
	} while (Temp != NULL);
}

/*
	Heap position of the list,
	since &Any is invalid adress for outside of this scope
*/
region_chain*
GetListHead(region_chain Any) {
	while (Any.Back) Any = *Any.Back;
	return Any.Next->Back; 
}

/*
Return:
LEFT if R1 endpoint < R2.start, R1 falls left side of the R2
RIGHT if R1 start > R2 endpoint, R1 falls right side of the R2
COLLISION if two regions collide, R1 and R2 collides

Do not use this function to check region collisions,
instead use IsRegionsCollide.
*/
rec_or
GetOrientation(nar_record *R1, nar_record *R2) {
	rec_or Return = (rec_or)0;

	ULONGLONG R1EP = R1->StartPos + R1->Len;
	ULONGLONG R2EP = R2->StartPos + R2->Len;
	
	if (R1EP < R2->StartPos) {
		Return = rec_or::LEFT;
	}
	else if (R1->StartPos > R2EP) {
		Return = rec_or::RIGHT;
	}
	else {
		Return = rec_or::COLLISION;
	}
	
	return Return;
}
/*
So thing about generate file name and getfilename functions,
generate file names ALWAYS used to create files, most of them used when
incremental backup requested by application, if operation succeeds, then they are
valid. If not, they will be deleted anyway.
Problem is, there MUST be always a log file to keep track of changes on the volume, if operation fails, we must not delete what we just created, or any other file that created after request.
volume_backup_inf contains LastCompletedDiffIndex and CurrentLogIndex to seperate two concepts. For now it is vague that we might not need such concepts.
Because no matter what happens on the volume, we must keep tracking it, and incremental index counter might not be good idea, if anything fails, whole chain might be broken. 

TLDR for myself
use generatefilename to create files with those names,
use getfilename to read files that are valid

restore operation must use getfilename, because we need valid file names,
incremental backup just need to create files, so use generatefilename functions
*/




/*
Recursively removing duplicate regions from incremental change logs.
First element of metadatas is ID to be restored, 0th is fullbackup regions
For entry point, MDShadow == Metadatas[Count]

Since we are iterating back, no need to use data_array, just check ID > 0
*/

void
RemoveDuplicates(region_chain** Metadatas,
	data_array<nar_record>* MDShadow, int ID) {

	if (ID == -1) return; //End of recursion

	UINT32 SIndex = 0;

	region_chain* MReg = Metadatas[ID];

	for (;;) {
		//TODO Condition
		if (MReg == NULL || SIndex >= MDShadow->Count) {
			break;
		}

		UINT32 Count = 0;
		
		rec_or Ori = GetOrientation(&MReg->Rec,&MDShadow->Data[SIndex]);

		if (Ori == LEFT) {
			MReg = MReg->Next;
		}
		else if (Ori == RIGHT) {
			SIndex++;
		}
		else if (Ori == COLLISION) {
			
			while (TRUE) {
				if (MReg == NULL || SIndex == MDShadow->Count) break;

				Count = IsRegionsCollide(&MReg->Rec, &MDShadow->Data[SIndex]);
				if (Count == 0) {
					MReg = MReg->Next;
					break;
				}

				/*Region's shadow, splits record into two*/
				if (Count == 2) {

					if (MDShadow->Data[SIndex].StartPos < MReg->Rec.StartPos) {
						RemoveFromList(MReg);
						MReg = MReg->Next;

						break;
						/*
						Shadow completelty overruns the region

						From this moment MReg->Next->Back != MReg, since we removed
						it from the region, but keep it's pointer to iterate one more time
						*/

					}
					else {
						/*
						Shadow splits region into two block
						*/
						ULONGLONG MinStart = MIN(MReg->Rec.StartPos, MDShadow->Data[SIndex].StartPos);

						nar_record LeftReg = { 0,0 };
						LeftReg.StartPos = MReg->Rec.StartPos;
						LeftReg.Len = MDShadow->Data[SIndex].StartPos - MReg->Rec.StartPos;
						InsertToList(MReg, LeftReg);

						nar_record RightReg = { 0,0 };

						//End of shadow region
						ULONGLONG SREP = MDShadow->Data[SIndex].StartPos + MDShadow->Data[SIndex].Len;

						//End of metadata region
						ULONGLONG MREP = MReg->Rec.StartPos + MReg->Rec.Len;

						RightReg.StartPos = SREP;
						RightReg.Len = MREP - SREP;
						InsertToList(MReg->Next, RightReg);
						PrintList(Metadatas[ID]);
						RemoveFromList(MReg);
						/*
						Since MReg is split into two, It can't be used anymore,
						remove it and iterate to next one, left region we just split,
						which is completed it's operation, but right one might be colliding
						next with MDShadow element, so iterate to next one and check them
						*/
						MReg = MReg->Next->Next; //Right region
						SIndex++;

					}//End of if shadow overruns region

				}//End of if Count == 2
				else {
					/*Regions collide default way*/

					//End point of metadata
					ULONGLONG MEP = MReg->Rec.StartPos + MReg->Rec.Len;
					//End point of shadow 
					ULONGLONG SEP = MDShadow->Data[SIndex].StartPos + MDShadow->Data[SIndex].Len;

					if (SEP > MEP) {
						/*
						Collision deleted right side of the metadata
						*/
						nar_record New = { 0,0 };
						New.StartPos = MReg->Rec.StartPos;
						New.Len = MDShadow->Data[SIndex].StartPos - MReg->Rec.StartPos;
						InsertToList(MReg, New);
						PrintList(Metadatas[ID]);
						RemoveFromList(MReg); //This does not clear MReg's pointers, so we can keep on iterating
						//This means MReg->Next->Back != MReg, rather equal to MReg->Back
						/*
						It is impossible to new region to collide with next one
						OLD          |-------*******| stars will be deleted
						SHADOW               |----------|  |--------|
																		^             ^
																		|             | this is next region queued up
																		this is region we are checking

						new status   |-------|
						shadow               |----------|  |--------|
						
						SIndex can be incremented safely,
						Since MReg->Next is region we just created, and we know it is non-colliding, we should
						iterate one more.
						*/
						MReg = MReg->Next->Next;
						SIndex++;
					}
					else {
						/*
						OLD                        |*****-----| stars will be deleted
						SHADOW               |----------|  |--------|
						Collision deleted left side of the metadata,
						since there MAY be another region colliding with metadata,
						we should break here, then check for another collision with new region
						*/
						nar_record New = { 0,0 };
						New.StartPos = MDShadow->Data[SIndex].StartPos + MDShadow->Data[SIndex].Len;
						New.Len = MEP - New.StartPos;
						/*Collision deleted left side of the metadata*/
						InsertToList(MReg, New);
						RemoveFromList(MReg);

						MReg = MReg->Next;
						PrintList(Metadatas[ID]);
						SIndex++;
					}

					
					break;
				}



			}//End of while

		}//End of IF COLLISION


	}

	/*
		Special case: If list's head is removed, then Metadata[ID] need to be updated
	*/
	Metadatas[ID] = GetListHead(*MReg);

	PrintList(Metadatas[ID]);

#if 0
	MReg = Metadatas[ID];
	while (MReg != NULL) {
		/*
		TODO update MDShadow
		*/
	}
#endif

}




BOOLEAN
CopyData(HANDLE S, HANDLE D, DWORD Len, DWORD BufSize) {
	BOOLEAN Return = TRUE;
	if (BufSize == 0) {
		//TODO
	}

	void* Buffer = malloc(BufSize);
	if (Buffer != NULL) {
		DWORD BytesRemaining = Len;
		DWORD BytesOperated = 0;
		if (BytesRemaining > BufSize) {

			while (BytesRemaining > BufSize) {
				if (ReadFile(S, Buffer, BufSize, &BytesOperated, 0)) {
					if (!WriteFile(D, Buffer, BufSize, &BytesOperated, 0) || BytesOperated != BufSize) {
						printf("Writefile failed\n");
						printf("Bytes written -> %d\n", BytesOperated);
						DisplayError(GetLastError());
						Return = FALSE;
						break;
					}
					BytesRemaining -= BufSize;
				}
				else {
					Return = FALSE;
					//if readfile failed
					break;
				}

			}
		}

		if (BytesRemaining > 0) {
			if (ReadFile(S, Buffer, BytesRemaining, &BytesOperated, 0) && BytesOperated == BytesRemaining) {
				if (!WriteFile(D, Buffer, BytesRemaining, &BytesOperated, 0) || BytesOperated != BytesRemaining) {
					printf("Writefile failed\n");
					printf("Bytes written -> %d\n", BytesOperated);
					DisplayError(GetLastError());
					Return = FALSE;
				}
			}
		}


	}//If Buffer != NULL
	else {
		printf("Can't allocate memory for buffer\n");
		Return = FALSE;
	}

	CLEANMEMORY(Buffer);
	return Return;
}

inline std::wstring
GenerateMFTFileName(wchar_t Letter, int ID) {
		std::wstring Return = DB_MFT_FILE_NAME;
		Return += Letter;
		Return += std::to_wstring(ID);
		return Return;
}

inline std::wstring
GenerateMFTMetadataFileName(wchar_t Letter, int ID) {
		std::wstring Return = DB_MFT_LCN_FILE_NAME;
		Return += Letter;
		Return += std::to_wstring(ID);
		return Return;
}

inline std::wstring
GenerateDBMetadataFileName(wchar_t Letter, int ID) {
		std::wstring Result = (DB_METADATA_FILE_NAME);
		Result += Letter;
		Result += std::to_wstring(ID);
		return Result;
}

inline std::wstring
GenerateDBFileName(wchar_t Letter, int ID) {
		std::wstring Return = DB_FILE_NAME;
		Return += Letter;
		Return += std::to_wstring(ID);
		return Return;
}

inline std::wstring
GenerateFBFileName(wchar_t Letter) {
		std::wstring Return = FB_FILE_NAME;
		Return += Letter;
		return Return;
}

inline std::wstring
GenerateFBMetadataFileName(wchar_t Letter) {
		std::wstring Return = FB_METADATA_FILE_NAME;
		Return += Letter;
		return Return;
}

inline BOOLEAN
InitNewLogFile(volume_backup_inf* V) {
		BOOLEAN Return = FALSE;
		CloseHandle(V->LogHandle);
		
		V->LogHandle = CreateFileW(
															 GenerateDBMetadataFileName(V->Letter, V->CurrentLogIndex).c_str(),
															 GENERIC_READ | GENERIC_WRITE, 0, 0,
															 CREATE_ALWAYS, 0, 0
															 );
		if (V->LogHandle != INVALID_HANDLE_VALUE) {
				V->IncRecordCount = 0;
				Return = TRUE;
		}
		
		return Return;
}

BOOLEAN
SaveMFT(volume_backup_inf* VolInf, HANDLE VSSHandle, data_array<nar_record> *MFTLCN) {
		BOOLEAN Return = FALSE;
		
		char LetterANSII = 0;
		wctomb(&LetterANSII, VolInf->Letter);
		
		DWORD BufferSize = VolInf->ClusterSize;
		void* Buffer = malloc(BufferSize);
		
		/*
		NOTE,
		IMPORTANT
		This function(SaveMFT), MUST be called last in the diff backup
		*/
		std::wstring MFTFileName = GenerateMFTFileName(VolInf->Letter,VolInf->CurrentLogIndex);
		HANDLE MFTSaveFile = CreateFileW(
																		 MFTFileName.c_str(),
																		 GENERIC_WRITE, 0, 0,
																		 CREATE_ALWAYS, 0, 0
																		 );
		
		std::wstring MFTMetadataFName = GenerateMFTMetadataFileName(VolInf->Letter,VolInf->CurrentLogIndex);
		HANDLE MFTMDFile = CreateFileW(
																	 MFTMetadataFName.c_str(),
																	 GENERIC_WRITE, 0, 0,
																	 CREATE_ALWAYS, 0, 0
																	 );
		
		
		if (MFTSaveFile != INVALID_HANDLE_VALUE
				&& MFTMDFile != INVALID_HANDLE_VALUE) {
				
				
				LARGE_INTEGER MoveTo = { 0 };
				LARGE_INTEGER NewFilePointer = { 0 };
				BOOL Result = 0;
				DWORD BytesOperated = 0;
				for (int i = 0; i < MFTLCN->Count; i++) {
						MoveTo.QuadPart = MFTLCN->Data[i].StartPos * VolInf->ClusterSize;
						
						Result = SetFilePointerEx(VSSHandle, MoveTo, &NewFilePointer, FILE_BEGIN);
						if (!SUCCEEDED(Result) || MoveTo.QuadPart != NewFilePointer.QuadPart) {
								printf("Set file pointer failed\n");
								printf("Tried to set pointer -> %I64d\n", MoveTo.QuadPart);
								printf("Instead, moved to -> %I64d\n", NewFilePointer.QuadPart);
								DisplayError(GetLastError());
								goto Exit;
						}
						
						if (!CopyData(VSSHandle, MFTSaveFile, MFTLCN->Data[i].Len * VolInf->ClusterSize)) {
								printf("Error occured while copying MFT file\n");
								goto Exit;
						}
						
#if 0
						for (int j = 0; j < MFTLCN->Data[i].Len; j++) {
								Result = ReadFile(VSSHandle, Buffer, BufferSize, &BytesOperated, 0);
								if (!SUCCEEDED(Result) || BytesOperated != BufferSize) {
										printf("Cant read from vss\n");
										printf("Bytes read -> %I64d\n", BytesOperated);
										DisplayError(GetLastError());
										goto Exit;
								}
								
								Result = WriteFile(MFTSaveFile, Buffer, BufferSize, &BytesOperated,0);
								if (!SUCCEEDED(Result) || BytesOperated != BufferSize) {
										printf("Cant write to mft file, %S \n", MFTFileName.c_str());
										printf("Bytes written %I64d\n", BytesOperated);
										DisplayError(GetLastError());
										goto Exit;
								}
						}
#endif 
						
				}
				
				Result = WriteFile(MFTMDFile, MFTLCN->Data, MFTLCN->Count * sizeof(nar_record), &BytesOperated, 0);
				if (!SUCCEEDED(Result) || MFTLCN->Count * sizeof(nar_record) != BytesOperated) {
						printf("Cant save mft metadata to file %S\n", MFTMetadataFName.c_str());
						printf("Bytes written -> %I64d\n", BytesOperated);
						DisplayError(GetLastError());
				}
				else {
						Return = TRUE;
				}
				
		}
		else {
				if (MFTSaveFile == INVALID_HANDLE_VALUE) {
						printf("Cant open file to save MFT, %S\n", MFTFileName.c_str());
						DisplayError(GetLastError());
				}
				if (MFTMDFile == INVALID_HANDLE_VALUE) {
						printf("Cant open file to save MFT metadata, %S\n", MFTMetadataFName.c_str());
						DisplayError(GetLastError());
				}
		}
		
		Exit:
		CLEANMEMORY(Buffer);
		CLEANHANDLE(MFTSaveFile);
		CLEANHANDLE(MFTMDFile);
		
		return Return;
		
}


BOOLEAN
RestoreMFT(restore_inf *R,HANDLE VolumeHandle) {
		BOOLEAN Return = FALSE;
		DWORD BufferSize = 4096;
		void* Buffer = malloc(BufferSize);
		
		HANDLE MFTMDHandle = INVALID_HANDLE_VALUE;
		std::wstring MFTFileName = GenerateMFTFileName(R->SrcLetter,R->DiffVersion);
		HANDLE MFTHandle = CreateFileW(
																	 MFTFileName.c_str(),
																	 GENERIC_READ, 0, 0,
																	 OPEN_EXISTING, 0, 0
																	 );
		printf("Restoring MFT with file %S\n", MFTFileName.c_str());
		
		if (MFTHandle != INVALID_HANDLE_VALUE) {
				std::wstring MFTMDFileName = GenerateMFTMetadataFileName(R->SrcLetter,R->DiffVersion);
				MFTMDHandle = CreateFileW(
																	MFTMDFileName.c_str(),
																	GENERIC_READ, 0, 0,
																	OPEN_EXISTING, 0, 0
																	);
				
				if (MFTMDHandle != INVALID_HANDLE_VALUE) {
						DWORD FileSize = GetFileSize(MFTMDHandle, 0);
						data_array<nar_record> Metadata = { 0,0 };
						Metadata.Data = (nar_record*)malloc(FileSize);
						DWORD BytesRead = 0;
						if (ReadFile(MFTMDHandle, Metadata.Data, FileSize, &BytesRead, 0) && FileSize == BytesRead) {
								
								//Read Metadata, continue to copy operation
								Metadata.Count = FileSize / sizeof(nar_record);
								
								LARGE_INTEGER MoveTo = { 0 };
								LARGE_INTEGER NewFilePointer = { 0 };
								
								for (int i = 0; i < Metadata.Count; i++) {
										HRESULT Result = 0;
										MoveTo.QuadPart = Metadata.Data[i].StartPos * 4096LL;
										
										Result = SetFilePointerEx(VolumeHandle, MoveTo, &NewFilePointer, FILE_BEGIN);
										if (!SUCCEEDED(Result) || MoveTo.QuadPart != NewFilePointer.QuadPart) {
												printf("Set file pointer failed\n");
												printf("Tried to set pointer -> %I64d\n", MoveTo.QuadPart);
												printf("Instead, moved to -> %I64d\n", NewFilePointer.QuadPart);
												DisplayError(GetLastError());
												goto Exit;
										}
										
										if (!CopyData(MFTHandle, VolumeHandle, Metadata.Data[i].Len * R->ClusterSize)) {
												goto Exit;
										}
										
#if 0
										for (int j = 0; j < Metadata.Data[i].Len; j++) {
												DWORD BytesOperated = 0;
												Result = ReadFile(MFTHandle, Buffer, BufferSize, &BytesOperated, 0);
												if (!SUCCEEDED(Result) || BytesOperated != BufferSize) {
														printf("Cant read from mft file\n");
														printf("Bytes read -> %d\n", BytesOperated);
														printf("Buffersize %d\n", BufferSize);
														DisplayError(GetLastError());
														goto Exit;
												}
												
												Result = WriteFile(VolumeHandle, Buffer, BufferSize, &BytesOperated,0);
												if (!SUCCEEDED(Result) || BytesOperated != BufferSize) {
														printf("Cant write to volume, %S \n", MFTFileName.c_str());
														printf("Bytes written %I64d\n", BytesOperated);
														DisplayError(GetLastError());
														goto Exit;
												}
										}
#endif 
										
								} //End of mft-copy operation
								Return = TRUE;
								
						}//if readfile end
						else {
								printf("Couldnt read mft metadata\n");
								DisplayError(GetLastError());
						}
						
				}
		}
		else {
				//TODO Error occured
		}
		
		Exit:
		CLEANHANDLE(MFTMDHandle);
		CLEANHANDLE(MFTHandle);
		CLEANMEMORY(Buffer);
		return Return;
}

inline BOOLEAN
InitRestoreTargetInf(restore_inf *Inf, wchar_t Letter) {
		
		Assert(Inf != NULL);
		BOOLEAN Return = FALSE;
		WCHAR Temp[] = L"!:\\";
		Temp[0] = Letter;
		DWORD BytesPerSector = 0;
		DWORD SectorsPerCluster = 0;
		
		if (!GetDiskFreeSpaceW(Temp, &SectorsPerCluster, &BytesPerSector, 0, 0)) {
				Inf->ClusterSize = 0;
		}
		else {
				Return = TRUE;
		}
		Inf->ClusterSize = BytesPerSector * SectorsPerCluster;
		
		
		return Return;
}

/*
Initialize structure from binary file. 
first sizeof(volume_backup_inf)  bytes contains stack variables(arrays and their old pointed values are stored too, since this information copied with memcpy operation), 
rest of it is heap variables, array's values, variable order is, which order they written in structure.
*/
inline BOOLEAN
InitVolumeInf(volume_backup_inf *VolInf, const wchar_t *Filepath) {
		BOOLEAN Return = FALSE;
		//TODO complete
		HANDLE Handle = CreateFileW(Filepath, GENERIC_READ, 0, 0, OPEN_ALWAYS, 0, 0);
		if (Handle != INVALID_HANDLE_VALUE) {
				DWORD FileSize = GetFileSize(Handle, 0);
				if (FileSize != 0) {
						DWORD BytesRead = 0;
						if (ReadFile(Handle, VolInf, sizeof(volume_backup_inf), &BytesRead, 0)) {
								Return = TRUE;
								//first part read, rest of it is array values
								
								
						}
						else {
								//TODO log
						}
				}
				else {
						//TODO Log
				}
		}
		else {
				//TODO log
		}
		return Return;
}


/*
Initialize structure from scratch, just with it's letter
*/
inline BOOLEAN
InitVolumeInf(volume_backup_inf* VolInf, wchar_t Letter) {
		//TODO, initialize VolInf->PartitionName
		
		BOOLEAN Return = FALSE;
		Assert(VolInf != NULL);
		VolInf->Letter = Letter;
		VolInf->IsActive = FALSE;
		VolInf->FullBackupExists = FALSE;
		VolInf->SaveToFile = TRUE;
		VolInf->CurrentLogIndex = 0;
		VolInf->ClusterCount = 0;
		VolInf->ClusterSize = 0;
		VolInf->RecordsMem.reserve(2 << 8);
		
		wchar_t Temp[] = L"!:\\";
		Temp[0] = VolInf->Letter;
		
		DWORD BytesPerSector = 0;
		DWORD SectorsPerCluster = 0;
		DWORD ClusterCount = 0;
		
		
		if (GetDiskFreeSpaceW(Temp, &SectorsPerCluster, &BytesPerSector, 0, &ClusterCount)) {
				VolInf->ClusterCount = ClusterCount;
				VolInf->ClusterSize = SectorsPerCluster * BytesPerSector;
				Return = TRUE;
		}
		else {
				//Failed
				//TODO LOG
		}
		
		VolInf->ExtraPartitions = { 0,0 };
		VolInf->ContextIndex = 0;
		VolInf->ContextIndex = -1; //Access violation if tried to use volume without adding it to tracklist
		VolInf->LogHandle = INVALID_HANDLE_VALUE;
		VolInf->IncRecordCount = 0;
		
		//TODO link letter name and full name 
		//TODO HANDLE EXTRA PARTITIONS, SUCH AS BOOT AND RECOVERY
		return Return;
}



BOOLEAN
IsSameVolumes(const WCHAR* OpName, const WCHAR VolumeLetter) {
		return TRUE;//TODO
}

BOOL 
CompareNarRecords(const void* v1, const void* v2) {
		
		nar_record* n1 = (nar_record*)v1;
		nar_record* n2 = (nar_record*)v2;
		if (n1->StartPos == n2->StartPos && n2->Len < n1->Len) {
				return 1;
		}
		
		if (n1->StartPos > n2->StartPos) {
				return 1;
		}
		return -1;
		
}

std::wstring
GetShadowPath(std::wstring Drive, CComPtr<IVssBackupComponents>& ptr) {
		VSS_ID sid;
		HRESULT res;
		
		res = CreateVssBackupComponents(&ptr);
		
		res = ptr->InitializeForBackup();
		ASSERT_VSS(res == S_OK);
		res = ptr->SetContext(VSS_CTX_BACKUP);
		ASSERT_VSS(res == S_OK);
		res = ptr->StartSnapshotSet(&sid);
		ASSERT_VSS(res == S_OK);
		res = ptr->SetBackupState(false, false, VSS_BACKUP_TYPE::VSS_BT_FULL, false);
		ASSERT_VSS(res == S_OK);
		res = ptr->AddToSnapshotSet((LPWSTR)Drive.c_str(), GUID_NULL, &sid); // C:\\ ex
		ASSERT_VSS(res == S_OK);
		
		{
				CComPtr<IVssAsync> Async;
				res = ptr->PrepareForBackup(&Async);
				ASSERT_VSS(res == S_OK);
				Async->Wait();
		}
		
		{
				CComPtr<IVssAsync> Async;
				res = ptr->DoSnapshotSet(&Async);
				ASSERT_VSS(res == S_OK);
				Async->Wait();
		}
		
		VSS_SNAPSHOT_PROP SnapshotProp;
		ptr->GetSnapshotProperties(sid, &SnapshotProp);
		std::wstring ShadowPath = std::wstring(SnapshotProp.m_pwszSnapshotDeviceObject);
		
		return ShadowPath;
}

BOOL
IsRegionsCollide(nar_record* R1, nar_record* R2) {
		BOOL Result = FALSE;
		ULONGLONG R1EndPoint = R1->StartPos + R1->Len;
		ULONGLONG R2EndPoint = R2->StartPos + R2->Len;
		
		if(R1EndPoint < R2EndPoint && R1->StartPos > R2->StartPos){
				return 2; /*Collision splits region into two block*/
		}
		if(R2EndPoint < R1EndPoint && R2->StartPos > R1->StartPos){
				return 2; /*Collision splits region into two block*/
		}
		
		if ((R1EndPoint < R2EndPoint
				 && R1EndPoint >= R2->StartPos)
				|| (R2EndPoint < R1EndPoint
						&& R2EndPoint >= R1->StartPos)
				) {
				Result = TRUE;
		}
		
		return Result;
}

inline VOID
NarCloseThreadCom(
									PLOG_CONTEXT Context
									) {
		//TODO termination of the whole program, probably..
		Assert(Context->Thread != NULL);
		
		Context->CleaningUp = TRUE;
		WaitForSingleObject(Context->ShutDown, INFINITE);
		
		CloseHandle(Context->ShutDown);
		CloseHandle(Context->Thread);
		
		Context->ShutDown = NULL;
}

inline BOOL
NarCreateThreadCom(
									 PLOG_CONTEXT Context
									 ) {
		Assert(Context->Thread == NULL);
		Assert(Context->ShutDown == NULL);
		
		BOOL Result = FALSE;
		
		Context->CleaningUp = FALSE;
		
		Context->ShutDown = CreateSemaphoreW(NULL,
																				 0,
																				 1,
																				 L"MiniSpy shut down"
																				 );
		if (Context->ShutDown != NULL) {
				Context->Thread = CreateThread(NULL,
																			 0,
																			 RetrieveLogRecords,
																			 (LPVOID)Context,
																			 0,
																			 0);
				if (Context->Thread != NULL) {
						Result = TRUE;
				}
				else {
						printf("Can't create thread to communicate with driver\n");
						DisplayError(GetLastError());
				}
				
		}
		else {
				//Unable to create semaphore
				printf("Could not create semaphore for thread..\n");
				DisplayError(GetLastError());
		}
		
		
		return Result;
}


std::string 
NarExecuteCommand(const char* cmd, std::string FileName) {
		//TODO return char* instead of std::string
		
		
		std::string result = "";
		
		system(cmd);
		
		std::ifstream t(FileName);
		std::stringstream buffer;
		buffer << t.rdbuf();
		result = buffer.str();
		
		return result;
}

/*
Pass by value, might be slow if input str is too big ?? 
*/
inline std::vector<std::string>
Split(std::string str, std::string delimiter) {
		UINT SizeAssumed = 100;
		
		std::vector<std::string> Result;	
		Result.reserve(SizeAssumed);
		
		size_t pos = 0;
		std::string token;
		while ((pos = str.find(delimiter)) != std::string::npos) {
				token = str.substr(0, pos);
				str.erase(0, pos + delimiter.length());
				Result.emplace_back(std::move(token));
		}
		if (str.length() > 0) Result.emplace_back(std::move(str));
		Result.shrink_to_fit();
		
		return Result;
}

inline std::vector<std::wstring>
Split(std::wstring str, std::wstring delimiter) {
		UINT SizeAssumed = 100;
		
		std::vector<std::wstring> Result;
		Result.reserve(SizeAssumed);
		
		size_t pos = 0;
		std::wstring token;
		while ((pos = str.find(delimiter)) != std::string::npos) {
				token = str.substr(0, pos);
				str.erase(0, pos + delimiter.length());
				Result.emplace_back(std::move(token));
		}
		if (str.length() > 0) Result.emplace_back(std::move(str));
		Result.shrink_to_fit();
		
		return Result;
}


data_array<nar_record>
GetMFTLCN(char VolumeLetter) {
		//TODO check if input falls in a-z || A-Z
		data_array<nar_record> Result = { 0,0 };
		UINT SizeAssumed = 256;
		Result.Data = (nar_record*)malloc(sizeof(nar_record) * 256);
		Result.Count = 0;
		
		static char Command[] = "fsutil file queryextents C:\\$mft csv >TempTestFile";
		Command[25] = VolumeLetter;
		
		std::string Answer = NarExecuteCommand(Command, "TempTestFile");
		
		if (Answer[0] == '\n') Answer[0] = '#';
		Answer = Answer.substr(Answer.find("\n") + 1, Answer.size());
		
		size_t NewLinePos = 0;
		
		auto Rows = Split(Answer, "\n");
		
		for (const auto& Iter : Rows) {
				auto Vals = Split(Iter, ",");
				
				Result.Data[Result.Count].StartPos = std::stoull(Vals[2], 0, 16);
				Result.Data[Result.Count].Len = std::stoull(Vals[1], 0, 16);
				Result.Count++;
				if (Result.Count == SizeAssumed) {
						SizeAssumed *= 2;
						realloc(Result.Data, sizeof(Result.Data[0]) * SizeAssumed);
				}
		}
		
		realloc(Result.Data, sizeof(Result.Data[0]) * Result.Count);
		
		return Result;
}





/*
This operation just adds volume to list, does not starts to filter it,
until it's fullbackup is requested. After fullbackup, call AttachVolume to start filtering
*/
BOOLEAN
AddVolumeToTrack(PLOG_CONTEXT Context, wchar_t Letter) {
		BOOLEAN ErrorOccured = TRUE;
		HRESULT Result = 0;
		
		
		if (Context->Volumes.Data == NULL) {
				printf("Context's volume data was null, allocating memory\n");
				Context->Volumes.Data = (volume_backup_inf*)malloc(sizeof(volume_backup_inf));
				Context->Volumes.Count = 1;
		}
		else {
				for (UINT i = 0; i < Context->Volumes.Count; i++) {
						if (Context->Volumes.Data[i].Letter == Letter) {
								return FALSE;
						}
				}
				Context->Volumes.Data = (volume_backup_inf*)realloc(Context->Volumes.Data, (Context->Volumes.Count + 1) * sizeof(volume_backup_inf));
				Context->Volumes.Count += 1;
				
				if (Context->Volumes.Data == NULL) {
						printf("Can't reallocate memory\n"); //TODO fatal error
						DisplayError(GetLastError());
						return FALSE;
				}
				
		}
		printf("Memory job done\n");
		
		volume_backup_inf *VolInf = &Context->Volumes.Data[Context->Volumes.Count - 1];
		
		if (InitVolumeInf(VolInf, Letter)) {
				VolInf->ContextIndex = Context->Volumes.Count - 1;
				ErrorOccured = FALSE;
		}
		
		Cleanup:
		if (ErrorOccured) {
				CLEANHANDLE(VolInf->LogHandle);
				Context->Volumes.Data = (volume_backup_inf*)realloc(Context->Volumes.Data, (Context->Volumes.Count - 1) * sizeof(volume_backup_inf));
				Context->Volumes.Count -= 1;
		}
		
		return !ErrorOccured;
}

BOOLEAN 
DetachVolume(PLOG_CONTEXT Context, UINT VolInfIndex) {
		BOOLEAN Return = FALSE;
		HRESULT Result = 0;
		std::wstring Temp = L"!:\\";
		Temp[0] = Context->Volumes.Data[VolInfIndex].Letter;
		
		volume_backup_inf *VolInf = &Context->Volumes.Data[VolInfIndex];
		
		Result = FilterDetach(MINISPY_NAME, Temp.c_str(), 0);
		if (SUCCEEDED(Result)) {
				VolInf->IsActive = FALSE;
				Return = TRUE;
		}
		else {
				printf("Can't detach filter\n");
				DisplayError(GetLastError());
		}
		return Return;
}


/*
Attachs filter and increments currentlogindex
*/
BOOLEAN
AttachVolume(PLOG_CONTEXT Context, UINT VolInfIndex) {
		BOOLEAN Return = FALSE;
		HRESULT Result = 0;
		
		WCHAR Temp[] = L"!:\\";
		Temp[0] = Context->Volumes.Data[VolInfIndex].Letter;
		volume_backup_inf* VolInf = &Context->Volumes.Data[VolInfIndex];
		
		Result = FilterAttach(MINISPY_NAME, Temp, 0, 0, 0);
		if (SUCCEEDED(Result)) {
				VolInf->IsActive = TRUE;
				Return = TRUE;
		}
		else {
				printf("Can't attach filter\n");
				DisplayError(GetLastError());
		}
		
		return Return;
}



BOOLEAN
DiffBackupVolume(PLOG_CONTEXT Context, UINT VolInfIndex) {
		/*
		TODO eğer diffbackup başarılı olmazsa, oluşturulan dosyaları sil ve diffbackup sayısını bir düşür?
		*/
		BOOLEAN ErrorOccured = FALSE;
		
		data_array<nar_record> DiffRecords = { 0,0 };
		void* Buffer = 0;
		DWORD FileSize = 0;
		HANDLE DiffDataFile = 0;
		HANDLE ShadowHandle = 0;
		HANDLE MftOutput = 0;
		HANDLE MftHandle = 0;
		HANDLE MetadataFileHandle = 0;
		DWORD BytesOperated = 0;
		DWORD BufferSize = 1024 * 1024 * 128; // 128 MB;
		HRESULT Result = 0;
		std::wstring ShadowPath = L"";
		CComPtr<IVssBackupComponents> ptr;
		std::wstring dbfname = L"";
		volume_backup_inf* VolInf = 0;
		std::wstring temp = L"";
		data_array<nar_record> MFTLCN;
		
		VolInf = &Context->Volumes.Data[VolInfIndex];
		MFTLCN = GetMFTLCN(VolInf->Letter);
		
		printf("Detaching the  volume\n");
		if (!DetachVolume(Context, VolInfIndex)) {
				printf("Detach volume failed\n");
				goto D_Cleanup;
		}
		printf("Volume detached !\n");
		
		FileSize = VolInf->IncRecordCount * sizeof(nar_record);
		VolInf->SaveToFile = FALSE;
		VolInf->IsActive = TRUE;
		
		{
				WCHAR Temp[] = L"!:\\";
				Temp[0] = Context->Volumes.Data[VolInfIndex].Letter;
				ShadowPath = GetShadowPath(Temp, ptr);
		}
		
		if (!AttachVolume(Context, VolInfIndex)) {
				printf("Cant attach volume\n");
				goto D_Cleanup;
		}
		printf("Filtering started again!\n");
		
		DiffRecords.Data = (nar_record*)malloc(FileSize);
		DiffRecords.Count = FileSize / sizeof(nar_record);
		
		if (SetFilePointer(VolInf->LogHandle, 0, 0, FILE_BEGIN) != 0) {
				printf("Failed to set file pointer to beginning of the file\n");
				DisplayError(GetLastError());
		}
		
		Result = ReadFile(VolInf->LogHandle, DiffRecords.Data, FileSize, &BytesOperated, 0);
		if (!SUCCEEDED(Result) || FileSize != BytesOperated) {
				printf("Cant read difmetadata \n");
				printf("Filesize-> %d, BytesOperated -> %d", FileSize, BytesOperated);
				printf("N of records => %d", FileSize / sizeof(nar_record));
				DisplayError(GetLastError());
				goto D_Cleanup;
		}
		
		
		dbfname = GenerateDBFileName(VolInf->Letter, VolInf->CurrentLogIndex);
		DiffDataFile = CreateFileW(
															 dbfname.c_str(),
															 GENERIC_WRITE,
															 0, 0,
															 CREATE_ALWAYS, 0, 0
															 );
		if (DiffDataFile == INVALID_HANDLE_VALUE) {
				printf("Can not create file to store diff disk data, %S\n", dbfname.c_str());
				DisplayError(GetLastError());
				goto D_Cleanup;
		}
		
		VolInf->SaveToFile = FALSE;
		VolInf->FlushToFile = FALSE;
		
		
		/*
		NOT IMPLEMENTED
			So problem is, which I can not describe properly but at least can show it
			Let Rn be the record with at the index of N, so R0 becomes first record, R1 second and so on
	 
			Let assume there are three records, at least one is overlapping with other records regions
	 
			R1 =>                      |-----------------|
			R2 =>         |--------------|
			R3 =>                                |------------------|
			We can shrink that information such that, only storing new record like start => R2.start and len = R3.Len + R3.Start
	 
			If such collisions ever occurs, we can shrink them. Algorithm below does that.
			But in order to process that, list MUST be sorted so we can compare consequent list elements
		NOT IMPLEMENTED
		*/
#if 1
		/*
	possible implementation of algorithm above
	*/
		
		qsort(DiffRecords.Data, DiffRecords.Count, sizeof(nar_record), CompareNarRecords);
		
		
		UINT32 MergedRecordsIndex = 0;
		UINT32 CurrentIter = 0;
		
		for (;;) {
				if (CurrentIter == DiffRecords.Count - 1) {
						//Special case for the last element in the array
						if (!IsRegionsCollide(&DiffRecords.Data[CurrentIter], &DiffRecords.Data[MergedRecordsIndex - 1])) {
								DiffRecords.Data[MergedRecordsIndex] = DiffRecords.Data[CurrentIter];
								MergedRecordsIndex++;
						}
						break;
				}
				if (CurrentIter >= DiffRecords.Count) break;
				
				DiffRecords.Data[MergedRecordsIndex].StartPos = DiffRecords.Data[CurrentIter].StartPos;
				DiffRecords.Data[MergedRecordsIndex].Len = DiffRecords.Data[CurrentIter].Len;
				
				ULONGLONG EndPointTemp = DiffRecords.Data[CurrentIter].StartPos + DiffRecords.Data[CurrentIter].Len;
				
				while (DiffRecords.Data[MergedRecordsIndex].StartPos == DiffRecords.Data[CurrentIter].StartPos
							 || IsRegionsCollide(&DiffRecords.Data[MergedRecordsIndex], &DiffRecords.Data[CurrentIter])) {
						
						ULONGLONG EP1 = DiffRecords.Data[CurrentIter].StartPos + DiffRecords.Data[CurrentIter].Len;
						ULONGLONG EP2 = DiffRecords.Data[MergedRecordsIndex].StartPos + DiffRecords.Data[MergedRecordsIndex].Len;
						
						EndPointTemp = MAX(EP1, EP2);
						DiffRecords.Data[MergedRecordsIndex].Len = EndPointTemp - DiffRecords.Data[MergedRecordsIndex].StartPos;
						
						CurrentIter++;
						
				}
				
				DiffRecords.Data[MergedRecordsIndex].Len = EndPointTemp - DiffRecords.Data[MergedRecordsIndex].StartPos;
				MergedRecordsIndex++;
				if (RecordEqual(&DiffRecords.Data[MergedRecordsIndex], &DiffRecords.Data[CurrentIter])) {
						CurrentIter++;
				}
				
		}
		
		DiffRecords.Count = MergedRecordsIndex;
		realloc(DiffRecords.Data, DiffRecords.Count * sizeof(DiffRecords.Data[0]));
		
#endif
		
		
		ShadowHandle = CreateFileW(
															 ShadowPath.c_str(),
															 GENERIC_READ,
															 0,
															 NULL,
															 OPEN_EXISTING,
															 NULL,
															 NULL
															 );
		if (ShadowHandle == INVALID_HANDLE_VALUE) {
				printf("Can not open shadow path ..\n");
				DisplayError(GetLastError());
		}
		
		
		
		Buffer = malloc(BufferSize);
		if (Buffer == NULL) {
				goto D_Cleanup;
		}
		
		DWORD ClusterSize = 512 * 8;
		LARGE_INTEGER NewFilePointer;
		NewFilePointer.QuadPart = 0;
		
		printf("Saving differential state of the disk, please do not close the program\n");
		for (UINT i = 0; i < DiffRecords.Count; i++) {
				LARGE_INTEGER MoveTo;
				DWORD OperationSize = DiffRecords.Data[i].Len * ClusterSize;
				MoveTo.QuadPart = ClusterSize * DiffRecords.Data[i].StartPos;
				Result = SetFilePointerEx(ShadowHandle, MoveTo, &NewFilePointer, FILE_BEGIN);
				if (!SUCCEEDED(Result) || NewFilePointer.QuadPart != MoveTo.QuadPart) {
						printf("Failed to set file pointer..\n");
						DisplayError(GetLastError());
						goto D_Cleanup;
				}
				
				CopyData(ShadowHandle, DiffDataFile, DiffRecords.Data[i].Len * VolInf->ClusterSize);
				/*
				if (DiffRecords.Data[i].Len > BufferSize / ClusterSize) {
					printf("Operation length is bigger than buffer size, program can not execute this for demo (OperationLen in # clusters => %I64d)..\n", DiffRecords.Data[i].Len);
					goto D_Cleanup;
				}
		 
		 
				Result = ReadFile(ShadowHandle, Buffer, OperationSize, &BytesOperated, 0);
				if (!SUCCEEDED(Result) || BytesOperated != OperationSize) {
					printf("Could not read from shadow path.\n");
					printf("BytesReturned => %I64d\n", BytesOperated);
					printf("IterLen*ClusterSize => %I64d\n", OperationSize);
					DisplayError(GetLastError());
					goto D_Cleanup;
				}
		 
				Result = WriteFile(DiffDataFile, Buffer, OperationSize, &BytesOperated, 0);
				if (!SUCCEEDED(Result) || BytesOperated != OperationSize) {
					printf("Error occured while writing data to file\n");
					printf("BytesReturned => %I64d\n", BytesOperated);
					printf("IterLen*ClusterSize => %I64d\n", OperationSize);
					DisplayError(GetLastError());
					goto D_Cleanup;
				}
				*/
		}
		
		printf("MFT will be saved to seperate file\n");
		if (!SaveMFT(VolInf, ShadowHandle, &MFTLCN)) {
				printf("Cant save MFT\n");
				goto D_Cleanup;
		}
		
		
		printf("Completed diff \n");
		ErrorOccured = FALSE;
		D_Cleanup:
		//If operation succeeded, save all logs that are in RAM to new file, if op failed
		//do not close old handle.
		if (!ErrorOccured) {
				/*Since merge algorithm may have change size of the record buffer, 
				we should overwrite and truncate it*/
				SetFilePointer(VolInf->LogHandle, 0, 0, FILE_BEGIN);
				WriteFile(VolInf->LogHandle, DiffRecords.Data, DiffRecords.Count * sizeof(nar_record), 0, 0);
				SetEndOfFile(VolInf->LogHandle);
				
				VolInf->CurrentLogIndex++;
				
				if (!InitNewLogFile(VolInf)) {
						printf("Couldn't create metadata file, this is fatal error\n");
						printf("Please restart application\n");
						DisplayError(GetLastError());
				}
				
		}
		else {
				DeleteFileW(GenerateDBFileName(VolInf->Letter,VolInf->CurrentLogIndex).c_str());
		}
		
		VolInf->SaveToFile = TRUE;
		VolInf->FlushToFile = TRUE;
		
		CLEANMEMORY(Buffer);
		CLEANMEMORY(DiffRecords.Data);
		
		CLEANHANDLE(DiffDataFile);
		CLEANHANDLE(ShadowHandle);
		CLEANHANDLE(MftOutput);
		CLEANHANDLE(MftHandle);
		
		return !ErrorOccured;
}

BOOLEAN
FullBackupVolume(PLOG_CONTEXT Context, UINT VolInfIndex) {
		BOOLEAN Return = FALSE;
		HRESULT Result = 0;
		VOLUME_BITMAP_BUFFER* Bitmap = 0;
		UINT* ClusterIndices = 0;
		void* DataBuffer = 0;
		
		HANDLE DiskHandle = 0;
		HANDLE OutputHandle = 0;
		HANDLE MetadataFileHandle = 0;
		std::wstring ShadowPath;
		// TODO check if VSS requests could be filtered with driver with some kind of flag
		printf("Waiting VSS!\n");
		CComPtr<IVssBackupComponents> ptr;
		std::wstring fbmetadatafname;
		std::wstring FBFname = L"";
		ShadowPath = L"!:\\";
		ShadowPath[0] = Context->Volumes.Data[VolInfIndex].Letter;
		
		ShadowPath = GetShadowPath(ShadowPath.c_str(), ptr);
		
		if (!InitNewLogFile(&Context->Volumes.Data[VolInfIndex])) {
				printf("Cant create log file for inc operation\n");
				goto F_Cleanup;
		}
		
		
		if (!AttachVolume(Context, VolInfIndex)) {
				printf("Attach volume failed\n");
				goto F_Cleanup;
		}
		
		printf("Minifilter started!\n");
		
		printf("%S\n", ShadowPath.c_str());
		
		
		DiskHandle = CreateFileW(
														 ShadowPath.c_str(),
														 GENERIC_READ,
														 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
														 NULL,
														 OPEN_EXISTING,
														 0,
														 NULL
														 );
		if (DiskHandle == INVALID_HANDLE_VALUE) {
				printf("Couldn't open shadow disk. Terminating now!\n");
				goto F_Cleanup;
		}
		
		STARTING_LCN_INPUT_BUFFER StartingLCN;
		StartingLCN.StartingLcn.QuadPart = 0;
		DWORD BytesReturned;
		ULONGLONG MaxClusterCount = 0;
		DWORD BufferSize = 1024 * 1024 * 64; // 64megabytes
		
		
		Bitmap = (VOLUME_BITMAP_BUFFER*)malloc(BufferSize);
		if (Bitmap == NULL) {
				printf("Couldn't allocate bitmap buffer! This fatal error. Terminating..\n");
				goto F_Cleanup;
		}
		
		Result = DeviceIoControl(
														 DiskHandle,
														 FSCTL_GET_VOLUME_BITMAP,
														 &StartingLCN,
														 sizeof(StartingLCN),
														 Bitmap,
														 BufferSize,
														 &BytesReturned,
														 0
														 );
		if (!SUCCEEDED(Result)) {
				printf("Get volume bitmap failed..\n");
				Result = GetLastError();
				DisplayError(Result);
				goto F_Cleanup;
		}
		
		MaxClusterCount = Bitmap->BitmapSize.QuadPart;
		DWORD ClustersRead = 0;
		UCHAR* BitmapIndex = Bitmap->Buffer;
		ULONGLONG UsedClusterCount = 0;
		UCHAR BitmapMask = 1;
		DWORD ToMove = BufferSize;
		DWORD ClusterSize = 8 * 512;
		
		ClusterIndices = (UINT*)malloc(MaxClusterCount * sizeof(UINT));
		UINT CurrentIndex = 0;
		while (ClustersRead < MaxClusterCount) {
				if ((*BitmapIndex & BitmapMask) == BitmapMask) {
						ClusterIndices[CurrentIndex++] = (ClustersRead);
						UsedClusterCount++;
				}
				BitmapMask <<= 1;
				if (BitmapMask == 0) {
						BitmapMask = 1;
						BitmapIndex++;
				}
				ClustersRead++;
		}
		
		/*
	//Early deallocation to save some memory.
	This is not a problem for cleanup procedure since we are checking if memory is null before deallocation anyway.
	*/
		FBFname = GenerateFBFileName(Context->Volumes.Data[VolInfIndex].Letter);
		OutputHandle = CreateFileW(
															 FBFname.c_str(),
															 GENERIC_WRITE,
															 0,
															 NULL,
															 CREATE_ALWAYS,
															 0, 0
															 );
		if (OutputHandle == INVALID_HANDLE_VALUE) {
				printf("Could not create file %S\n",FBFname.c_str());
				Result = GetLastError();
				DisplayError(Result);
				goto F_Cleanup;
		}
		
		
		UINT64 FileBufferSize = ClusterSize;
		DataBuffer = malloc(FileBufferSize);
		DWORD BytesTransferred = 0;
		LARGE_INTEGER NewFilePointer;
		
		NewFilePointer.QuadPart = 0;
		
		printf("Saving disk state please do not close the program...\n");
		for (UINT i = 0; i < CurrentIndex; i++) {
				LARGE_INTEGER MoveTo;
				MoveTo.QuadPart = ClusterSize * ClusterIndices[i];
				
				Result = SetFilePointerEx(DiskHandle, MoveTo, &NewFilePointer, FILE_BEGIN);
				if (!SUCCEEDED(Result) || (NewFilePointer.QuadPart != MoveTo.QuadPart)) {
						printf("Setfilepointerex failed..\n");
						DisplayError(GetLastError());
						goto F_Cleanup;
				}
				
				Result = ReadFile(DiskHandle, DataBuffer, ClusterSize, &BytesTransferred, 0);
				if (!SUCCEEDED(Result) || BytesTransferred != ClusterSize) {
						printf("Readfile failed..\n");
						DisplayError(GetLastError());
						goto F_Cleanup;
				}
				
				Result = WriteFile(OutputHandle, DataBuffer, ClusterSize, &BytesTransferred, 0);
				if (!SUCCEEDED(Result) || BytesTransferred != ClusterSize) {
						printf("WriteFile failed..\n");
						DisplayError(GetLastError());
						goto F_Cleanup;
				}
		}
		printf("Disk stated saved to file %S\n",FBFname.c_str());
		// save bitmap info
		
		fbmetadatafname = GenerateFBMetadataFileName(Context->Volumes.Data[VolInfIndex].Letter);
		MetadataFileHandle = CreateFileW(
																		 fbmetadatafname.c_str(),
																		 GENERIC_WRITE,
																		 0, 0,
																		 CREATE_ALWAYS, 0, 0
																		 );
		if (MetadataFileHandle == INVALID_HANDLE_VALUE) {
				printf("Could not create file for metadata information.. \n");
				DisplayError(GetLastError());
				goto F_Cleanup;
		}
		
		
		Result = WriteFile(MetadataFileHandle, ClusterIndices, sizeof(UINT) * CurrentIndex, &BytesTransferred, 0);
		if (!SUCCEEDED(Result) || BytesTransferred != sizeof(UINT) * CurrentIndex) {
				printf("Failed to write metadata information to file..\n");
				printf("BytesTransferred => %d\n", BytesTransferred);
				printf("OpSize => %d\n", sizeof(UINT) * CurrentIndex);
				
				DisplayError(GetLastError());
				goto F_Cleanup;
		}
		printf("Disk bitmap info saved to the file \n");
		
		printf("You can start diff backup operation by pressing [d] and [enter]\n");
		
		Return = TRUE;
		F_Cleanup:
		CLEANMEMORY(Bitmap);
		CLEANMEMORY(ClusterIndices);
		CLEANMEMORY(DataBuffer);
		printf("Memory freed\n");
		
		CLEANHANDLE(DiskHandle);
		CLEANHANDLE(OutputHandle);
		CLEANHANDLE(MetadataFileHandle);
		printf("Handles freed\n");
		return Return;
}



/*
Change RestoreTarget to some struct, containing information needed for restore operation
*/
BOOLEAN
RestoreVolume(PLOG_CONTEXT Context, restore_inf* RestoreInf) {
		BOOLEAN Return = FALSE;
		//Restore to diff.
		HRESULT Result = 0;
		HANDLE FBackupFile = 0; //full backup data
		HANDLE FBackupMDFile = 0; //metadata for full backup
		HANDLE DBackupFile = 0; // diff backup data
		HANDLE DBackupMDFile = 0; //metadata for diff backup
		HANDLE VolumeToRestore = 0;
		DWORD BufferSize = 1024 * 1024 * 128; //128MB
		DWORD ClusterSize = RestoreInf->ClusterSize;
		DWORD BytesOperated = 0;
		DWORD FileSize = 0;
		data_array<UINT> FBClusterIndices = { 0,0 };
		data_array<nar_record> DBRecords = { 0, 0 };
		
		std::wstring dbfname = L"";
		std::wstring fbfname = L"";
		std::wstring fbmdfname = L"";
		std::wstring DMDFileName = L"";
		
		void* Buffer = malloc(BufferSize);
		if (Buffer == NULL) {
				printf("Can't allocate memory\n");
				DisplayError(GetLastError());
				goto R_Cleanup;
		}
		
		if (Buffer == NULL) {
				printf("Can't allocate buffer\n");
				DisplayError(GetLastError());
				goto R_Cleanup;
		}
		
		//Opening handles first
		fbfname = GenerateFBFileName(RestoreInf->SrcLetter);
		FBackupFile = CreateFileW(
															fbfname.c_str(),
															GENERIC_READ,
															FILE_SHARE_READ,
															0, OPEN_EXISTING, 0, 0
															);
		if (FBackupFile == INVALID_HANDLE_VALUE) {
				printf("Can't open fullbackup file for restore operation %S\n",fbfname.c_str());
				DisplayError(GetLastError());
				goto R_Cleanup;
		}
		
		fbmdfname = GenerateFBMetadataFileName(RestoreInf->SrcLetter);
		FBackupMDFile = CreateFileW(
																fbmdfname.c_str(),
																GENERIC_READ,
																FILE_SHARE_READ,
																0, OPEN_EXISTING, 0, 0
																);
		if (FBackupMDFile == INVALID_HANDLE_VALUE) {
				printf("Can't open fullbackup metadata file for restore operation %S\n",fbmdfname.c_str());
				DisplayError(GetLastError());
				goto R_Cleanup;
		}
		
		dbfname = GenerateDBFileName(RestoreInf->SrcLetter,RestoreInf->DiffVersion);
		DBackupFile = CreateFileW(
															dbfname.c_str(),
															GENERIC_READ,
															FILE_SHARE_READ,
															0, OPEN_EXISTING, 0, 0
															);
		if (DBackupFile == INVALID_HANDLE_VALUE) {
				printf("Can't open diffbackup file for restore operation %S\n",dbfname.c_str());
				DisplayError(GetLastError());
				goto R_Cleanup;
		}
		
		
		DMDFileName = GenerateDBMetadataFileName(RestoreInf->SrcLetter,RestoreInf->DiffVersion);
		
		DBackupMDFile = CreateFileW(
																DMDFileName.c_str(),
																GENERIC_READ,
																FILE_SHARE_READ,
																0, OPEN_EXISTING, 0, 0
																);
		if (FBackupMDFile == INVALID_HANDLE_VALUE) {
				printf("Can't open diffbackup metadata file for restore operation %S\n",DMDFileName.c_str());
				DisplayError(GetLastError());
				goto R_Cleanup;
		}
		
		WCHAR VTRNameTemp[] = L"\\\\.\\ :";
		VTRNameTemp[lstrlenW(VTRNameTemp) - 2] = RestoreInf->TargetLetter;
		
		VolumeToRestore = CreateFileW(
																	VTRNameTemp,
																	GENERIC_READ | GENERIC_WRITE,
																	FILE_SHARE_READ,
																	0,
																	OPEN_EXISTING,
																	0,
																	NULL
																	);
		if (VolumeToRestore == INVALID_HANDLE_VALUE) {
				printf("Can't open volume to restore %S\n", VTRNameTemp);
				DisplayError(GetLastError());
				goto R_Cleanup;
		}
		
		Result = DeviceIoControl(
														 VolumeToRestore,
														 FSCTL_LOCK_VOLUME,
														 0, 0, 0, 0, 0, 0
														 );
		if (!SUCCEEDED(Result)) {
				printf("Can't lock volume\n");
				DisplayError(GetLastError());
				goto R_Cleanup;
		}
		
		FileSize = GetFileSize(FBackupMDFile, 0);
		FBClusterIndices.Data = (UINT*)malloc(FileSize);
		FBClusterIndices.Count = FileSize / sizeof(UINT);
		Result = ReadFile(FBackupMDFile, FBClusterIndices.Data, FileSize, &BytesOperated, 0);
		if (!SUCCEEDED(Result) || BytesOperated != FileSize) {
				printf("Error occured while reading diff metadata file.. \n");
				printf("BytesRead => %d\n", BytesOperated);
				DisplayError(GetLastError());
				goto R_Cleanup;
		}
		
		
		LARGE_INTEGER MoveTo;
		LARGE_INTEGER NewFilePointer;
		MoveTo.QuadPart = 0;
		NewFilePointer.QuadPart = 0;
		
		for (UINT i = 0; i < FBClusterIndices.Count; i++) {
				MoveTo.QuadPart = ClusterSize * FBClusterIndices.Data[i];
				Result = SetFilePointerEx(VolumeToRestore, MoveTo, &NewFilePointer, FILE_BEGIN);
				if (!SUCCEEDED(Result) || MoveTo.QuadPart != NewFilePointer.QuadPart) {
						printf("Setfilepointerex failed!\n");
						printf("Iter was -> %i\n", i);
						DisplayError(GetLastError());
						goto R_Cleanup;
				}
				
				Result = ReadFile(FBackupFile, Buffer, ClusterSize, &BytesOperated, 0);
				if (!SUCCEEDED(Result) || BytesOperated != ClusterSize) {
						printf("ReadFile failed!\n");
						printf("Iter was -> %i\n", i);
						DisplayError(GetLastError());
						goto R_Cleanup;
				}
				
				Result = WriteFile(VolumeToRestore, Buffer, ClusterSize, &BytesOperated, 0);
				if (!SUCCEEDED(Result) || BytesOperated != ClusterSize) {
						printf("WriteFile failed!\n");
						printf("Iter was -> %i\n", i);
						DisplayError(GetLastError());
						goto R_Cleanup;
				}
		}
		printf("Restored to full, continuing to diff restore\n");
		
		FileSize = GetFileSize(DBackupMDFile, 0);
		DBRecords.Data = (nar_record*)malloc(FileSize);
		DBRecords.Count = FileSize / sizeof(nar_record);
		
		Result = ReadFile(DBackupMDFile, DBRecords.Data, FileSize, &BytesOperated, 0);
		if (!SUCCEEDED(Result) || BytesOperated != FileSize) {
				printf("Can't read diff metadata\n");
				DisplayError(GetLastError());
				goto R_Cleanup;
		}
		
		MoveTo.QuadPart = 0;
		NewFilePointer.QuadPart = 0;
		printf("DBMT Count -> %d\n", DBRecords.Count);
		
		for (UINT i = 0; i < DBRecords.Count; i++) {
				
				MoveTo.QuadPart = DBRecords.Data[i].StartPos * ClusterSize;
				Result = SetFilePointerEx(VolumeToRestore, MoveTo, &NewFilePointer, FILE_BEGIN);
				if (!SUCCEEDED(Result) || NewFilePointer.QuadPart != MoveTo.QuadPart) {
						printf("SetFilePointerEx failed\n");
						printf("Iter was -> %i\n", i);
						DisplayError(GetLastError());
						//goto R_Cleanup;
				}
				
				if (!CopyData(DBackupFile, VolumeToRestore, RestoreInf->ClusterSize * DBRecords.Data[i].Len)) {
						printf("Cant restore incremental parts\n");
						goto R_Cleanup;
				}
				
		}
		
		
		if (!RestoreMFT(RestoreInf, VolumeToRestore)) {
				printf("Couldnt restore MFT file\n");
		}
		
		printf("Restored to diff\n");
		Result = DeviceIoControl(
														 VolumeToRestore,
														 FSCTL_UNLOCK_VOLUME,
														 0, 0, 0, 0, 0, 0
														 );
		if (!SUCCEEDED(Result)) {
				printf("Can't unlock volume\n");
				DisplayError(GetLastError());
				goto R_Cleanup;
		}
		
		
		Return = TRUE;
		R_Cleanup:
		printf("Cleaning up restore operation..\n");
		CLEANHANDLE(FBackupFile);
		CLEANHANDLE(FBackupMDFile);
		CLEANHANDLE(DBackupFile);
		CLEANHANDLE(DBackupMDFile);
		CLEANHANDLE(VolumeToRestore);
		
		CLEANMEMORY(Buffer);
		CLEANMEMORY(FBClusterIndices.Data);
		CLEANMEMORY(DBRecords.Data);
		
		printf("Cleaned!\n");
		return Return;
}


//
//  Main uses a loop which has an assignment in the while 
//  conditional statement. Suppress the compiler's warning.
//

#pragma warning(push)
#pragma warning(disable:4706) // assignment within conditional expression

int
wmain(
			int argc,
			WCHAR* argv[]
			) {

	//region_chain* C = new region_chain;
	//
	//C->Rec = { 100,200 };
	//InsertToList(C, { 20,20 });
	//InsertToList(C, { 30,30 });
	//RemoveFromList(C);
	
#if 1
	region_chain** Chain = (region_chain**)malloc(sizeof(region_chain*));
	Chain[0] = (region_chain*)malloc(sizeof(region_chain));
	region_chain* C = Chain[0];
	
	C->Rec =        { 0  , 100};
	C->Next = 0;
	C->Back = 0;
	AppendToList(C, { 240, 80 });
	AppendToList(C, { 400, 70 });


	data_array<nar_record> d;
	d.Data = (nar_record*)malloc(sizeof(nar_record)*3);
	d.Data[0] = { 120,50 };
	d.Data[1] = { 200,100 };
	d.Data[2] = { 450,50 };
	d.Count = 3;
	RemoveDuplicates(Chain, &d, 0);
#endif 

#if 0
		data_array<nar_record> Test = { 0,0 };
		Test.Data = (nar_record*)malloc(sizeof(nar_record) * 11);
		Test.Data[0] = nar_record{ 0,112 };
		Test.Data[1] = nar_record{ 56,42 };
		Test.Data[2] = nar_record{ 900,94 };
		Test.Data[3] = nar_record{ 728,2 };
		Test.Data[4] = nar_record{ 300,230 };
		Test.Data[5] = nar_record{ 110,20 };
		Test.Data[6] = nar_record{ 700,465 };
		Test.Data[7] = nar_record{ 500,80 };
		Test.Data[8] = nar_record{ 110,5 };
		Test.Data[9] = nar_record{ 8200,40 };
		Test.Data[10] = nar_record{ 170,50 };
		Test.Count = 11;
		
		qsort(Test.Data, Test.Count, sizeof(nar_record), CompareNarRecords);
		
		for (int i = 0; i < Test.Count; i++) {
				printf("%I64d\t%I64d\n", Test.Data[i].StartPos, Test.Data[i].Len);
		}
		
		
		UINT32 MergedRecordsIndex = 0;
		UINT32 CurrentIter = 0;
		
		for (;;) {
				if (CurrentIter == Test.Count - 1) {
						//Special case for the last element in the array
						if (!IsRegionsCollide(&Test.Data[CurrentIter], &Test.Data[MergedRecordsIndex - 1])) {
								Test.Data[MergedRecordsIndex] = Test.Data[CurrentIter];
								MergedRecordsIndex++;
						}
						break;
				}
				if (CurrentIter >= Test.Count) break;
				
				Test.Data[MergedRecordsIndex].StartPos = Test.Data[CurrentIter].StartPos;
				Test.Data[MergedRecordsIndex].Len = Test.Data[CurrentIter].Len;
				
				ULONGLONG EndPointTemp = Test.Data[CurrentIter].StartPos + Test.Data[CurrentIter].Len;
				
				while (Test.Data[MergedRecordsIndex].StartPos == Test.Data[CurrentIter].StartPos
							 || IsRegionsCollide(&Test.Data[MergedRecordsIndex], &Test.Data[CurrentIter])) {
						
						ULONGLONG EP1 = Test.Data[CurrentIter].StartPos + Test.Data[CurrentIter].Len;
						ULONGLONG EP2 = Test.Data[MergedRecordsIndex].StartPos + Test.Data[MergedRecordsIndex].Len;
						
						EndPointTemp = MAX(EP1, EP2);
						Test.Data[MergedRecordsIndex].Len = EndPointTemp - Test.Data[MergedRecordsIndex].StartPos;
						
						CurrentIter++;
						
				}
				
				Test.Data[MergedRecordsIndex].Len = EndPointTemp - Test.Data[MergedRecordsIndex].StartPos;
				MergedRecordsIndex++;
				if (RecordEqual(&Test.Data[MergedRecordsIndex],&Test.Data[CurrentIter])) { 
						CurrentIter++; 
				}
				
		}
		
		Test.Count = MergedRecordsIndex;
		realloc(Test.Data, Test.Count * sizeof(Test.Data[0]));
		printf("\n\n\n");
		for (int i = 0; i < Test.Count; i++) {
				printf("%I64d\t%I64d\n", Test.Data[i].StartPos, Test.Data[i].Len);
		}
		
		return 0;
#endif 
		
		
		WCHAR TrackedVolumeName[] = L"ASDFASF";
		WCHAR RestoreVolumeName[] = L"ASDFASDFAS";
		
		
		HANDLE port = INVALID_HANDLE_VALUE;
		HRESULT hResult = S_OK;
		DWORD result;
		LOG_CONTEXT context = { 0 };
		
		
		hResult = CoInitialize(NULL);
		if (!SUCCEEDED(hResult)) {
				printf("Failed CoInitialize function \n");
				DisplayError(GetLastError());
				goto Main_Exit;
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
				printf("Failed CoInitializeSecurity function \n");
				DisplayError(GetLastError());
				goto Main_Exit;
		}
		
		
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
		context.ShutDown = NULL;
		context.CleaningUp = FALSE;
		context.Thread = 0;
		context.Volumes = { 0,0 };
		//
		// Create the thread to read the log records that are gathered
		// by MiniSpy.sys.
		//
		
		NarCreateThreadCom(&context);
		
#if 1
#pragma region NAR_TEST
		WCHAR instanceName[INSTANCE_NAME_MAX_CHARS + 1];
		
#pragma endregion
#endif 
		UINT NDiffBackedUp = 0;
		
		for (;;) {
				std::wstring Input;
				std::wcin >> Input;
				auto Answer = Split(Input, L",");
				
				if (Answer[0] == L"q") {
						goto Main_Cleanup;
				}
				else if (Answer[0] == L"diff") {
						int Index = -1;
						wchar_t Letter = Answer[1][0];
						if (context.Volumes.Data == NULL) {
								printf("You should take fullbackup of the volume first\n");
								continue;
						}
						for (int i = 0; i < context.Volumes.Count; i++) {
								if (context.Volumes.Data[i].Letter = Letter) {
										Index = i;
										break;
								}
						}
						printf("INDEX AT LIST %d\n", Index);
						DiffBackupVolume(&context, Index); //Hard coded
				}
				else if (Answer[0] == L"full") {
						wchar_t ReqLetter = Answer[1][0];
						int Index = -1;
						AddVolumeToTrack(&context, ReqLetter);
						for (int i = 0; i < context.Volumes.Count; i++) {
								if (context.Volumes.Data[i].Letter == ReqLetter) {
										Index = i;
										break;
								}
						}
						printf("Test\n");
						FullBackupVolume(&context, Index);
				}
				else if (Answer[0] == L"restore") {
						//TODO construct restore structure			
						wchar_t SrcLetter = Answer[1][0];
						wchar_t RestoreTarget = Answer[2][0];
						int DiffIndex = std::stoi(Answer[3]);
						
						restore_inf RestoreInf;
						RestoreInf.SrcLetter = SrcLetter;
						RestoreInf.ClusterSize = 4096;
						RestoreInf.TargetLetter = RestoreTarget;
						RestoreInf.DiffVersion = DiffIndex;
						RestoreInf.ToFull = FALSE;
						
						for (int i = 0; i < context.Volumes.Count; i++) {
								if (context.Volumes.Data[i].Letter = SrcLetter) {
										DetachVolume(&context, i);
										break;
								}
						}
						
						RestoreVolume(&context, &RestoreInf);
						
				}
				else {
						//TODO
				}
				
				Sleep(50);
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
		
		
		Main_Exit:
		
		//
		// Clean up the data that is always around and exit
		//
		
		if (context.ShutDown) {
				
				CloseHandle(context.ShutDown);
		}
		
		if (INVALID_HANDLE_VALUE != port) {
				CloseHandle(port);
		}
		return 0;
}


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