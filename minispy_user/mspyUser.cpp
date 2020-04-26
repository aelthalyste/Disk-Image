/*
IMPLEMENTED @MergeRegions function
  So problem is, which I can not describe properly but at least can show it
  Let Rn be the record with at the index of N, so R0 becomes first record, M second and so on

  Let assume there are three records, at least one is overlapping with other records regions

  M =>                      |-----------------|
  S =>         |--------------|
  R3 =>                                |------------------|
  We can shrink that information such that, only storing new record like start => S.start and len = R3.Len + R3.Start

  If such collisions ever occurs, we can shrink them.
  But in order to process that, list MUST be sorted so we can compare consequent list elements
*/


#include <iostream> //TODO remove this

#include <DriverSpecs.h>
_Analysis_mode_(_Analysis_code_type_user_code_)

#include <stdlib.h>
#include <stdio.h>
#include <cstdio>
#include <windows.h>
#include <assert.h>
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

#include "mspyLog.h"



void
MergeRegions(data_array<nar_record>* R) {
    UINT32 MergedRecordsIndex = 0;
    UINT32 CurrentIter = 0;
    
    for (;;) {
        if (CurrentIter >= R->Count) {
            break;
        }
        
        UINT32 EndPointTemp = R->Data[CurrentIter].StartPos + R->Data[CurrentIter].Len;
        
        if (IsRegionsCollide(&R->Data[MergedRecordsIndex], &R->Data[CurrentIter])) {
            UINT32 EP1 = R->Data[CurrentIter].StartPos + R->Data[CurrentIter].Len;
            UINT32 EP2 = R->Data[MergedRecordsIndex].StartPos + R->Data[MergedRecordsIndex].Len;
            
            EndPointTemp = MAX(EP1, EP2);
            R->Data[MergedRecordsIndex].Len = EndPointTemp - R->Data[MergedRecordsIndex].StartPos;
            
            CurrentIter++;
        }
        else {
            MergedRecordsIndex++;
            R->Data[MergedRecordsIndex] = R->Data[CurrentIter];
        }
        
        
    }
    R->Count = MergedRecordsIndex + 1;
    R->Data = (nar_record*)realloc(R->Data, sizeof(nar_record) * R->Count);
    
}

#if 0 // OPTIMIZED INCREMENTAL MERGE ALORITHM

void
MergeRegions(region_chain* Chain) {
    UINT32 MergedRecordsIndex = 0;
    UINT32 CurrentIter = 0;
    region_chain* R = Chain->Next;
    
    for (;;) {
        if (R == NULL || R->Next == NULL) {
            break;
        }
        
        
        if (IsRegionsCollide(&R->Rec, &R->Next->Rec)) {
            UINT32 EP1 = R->Rec.StartPos + R->Rec.Len;
            UINT32 EP2 = R->Next->Rec.StartPos + R->Next->Rec.Len;
            UINT32 EndPointTemp = MAX(EP1, EP2);
            
            R->Rec.Len = EndPointTemp - R->Rec.StartPos;
            RemoveFromList(R->Next);
            if (R->Next == NULL) break;
        }
        else {
            R = R->Next;
        }
        
    }
    
}

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
        New->Index = 0;
        
    }
    
    /*Not checking Root->Back since it is impossible to replace head with Insert*/
    
}

void
AppendToList(region_chain* AnyPoint, nar_record Rec) {
    /*Find tail of the list*/
    while (AnyPoint->Next) AnyPoint = AnyPoint->Next;
    //Append new data
    AnyPoint->Next = (region_chain*)malloc(sizeof(region_chain));
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
    
    //	R->Back = 0;
    //	R->Next = 0;
    
    //CLEANMEMORY(R); //TODO maybe not clean memory here ?
}

void
PrintList(region_chain* Temp) {
    printf("\n######\n");
    while (Temp != NULL) {
        printf("%I64d\t%I64d\n", Temp->Rec.StartPos, Temp->Rec.Len);
        Temp = Temp->Next;
    }
    printf("\n######\n");
}

void
PrintListReverse(region_chain* Temp) {
    printf("###\n");
    while (Temp->Next) Temp = Temp->Next;
    while (Temp != NULL) {
        printf("%I64d\t%I64d\n", Temp->Rec.StartPos, Temp->Rec.Len);
        Temp = Temp->Back;
    }
    printf("###\n");
}


/*
Finds chain's record offset (in Cluster count) at incremental chunk.
Return: ClusterIndex
*/
DWORD
FindPosition(nar_record* R, data_array<nar_record>* M) {
    // TODO: binary search
    UINT i = 0;
    DWORD Return = 0;
    while (R->StartPos >= M->Data[i].StartPos) {
        Return += M->Data[i].Len; //Incrementing offset
        i++;
    }
    
    Return += (-M->Data[i - 1].Len + R->StartPos - M->Data[i - 1].StartPos);
    
    return Return;
}

/*
S: Source
*/
region_chain*
CopyChain(region_chain* S) {
    
    region_chain* Head = GetListHead(*S);
    
    region_chain* Return = (region_chain*)malloc(sizeof(region_chain));
    Return->Rec = Head->Rec;
    Return->Next = 0;
    Return->Back = 0;
    Head = Head->Next;
    
    while (Head) {
        AppendToList(Return, Head->Rec);
        Head = Head->Next;
    }
    
    return Return;
}


/*
Return:
S: Shadow region
M: Metadata reion

LEFT if M endpoint < S.start, M falls left side of the S
RIGHT if M start > S endpoint, M falls right side of the S
COLLISION if two regions collide, M and S collides
OVERRUNS, if S overruns M
SPLIT, if S splits M

Do not use this function to check region collisions,
instead use IsRegionsCollide.
*/
inline rec_or
GetOrientation(nar_record* M, nar_record* S) {
    rec_or Return = (rec_or)0;
    
    UINT32 MEP = M->StartPos + M->Len;
    UINT32 SEP = S->StartPos + S->Len;
    
    if (MEP <= S->StartPos) {
        Return = rec_or::LEFT;
    }
    else if (M->StartPos >= SEP) {
        Return = rec_or::RIGHT;
    }
    else if (SEP >= MEP && S->StartPos <= M->StartPos) {
        Return = rec_or::OVERRUN;
    }
    else if ((SEP < MEP && M->StartPos < S->StartPos)) {
        Return = rec_or::SPLIT;
    }
    else {
        Return = rec_or::COLLISION;
    }
    
    return Return;
}

/*
Recursively removing duplicate regions from incremental change logs.
First element of metadatas is ID to be restored, 0th is fullbackup regions
For entry point, MDShadow == Metadatas[Count]

Since we are iterating back, no need to use data_array, just check ID > 0
*/

void
RemoveDuplicates(region_chain** Metadatas,
                 region_chain* MDShadow, int ID) {
    
    if (ID == 0) return; //End of recursion TODO skip full backup
    
    region_chain* MReg = Metadatas[ID]->Next;
    region_chain* SReg = MDShadow->Next;
    PrintList(MReg);
    PrintList(SReg);
    
    for (;;) {
        if (MReg == NULL || SReg == NULL) {
            break;
        }
        
        rec_or Ori = GetOrientation(&MReg->Rec, &SReg->Rec);
        
        if (Ori == LEFT) {
            MReg = MReg->Next;
        }
        else if (Ori == RIGHT) {
            SReg = SReg->Next;
        }
        else if (Ori == COLLISION || Ori == OVERRUN || Ori == SPLIT) {
            
            while (TRUE) {
                
                if (MReg == NULL || SReg == NULL) break;
                
                Ori = GetOrientation(&MReg->Rec, &SReg->Rec);
                if (Ori != OVERRUN && Ori != COLLISION && Ori != SPLIT) {
                    MReg = MReg->Next;
                    break;
                }
                
                /*Region's shadow, splits record into two*/
                if (Ori == OVERRUN || Ori == SPLIT) {
                    
                    if (Ori == OVERRUN) {
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
                        //Ori == SPLIT
                        /*
                        Shadow splits region into two block
                        */
                        
                        nar_record LeftReg = { 0,0 };
                        LeftReg.StartPos = MReg->Rec.StartPos;
                        LeftReg.Len = SReg->Rec.StartPos - MReg->Rec.StartPos;
                        InsertToList(MReg, LeftReg);
                        RemoveFromList(MReg);
                        nar_record RightReg = { 0,0 };
                        
                        //End of shadow region
                        UINT32 SREP = SReg->Rec.StartPos + SReg->Rec.Len;
                        
                        //End of metadata region
                        UINT32 MREP = MReg->Rec.StartPos + MReg->Rec.Len;
                        
                        RightReg.StartPos = SREP;
                        RightReg.Len = MREP - SREP;
                        InsertToList(MReg->Next, RightReg);
                        /*
                        Since MReg is split into two, It can't be used anymore,
                        remove it and iterate to next one, left region we just split,
                        which is completed it's operation, but right one might be colliding
                        next with MDShadow element, so iterate to next one and check them
                        */
                        MReg = MReg->Next->Next; //Right region
                        SReg = SReg->Next;
                        
                        
                    }//End of if shadow overruns region
                    
                }//End of if Count == 2
                else if (Ori == COLLISION) {
                    
                    /*Regions collide default way*/
                    
                    //End point of metadata
                    UINT32 MEP = MReg->Rec.StartPos + MReg->Rec.Len;
                    //End point of shadow
                    UINT32 SEP = SReg->Rec.StartPos + SReg->Rec.Len;
                    
                    if (SEP >= MEP) {
                        /*
                        Collision deleted right side of the metadata
                        */
                        nar_record New = { 0,0 };
                        New.StartPos = MReg->Rec.StartPos;
                        New.Len = SReg->Rec.StartPos - MReg->Rec.StartPos;
                        InsertToList(MReg, New);
                        RemoveFromList(MReg);
                        
                        //This does not clear MReg's pointers, so we can keep on iterating
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
            
                        Since MReg->Next is region we just created, and we know it is non-colliding, we should
                        iterate one more.
                        SReg may be colliding with next region, better keep it unchanged.
                     */
                        MReg = MReg->Next->Next;
                        
                    }
                    else {
                        /*Collision will delete left side of the metadata*/
                        
                        /*
                              OLD                        |*****-----| stars will be deleted
                              SHADOW               |----------|  |--------|
                              Collision deleted left side of the metadata,
                              since there MAY be another region colliding with metadata,
                              we should break here, then check for another collision with new region
                              */
                        nar_record New = { 0,0 };
                        New.StartPos = SReg->Rec.StartPos + SReg->Rec.Len;
                        New.Len = MEP - New.StartPos;
                        InsertToList(MReg, New);
                        RemoveFromList(MReg);
                        
                        MReg = MReg->Next;
                        SReg = SReg->Next;
                        
                    }
                    
                    
                    break;
                }
                
                
                
            }//End of while
            
        }//End of IF COLLISION
        
        
    }
    
    
    /*
     Special case: If list's head is removed, then Metadata[ID] need to be updated
    */
    
#if 1
    //printf("\n$$$$\n");
    //PrintList(Metadatas[ID]);
    //PrintList(MDShadow);
    //printf("$$$$\n");
    
    /*Update shadow*/
    MReg = Metadatas[ID]->Next;
    SReg = MDShadow->Next; // 0th element is 0-0
    
    PrintList(MReg);
    PrintList(SReg);
    
    
    for (;;) {
        if (MReg == NULL || SReg == NULL) break;
        
        DWORD MEP = MReg->Rec.StartPos + MReg->Rec.Len;
        DWORD SEP = SReg->Rec.StartPos + SReg->Rec.Len;
        /*These conditions checks if shadow needs to be updated */
        
        if (SEP < MReg->Rec.StartPos) {
            /*
                 |--------|md
             |---------|shadow
            */
            InsertToList(SReg, MReg->Rec);
            SReg = SReg->Next->Next;
        }
        else if (MEP < SReg->Rec.StartPos) {
            /*
             |--------|md
                 |---------|shadow
            */
            InsertToList(SReg->Back, MReg->Rec);
            MReg = MReg->Next;
        }
        else {//Regions need to be merged
            
            for (;;) {
                
                if (MReg == NULL || SReg == NULL) break;
                
                if (MEP == SReg->Rec.StartPos) {
                    /*Join shadow from left side*/
                    SReg->Rec.StartPos = MReg->Rec.StartPos;
                    SReg->Rec.Len += MReg->Rec.Len;
                    MReg = MReg->Next;
                }
                else if (SEP == MReg->Rec.StartPos) {
                    /*
                  Join shadow from right side
                       Since shadow is extended, it may touch next region too.
                        Iterate one more time to find out
                  */
                    SReg->Rec.Len += MReg->Rec.Len;
                    MReg = MReg->Next;
                }
                else {
                    /*
                  IF none of the above happened, no regions in contact,
                  keep iterating at upper level for
                  */
                    break;
                }
                //Else
                
            }
            
        }
        
    }
#endif
    
    MergeRegions(MDShadow);
    //printf("\n$$$$\n");
    PrintList(Metadatas[ID]);
    PrintList(MDShadow);
    //printf("$$$$\n");
    
    RemoveDuplicates(Metadatas, MDShadow, ID - 1);
    
}

#endif

data_array<nar_record>
ReadMetadata(HANDLE F) {
    
    data_array<nar_record> Return = { 0,0 };
    
    SetFilePointer(F, 0, 0, FILE_BEGIN);
    DWORD FileSize = GetFileSize(F, 0);
    
    if (FileSize != 0) {
        DWORD BytesRead = 0;
        Return.Data = (nar_record*)malloc(FileSize);
        Return.Count = FileSize / sizeof(nar_record);
        if (Return.Data != NULL) {
            if (!ReadFile(F, Return.Data, FileSize, &BytesRead, 0) || BytesRead != FileSize) {
                printf("Cant read file\n");
                printf("Tried to read -> %d, instead read -> %d\n", FileSize, BytesRead);
                
                DisplayError(GetLastError());
                CLEANMEMORY(Return.Data);
                Return.Data = 0;
                Return.Count = 0;
            }
        }
        
    }
    else {
        printf("Filesize is zero\n");
        Return.Data = 0;
        Return.Count = 0;
    }
    
    return Return;
}

data_array<nar_record>
ReadFBMetadata(HANDLE F) {
    data_array<nar_record> Return = { 0,0 };
    Return.Insert({ 0,1 }); // NOTE assumption made, disk's first cluster is always marked
    
    SetFilePointer(F, 0, 0, FILE_BEGIN);
    DWORD FileSize = GetFileSize(F, 0);
    
    if (FileSize != 0) {
        DWORD BytesRead = 0;
        DWORD Last = 0;
        
        UINT* Buffer = (UINT*)malloc(FileSize);
        DWORD Count = FileSize / sizeof(UINT);
        
        if (Buffer != NULL) {
            
            if (ReadFile(F, Buffer, FileSize, &BytesRead, 0) && BytesRead == FileSize) {
                
                for (int i = 1; i < Count; i++) {
                    
                    if (Buffer[i] == Last + 1) {
                        Return.Data[Return.Count - 1].Len++;
                    }
                    else {
                        Return.Insert({ Buffer[i],1 });
                    }
                    Last = Buffer[i];
                    
                }
                
            }
            else {
                printf("Could not read file\n");
                DisplayError(GetLastError());
                CLEANMEMORY(Return.Data);
            }
            
            CLEANMEMORY(Buffer);
            
        }
        else {
            printf("Memory allocation error at ReadFBMetadata function\n");
            
            Return.Data = 0;
            Return.Count = 0;
            CLEANMEMORY(Return.Data);
        }
        
    }
    else {
        printf("File size is zero\n");
        Return.Data = 0;
        Return.Count = 0;
    }
    
    return Return;
    
}


BOOLEAN
CopyData(HANDLE S, HANDLE D, ULONGLONG Len) {
    BOOLEAN Return = TRUE;
    
    UINT32 BufSize = 1024 * 1024 * 64; //64MB
    
    void* Buffer = malloc(BufSize);
    if (Buffer != NULL) {
        ULONGLONG BytesRemaining = Len;
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
    std::wstring Return = MFT_FILE_NAME;
    Return += Letter;
    Return += std::to_wstring(ID);
    return Return;
}

inline std::wstring
GenerateMFTMetadataFileName(wchar_t Letter, int ID) {
    std::wstring Return = MFT_LCN_FILE_NAME;
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
SaveMFT(volume_backup_inf* VolInf, HANDLE VSSHandle, data_array<nar_record>* MFTLCN) {
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
    std::wstring MFTFileName = GenerateMFTFileName(VolInf->Letter, VolInf->CurrentLogIndex);
    HANDLE MFTSaveFile = CreateFileW(
                                     MFTFileName.c_str(),
                                     GENERIC_WRITE, 0, 0,
                                     CREATE_ALWAYS, 0, 0
                                     );
    
    std::wstring MFTMetadataFName = GenerateMFTMetadataFileName(VolInf->Letter, VolInf->CurrentLogIndex);
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
        for (unsigned int i = 0; i < MFTLCN->Count; i++) {
            MoveTo.QuadPart = (ULONGLONG)MFTLCN->Data[i].StartPos * (ULONGLONG)VolInf->ClusterSize;
            
            Result = SetFilePointerEx(VSSHandle, MoveTo, &NewFilePointer, FILE_BEGIN);
            if (!SUCCEEDED(Result) || MoveTo.QuadPart != NewFilePointer.QuadPart) {
                printf("Set file pointer failed\n");
                printf("Tried to set pointer -> %I64d\n", MoveTo.QuadPart);
                printf("Instead, moved to -> %I64d\n", NewFilePointer.QuadPart);
                DisplayError(GetLastError());
                goto Exit;
            }
            
            if (!CopyData(VSSHandle, MFTSaveFile, (ULONGLONG)MFTLCN->Data[i].Len * (ULONGLONG)VolInf->ClusterSize)) {
                printf("Error occured while copying MFT file\n");
                goto Exit;
            }
            
        }
        DWORD WriteSize = sizeof(nar_record) * MFTLCN->Count;
        Result = WriteFile(MFTMDFile, MFTLCN->Data, WriteSize, &BytesOperated, 0);
        if (!SUCCEEDED(Result) || WriteSize != BytesOperated) {
            printf("Cant save mft metadata to file %S\n", MFTMetadataFName.c_str());
            printf("Bytes written -> %I64d, tried -> %u\n", BytesOperated);
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
RestoreMFT(restore_inf* R, HANDLE VolumeHandle) {
    BOOLEAN Return = FALSE;
    DWORD BufferSize = 4096;
    void* Buffer = malloc(BufferSize);
    
    HANDLE MFTMDHandle = INVALID_HANDLE_VALUE;
    std::wstring MFTFileName = GenerateMFTFileName(R->SrcLetter, R->Version);
    HANDLE MFTHandle = CreateFileW(
                                   MFTFileName.c_str(),
                                   GENERIC_READ, 0, 0,
                                   OPEN_EXISTING, 0, 0
                                   );
    //printf("Restoring MFT with file %S\n", MFTFileName.c_str());
    
    if (MFTHandle != INVALID_HANDLE_VALUE) {
        std::wstring MFTMDFileName = GenerateMFTMetadataFileName(R->SrcLetter, R->Version);
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
                    MoveTo.QuadPart = (ULONGLONG)Metadata.Data[i].StartPos * (ULONGLONG)R->ClusterSize;
                    
                    Result = SetFilePointerEx(VolumeHandle, MoveTo, &NewFilePointer, FILE_BEGIN);
                    if (!SUCCEEDED(Result) || MoveTo.QuadPart != NewFilePointer.QuadPart) {
                        printf("Set file pointer failed\n");
                        printf("Tried to set pointer -> %I64d\n", MoveTo.QuadPart);
                        printf("Instead, moved to -> %I64d\n", NewFilePointer.QuadPart);
                        DisplayError(GetLastError());
                        goto Exit;
                    }
                    
                    if (!CopyData(MFTHandle, VolumeHandle, (ULONGLONG)Metadata.Data[i].Len * (ULONGLONG)R->ClusterSize)) {
                        goto Exit;
                    }
                    
                }
                Return = TRUE;
                
            }
            else {
                printf("Couldnt read mft metadata\n");
                DisplayError(GetLastError());
            }
            
        }
    }
    else {
        printf("MFTHandle can't be invalid\n");
        Return = FALSE;
    }
    
    Exit:
    CLEANHANDLE(MFTMDHandle);
    CLEANHANDLE(MFTHandle);
    CLEANMEMORY(Buffer);
    return Return;
}


inline BOOLEAN
InitRestoreTargetInf(restore_inf* Inf, wchar_t Letter) {
    
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


inline BOOLEAN
InitVolumeInf(volume_backup_inf* VolInf, wchar_t Letter, BackupType Type) {
    //TODO, initialize VolInf->PartitionName
    
    BOOLEAN Return = FALSE;
    
    VolInf->Letter = Letter;
    VolInf->FilterFlags.IsActive = FALSE;
    VolInf->FullBackupExists = FALSE;
    VolInf->BT = Type;
    
    VolInf->FilterFlags.SaveToFile = TRUE;
    VolInf->FilterFlags.FlushToFile = FALSE;
    
    VolInf->CurrentLogIndex = 0;
    
    VolInf->ClusterSize = 0;
    VolInf->RecordsMem.clear();
    VolInf->VSSPTR = 0;
    
    DWORD BytesPerSector = 0;
    DWORD SectorsPerCluster = 0;
    DWORD ClusterCount = 0;
    
    wchar_t DN[] = L"C:";
    DN[0] = Letter;
    if (QueryDosDeviceW(DN, VolInf->DOSName, 32) == 0) {
        printf("Failed to get device's DOS name\n");
        return FALSE;
    }
    
    wchar_t V[] = L"C:\\";
    V[0] = Letter;
    if (GetDiskFreeSpaceW(V, &SectorsPerCluster, &BytesPerSector, 0, &ClusterCount)) {
        VolInf->ClusterSize = SectorsPerCluster * BytesPerSector;
        Return = TRUE;
    }
    else {
        printf("Cant get disk information from WINAPI\n");
        DisplayError(GetLastError());
    }
    
    VolInf->ExtraPartitions = { 0,0 }; // TODO(Batuhan):
    VolInf->LogHandle = INVALID_HANDLE_VALUE;
    VolInf->IncRecordCount = 0;
    
    VolInf->Stream.Records = { 0,0 };
    VolInf->Stream.Handle = 0;
    VolInf->Stream.RecIndex = 0;
    VolInf->Stream.ClusterIndex = 0;
    
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

wchar_t*
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
    
    //TODO LEAK VSS_SNAPSHOT_PROP
    wchar_t* Result = (wchar_t*)malloc(sizeof(wchar_t) * lstrlenW(SnapshotProp.m_pwszSnapshotDeviceObject));
    
    Result = lstrcpyW(Result, SnapshotProp.m_pwszSnapshotDeviceObject);
    
    return Result;
}


inline BOOL
IsRegionsCollide(nar_record* R1, nar_record* R2) {
    BOOL Result = FALSE;
    UINT32 R1EndPoint = R1->StartPos + R1->Len;
    UINT32 R2EndPoint = R2->StartPos + R2->Len;
    
    if (R1->StartPos == R2->StartPos && R1->Len == R2->Len) {
        return TRUE;
    }
    
    if ((R1EndPoint <= R2EndPoint
         && R1EndPoint >= R2->StartPos)
        || (R2EndPoint <= R1EndPoint
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
    // NOTE: Context->Thread MUST NOT be null before entering this function.
    
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
    if (Context->Thread != NULL || Context->ShutDown != NULL) {
        printf("THREAD OR SHUTDOWN WASNT NULL\n");
    }
    
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
            printf("Communication thread created successfully\n");
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
    // TODO: make FileName optional, if it is not given, do not try to open file,
    //return empty string
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
    std::vector<std::string> Result;
    Result.reserve(100);
    
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
    data_array<nar_record> Result = { 0,0 };
    
    static char Command[] = "fsutil file queryextents C:\\$mft csv >TempTestFile";
    Command[25] = VolumeLetter;
    std::string Answer = NarExecuteCommand(Command, "TempTestFile");
    
    if (Answer[0] == '\n') Answer[0] = '#';
    Answer = Answer.substr(Answer.find("\n") + 1, Answer.size());
    
    auto Rows = Split(Answer, "\n");
    
    
    for (const auto& Iter : Rows) {
        auto Vals = Split(Iter, ",");
        nar_record r = { 0, 0 };
        r.StartPos = std::stoull(Vals[2], 0, 16);
        r.Len = std::stoull(Vals[1], 0, 16);
        Result.Insert(r);
    }
    
    return Result;
}





/*
This operation just adds volume to list, does not starts to filter it,
until it's fullbackup is requested. After fullbackup, call AttachVolume to start filtering
*/
BOOLEAN
AddVolumeToTrack(PLOG_CONTEXT Context, wchar_t Letter, BackupType Type) {
    BOOLEAN ErrorOccured = TRUE;
    
    volume_backup_inf VolInf;
    if (InitVolumeInf(&VolInf, Letter, Type)) {
        ErrorOccured = FALSE;
    }
    Context->Volumes.Insert(VolInf);
    
    printf("VolClusterSize => %d\n", Context->Volumes.Data[0].ClusterSize);
    
    printf("Volume inserted to the list\n");
    return !ErrorOccured;
}

BOOLEAN
DetachVolume(volume_backup_inf* VolInf) {
    BOOLEAN Return = TRUE;
    HRESULT Result = 0;
    std::wstring Temp = L"!:\\";
    Temp[0] = VolInf->Letter;
    
    Result = FilterDetach(MINISPY_NAME, Temp.c_str(), 0);
    
    if (SUCCEEDED(Result)) {
        VolInf->FilterFlags.IsActive = FALSE;
    }
    else {
        printf("Can't detach filter\n");
        printf("Result was -> %d\n", Result);
        DisplayError(Result);
        Return = FALSE;
    }
    
    return Return;
}


/*
Attachs filter
SetActive: default value is TRUE
*/
inline BOOLEAN
AttachVolume(volume_backup_inf* VolInf, BOOLEAN SetActive) {
    
    BOOLEAN Return = FALSE;
    HRESULT Result = 0;
    
    WCHAR Temp[] = L"!:\\";
    Temp[0] = VolInf->Letter;
    
    Result = FilterAttach(MINISPY_NAME, Temp, 0, 0, 0);
    if (SUCCEEDED(Result) || Result == ERROR_FLT_INSTANCE_NAME_COLLISION) {
        VolInf->FilterFlags.IsActive = SetActive;
        Return = TRUE;
    }
    else {
        printf("Can't attach filter\n");
        DisplayError(GetLastError());
    }
    
    return Return;
}


INT
GetVolumeID(PLOG_CONTEXT C, wchar_t Letter) {
    
    INT ID = -1;
    
    if (C->Volumes.Data != NULL) {
        if (C->Volumes.Count != 0) {
            for (int i = 0; i < C->Volumes.Count; i++) {
                if (C->Volumes.Data[i].Letter == Letter) {
                    ID = i;
                    break;
                }
            }
        }
        else {
            printf("Context's volume array is empty");
        }
        
    }
    else {
        printf("Context's volume array was null\n");
    }
    
    return ID;
}

BOOLEAN
ReadStream(volume_backup_inf* VolInf, void* Buffer, int TotalSize) {
    //TotalSize MUST be multiple of cluster size
    BOOLEAN Result = TRUE;
    
    int ClustersToRead = TotalSize / VolInf->ClusterSize;
    int BufferOffset = 0;
    DWORD BytesOperated = 0;
    
    if (ClustersToRead != 1)printf("Clusters to read wasnt one, instead = %i\n", ClustersToRead);
    
    for (;;) {
        
        if (VolInf->Stream.RecIndex == VolInf->Stream.Records.Count) {
            /*Last region and error TODO write doc*/
            printf("No regions left to read, overflow error!\n");
            break;
        }
        
        LARGE_INTEGER MoveTo = { 0 };
        LARGE_INTEGER NewFilePointer = { 0 };
        
        MoveTo.QuadPart = (ULONGLONG)VolInf->ClusterSize * ((ULONGLONG)VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].StartPos + (ULONGLONG)VolInf->Stream.ClusterIndex);
        if (!SetFilePointerEx(VolInf->Stream.Handle, MoveTo, &NewFilePointer, 0) || MoveTo.QuadPart != NewFilePointer.QuadPart) {
            printf("Can't set file pointer\n");
            DisplayError(GetLastError());
            break;
        }
        
        int CRemainingRegion = VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].Len - VolInf->Stream.ClusterIndex;
        
        //Write offset from the buffer, so new data dont overlap with old one
        void* BTemp = (BYTE*)Buffer + BufferOffset;
        
        if (ClustersToRead > CRemainingRegion) {
            //Read request exceeds current region
            printf("MUST NOT SEE THAT\n");
            DWORD RS = CRemainingRegion * VolInf->ClusterSize;
            if (RS == 0) printf("## RS WAS ZERO ##\n");
            if (!ReadFile(VolInf->Stream.Handle, BTemp, RS, &BytesOperated, 0) || BytesOperated != RS) {
                Result = FALSE;
                break;
            }
            BufferOffset += RS;
            ClustersToRead -= CRemainingRegion;
            VolInf->Stream.ClusterIndex = 0;
            VolInf->Stream.RecIndex++;
            
        }
        else {
            
            DWORD RS = ClustersToRead * VolInf->ClusterSize;
            if (RS == 0) printf("## RS WAS ZERO ##\n");
            if (!ReadFile(VolInf->Stream.Handle, BTemp, RS, &BytesOperated, 0) || BytesOperated != RS) {
                printf("Cant read data for streaming, bytes operated-> %d, requested ->%d, ", BytesOperated, RS);
                printf("(recindex %i, clusterindex %i)", VolInf->Stream.RecIndex, VolInf->Stream.ClusterIndex);
                printf("(reg_len %i, reg_count %i)\n", VolInf->Stream.Records.Count, VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].Len);
                DisplayError(GetLastError());
                Result = FALSE;
                break;
            }
            VolInf->Stream.ClusterIndex += ClustersToRead;
            if (VolInf->Stream.ClusterIndex == VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].Len) {
                //Whole region has been read, iterate next one
                VolInf->Stream.ClusterIndex = 0;
                VolInf->Stream.RecIndex++;
            }
            if (VolInf->Stream.ClusterIndex > VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].Len) {
                //That must not be possible
                printf("ClusterIndex exceeded region len, that MUST NOT happen at any circumstance\n");
                Result = FALSE;
            }
            ClustersToRead = 0;
            break;
            //Read request fits current region
        }
        
    }
    
    return Result;
}

BOOLEAN
TerminateBackup(volume_backup_inf* V, BOOLEAN Succeeded) {
    BOOLEAN Return = FALSE;
    V->VSSPTR.Release();
    
    if (!V->FullBackupExists) {
        printf("C_API: Fulbackup completed\n");
        //Termination of fullbackup
        //We should save records to file
        std::wstring FN = GenerateFBMetadataFileName(V->Letter);
        HANDLE MFile = CreateFileW(FN.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
        if (MFile == INVALID_HANDLE_VALUE) {
            printf("Can't create file for fullbackup metadata file, %S\n", FN.c_str());
            DisplayError(GetLastError());
        }
        else {
            DWORD Size = V->Stream.Records.Count * sizeof(nar_record);
            DWORD BytesWritten = 0;
            BOOL R = WriteFile(MFile, V->Stream.Records.Data, Size, &BytesWritten, 0);
            if (!SUCCEEDED(R) || Size != BytesWritten) {
                printf("Can't write metadata to file\n");
                printf("Written %d bytes, total of %d\n", BytesWritten, Size);
                DisplayError(GetLastError());
            }
            else {
                printf("C_API : Fullbackup exists set to true\n");
                Return = TRUE;
                V->FullBackupExists = TRUE;
            }
            
        }
        CLEANHANDLE(MFile);
    }
    else {
        //Termination of diff-inc backup
        
        // NOTE(Batuhan): Melik-Kadir abi ile konuş, belki en güvenli yöntem
        // eski dosyaya bir şey olmasın diye temp dosya oluşturup girdilerin hepsini oraya
        // yazıp, sonra güncel bir isimle değiştirmektir
        printf("## Updating metadata\n");
        if (V->BT == BackupType::Inc) {
            if (SetFilePointer(V->LogHandle, 0, 0, FILE_BEGIN) == 0) {
                
                DWORD BytesWritten = 0;
                DWORD BytesToWrite = V->Stream.Records.Count * sizeof(nar_record);
                if (WriteFile(V->LogHandle, V->Stream.Records.Data, BytesToWrite, &BytesWritten, 0) && BytesWritten == BytesToWrite) {
                    
                    if (!SetEndOfFile(V->LogHandle)) {
                        printf("Can't set end of file\n");
                    }
                    printf("Set end of file\n");
                    
                }
                else {
                    printf("Can't write to metadata file\n");
                    DisplayError(GetLastError());
                }
                
            }
            else {
                printf("Can't set file pointer\n");
                DisplayError(GetLastError());
            }
        }
        
        if (Succeeded) {
            /*
           Since merge algorithm may have change size of the record buffer,
              we should overwrite and truncate it
           */
            
            V->CurrentLogIndex++;
            printf("Initializing new log file\n");
            if (InitNewLogFile(V)) {
                Return = TRUE;
            }
            else {
                printf("Couldn't create metadata file, this is fatal error\n");
                DisplayError(GetLastError());
            }
            
        }
        
        V->FilterFlags.FlushToFile = TRUE;
        V->FilterFlags.SaveToFile = TRUE;
        
    }
    
    CLEANHANDLE(V->Stream.Handle);
    CLEANMEMORY(V->Stream.Records.Data);
    V->Stream.Records.Count = 0;
    V->Stream.RecIndex = 0;
    V->Stream.ClusterIndex = 0;
    
    
    
    // TODO(Batuhan): Clean VSSPTR
    return Return;
}

BOOLEAN
SetupStream(PLOG_CONTEXT C, wchar_t L, StreamInf* SI) {
    printf("Entered setup stream\n");
    BOOLEAN Return = FALSE;
    int ID = 0;//GetVolumeID(C, L);
    printf("Count %d\n", C->Volumes.Count);
    
    if (ID < 0) {
        printf("Couldn't find volume %S in list\n", L);
        return FALSE;
    }
    
    printf("ID is %d\n", ID);
    volume_backup_inf* VolInf = &C->Volumes.Data[ID];
    
    printf("VolInf assigned\n");
    data_array<nar_record> MFTLCN = GetMFTLCN((char)VolInf->Letter);
    
    if (SetupStreamHandle(VolInf)) {
        
        if (!VolInf->FullBackupExists) {
            
            //Fullbackup stream
            if (SetFullRecords(VolInf)) {
                Return = TRUE;
            }
            else {
                printf("SetFullRecords failed!\n");
            }
            
            if (Return == TRUE) {
                SI->ClusterCount = 0;
                SI->ClusterSize = VolInf->ClusterSize;
                SI->FileName = GenerateFBFileName(VolInf->Letter);
                SI->MetadataFileName = GenerateFBMetadataFileName(VolInf->Letter);
                for (int i = 0; i < VolInf->Stream.Records.Count; i++) {
                    SI->ClusterCount += VolInf->Stream.Records.Data[i].Len;
                }
            }
            
        }
        else {
            //Incremental or diff stream
            BackupType T = VolInf->BT;
            
            if (T == BackupType::Diff) {
                if (SetDiffRecords(VolInf)) {
                    Return = TRUE;
                }
                
            }
            else if (T == BackupType::Inc) {
                printf("Set inc records gonna start\n");
                if (SetIncRecords(VolInf)) {
                    Return = TRUE;
                }
                
            }
            
            if (Return == TRUE) {
                if (SaveMFT(VolInf, VolInf->Stream.Handle, &MFTLCN)) {
                    printf("MFT saved to disk\n");
                }
                else {
                    printf("Failed to save mft\n");
                    Return = FALSE;
                }
            }
            else {
                printf("SetRecords failed\n");
            }
            
            if (SI != NULL) {
                
                SI->ClusterCount = 0;
                SI->ClusterSize = VolInf->ClusterSize;
                SI->FileName = GenerateDBFileName(VolInf->Letter, VolInf->CurrentLogIndex);
                SI->MetadataFileName = GenerateDBMetadataFileName(VolInf->Letter, VolInf->CurrentLogIndex);
                for (int i = 0; i < VolInf->Stream.Records.Count; i++) {
                    SI->ClusterCount += VolInf->Stream.Records.Data[i].Len;
                }
                
            }
            
        }
        
    }
    else {
        printf("SetupStreamHandle failed\n");
    }
    
    return Return;
}

BOOLEAN
SetFullRecords(volume_backup_inf* V) {
    
    BOOLEAN Return = FALSE;
    UINT* ClusterIndices = 0;
    
    STARTING_LCN_INPUT_BUFFER StartingLCN;
    StartingLCN.StartingLcn.QuadPart = 0;
    ULONGLONG MaxClusterCount = 0;
    DWORD BufferSize = 1024 * 1024 * 64; // 64megabytes
    
    VOLUME_BITMAP_BUFFER* Bitmap = (VOLUME_BITMAP_BUFFER*)malloc(BufferSize);
    if (Bitmap != NULL) {
        HRESULT R = DeviceIoControl(V->Stream.Handle, FSCTL_GET_VOLUME_BITMAP, &StartingLCN,
                                    sizeof(StartingLCN), Bitmap, BufferSize, 0, 0);
        if (SUCCEEDED(R))
        {
            
            MaxClusterCount = Bitmap->BitmapSize.QuadPart;
            DWORD ClustersRead = 0;
            UCHAR* BitmapIndex = Bitmap->Buffer;
            UCHAR BitmapMask = 1;
            UINT32 CurrentIndex = 0;
            UINT32 LastActiveCluster = 0;
            
            V->Stream.Records.Data = (nar_record*)malloc(sizeof(nar_record));
            V->Stream.Records.Data[0] = { 0,1 };// NOTE(Batuhan): first cluster will always be backed up
            
            V->Stream.ClusterIndex = 0;
            V->Stream.RecIndex = 0;
            V->Stream.Records.Count = 1;
            
            /*
           NOTE(Batuhan): Since first cluster added to list via assumption, start iterating from second one
               */
            /*
         NOTE(Batuhan): Aşağıdaki algoritma çalışmazsa, WINAPI'dan aktif
         cluster sayısını al, uzunluğu o kadar olan nar_record listesi oluştur.
         Her cluster için ayrı record girdikten sonra merge_regions ile listeyi daralt
         */
            ClustersRead++;
            BitmapMask <<= 1;
            
            while (ClustersRead < MaxClusterCount) {
                if ((*BitmapIndex & BitmapMask) == BitmapMask) {
                    
                    if (LastActiveCluster == ClustersRead - 1) {
                        V->Stream.Records.Data[V->Stream.Records.Count - 1].Len++;
                    }
                    else {
                        V->Stream.Records.Insert({ ClustersRead,1 });
                    }
                    LastActiveCluster = ClustersRead;
                    
                }
                BitmapMask <<= 1;
                if (BitmapMask == 0) {
                    BitmapMask = 1;
                    BitmapIndex++;
                }
                ClustersRead++;
            }
            
            Return = TRUE;
            
            CLEANMEMORY(Bitmap);
            
        }
        else {
            // NOTE(Batuhan): DeviceIOControl failed
            printf("Get volume bitmap failed [DeviceIoControl].\n");
            printf("Result was -> %d\n", R);
            DisplayError(GetLastError());
        }
        
    }
    else {
        printf("Couldn't allocate memory for bitmap buffer!\n");
    }
    
    
    return Return;
}

BOOLEAN
SetIncRecords(volume_backup_inf* VolInf) {
    
    BOOLEAN Result = FALSE;
    DWORD BytesOperated = 0;
    
    DWORD FileSize = VolInf->IncRecordCount * sizeof(nar_record);
    
    VolInf->Stream.Records.Data = (nar_record*)malloc(FileSize);
    VolInf->Stream.Records.Count = FileSize / sizeof(nar_record);
    memset(VolInf->Stream.Records.Data, 0, FileSize);
    printf("CURRENT LOG INDEX -> %i\n", VolInf->CurrentLogIndex);
    if (SetFilePointer(VolInf->LogHandle, 0, 0, FILE_BEGIN) == 0) {
        Result = ReadFile(VolInf->LogHandle, VolInf->Stream.Records.Data, FileSize, &BytesOperated, 0);
        if (!SUCCEEDED(Result) || FileSize != BytesOperated) {
            printf("Cant read difmetadata \n");
            printf("Filesize-> %d, BytesOperated -> %d", FileSize, BytesOperated);
            printf("N of records => %d", FileSize / sizeof(nar_record));
            DisplayError(GetLastError());
        }
        else {
            qsort(VolInf->Stream.Records.Data, VolInf->Stream.Records.Count, sizeof(nar_record), CompareNarRecords);
            MergeRegions(&VolInf->Stream.Records);
            //for (unsigned int i = 0; i < VolInf->Stream.Records.Count; i++) printf("REG %u : %u\t%u\n",i, VolInf->Stream.Records.Data[i].StartPos, VolInf->Stream.Records.Data[i].Len);
        }
        
        Result = TRUE;
    }
    else {
        printf("Failed to set file pointer to beginning of the file\n");
        DisplayError(GetLastError());
        //TODO failed
    }
    
    
    return Result;
}




BOOLEAN
SetDiffRecords(volume_backup_inf *V) {
  printf("Entered SetDiffRecords\n");
  BOOLEAN Result = FALSE;

  HANDLE* F = (HANDLE*)malloc(sizeof(HANDLE) * (V->CurrentLogIndex));
  memset(F, 0, sizeof(HANDLE) * (V->CurrentLogIndex + 1));

  DWORD* FS = (DWORD*)malloc(sizeof(DWORD) * (V->CurrentLogIndex + 1));
  memset(FS, 0, sizeof(DWORD) * (V->CurrentLogIndex + 1));

  DWORD TotalFileSize = 0;

  for (int i = 0; i < V->CurrentLogIndex; i++) {
    std::wstring FName = GenerateDBMetadataFileName(V->Letter, i).c_str();

    F[i] = CreateFileW(FName.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

    if (F[i] == INVALID_HANDLE_VALUE) {
      printf("Can't open file %S\n", FName.c_str());
      DisplayError(GetLastError());
      return FALSE;
    }

    DWORD Temp = GetFileSize(F[i], 0);

    printf("Opened file %S, size %i\n", FName.c_str(), Temp);
    Assert(Temp > 0, "File size can't be lower than zero\n");

    TotalFileSize += Temp;
    FS[i] = Temp;
  }

  printf("IncRecordCount -> %i\n", V->IncRecordCount);
  F[V->CurrentLogIndex] = V->LogHandle;
  FS[V->CurrentLogIndex] = V->IncRecordCount * sizeof(nar_record);
  TotalFileSize += (V->IncRecordCount * sizeof(nar_record));

  printf("Totalfilesize %i\tCurrentlogindex %i\n", TotalFileSize, V->CurrentLogIndex);

  if (V->Stream.Records.Data != NULL) {
    // TODO(Batuhan): Log error
  }
  V->Stream.Records.Data = (nar_record*)malloc(TotalFileSize);
  V->Stream.Records.Count = 0;

  DWORD LogRead = 0;
  for (int i = 0; i <= V->CurrentLogIndex; i++) {
    
    std::wstring FN = GenerateDBMetadataFileName(V->Letter, i);

    DWORD BytesRead = 0;
    printf("Before setfileopinter\n");
    if (SetFilePointer(F[i], 0, 0, FILE_BEGIN) == 0) {

      if (LogRead > TotalFileSize / sizeof(nar_record)) {
        printf("Possibly access violation, LogRead -> %i , TotalFileSize -> %i\n", LogRead, TotalFileSize);
      }

      if (ReadFile(F[i], &V->Stream.Records.Data[LogRead], FS[i], &BytesRead, 0) && BytesRead == FS[i]) {
        printf("Advancing iterator by %i\n", FS[i] / sizeof(nar_record));
        V->Stream.Records.Count += FS[i] / sizeof(nar_record);
        LogRead += FS[i] / sizeof(nar_record);

      }
      else {

        printf("Unable to read log file\n");
        printf("File name %S\t FileSize %d\t BytesRead %d\n", FN.c_str(), FS[i], BytesRead);
        DisplayError(GetLastError());
        goto TERMINATE;
        // TODO(Batuhan): error

      }

    }
    else {
      printf("Unable to set file pointer to zero -> %S\n", FN.c_str());
      DisplayError(GetLastError());
      goto TERMINATE;
      // TODO(Batuhan): error
    }

  }

  Result = TRUE;
TERMINATE:

  for (int i = 0; i < V->CurrentLogIndex; i++) {
    CLEANHANDLE(F[i]);
  }

  CLEANMEMORY(FS);
  CLEANMEMORY(F);

  if (Result == TRUE) {
    qsort(V->Stream.Records.Data, V->Stream.Records.Count, sizeof(nar_record), CompareNarRecords);
    MergeRegions(&V->Stream.Records);
  }

  //TODO
  return Result;
}


BOOLEAN
SetupStreamHandle(volume_backup_inf* VolInf) {
    
    if (VolInf == NULL) {
        printf("volume_backup_inf is null\n");
        return FALSE;
    }
    if (VolInf->VSSPTR != NULL) {
        printf("VSS pointer isn't null\n");
        return FALSE;
    }
    
    BOOLEAN ErrorOccured = FALSE;
    HRESULT Result = 0;
    
    if (VolInf->FullBackupExists) {
        if (!DetachVolume(VolInf)) {
            printf("Detach volume failed\n");
            return FALSE;
        }
        printf("Volume detached !\n");
    }
    
    
    VolInf->FilterFlags.FlushToFile = FALSE;
    VolInf->FilterFlags.SaveToFile = !VolInf->FullBackupExists;
    VolInf->FilterFlags.IsActive = TRUE;
    
    
    WCHAR Temp[] = L"!:\\";
    Temp[0] = VolInf->Letter;
    wchar_t* ShadowPath = GetShadowPath(Temp, VolInf->VSSPTR);
    
    if (!VolInf->FullBackupExists) {
        if (!InitNewLogFile(VolInf)) {
            printf("Can't init new log file\n");
            return FALSE;
        }
    }
    
    if (!AttachVolume(VolInf)) {
        printf("Cant attach volume\n");
        return FALSE;
    }
    printf("Filtering started again!\n");
    
    VolInf->Stream.Handle = CreateFileW(ShadowPath,
                                        GENERIC_READ, 0, NULL,
                                        OPEN_EXISTING, NULL, NULL);
    
    
    if (VolInf->Stream.Handle == INVALID_HANDLE_VALUE) {
        printf("Can not open shadow path %S..\n", ShadowPath);
        DisplayError(GetLastError());
        return FALSE;
    }
    
    return TRUE;
}

#if 0
BOOLEAN
FullBackupVolume(PLOG_CONTEXT Context, UINT VolInfIndex) {
    
    BOOLEAN Return = FALSE;
    HRESULT Result = 0;
    VOLUME_BITMAP_BUFFER* Bitmap = 0;
    UINT* ClusterIndices = 0;
    
    HANDLE DiskHandle = 0;
    HANDLE OutputHandle = 0;
    HANDLE MetadataFileHandle = 0;
    std::wstring ShadowPath;
    // TODO check if VSS requests could be filtered with driver with some kind of flag
    printf("Waiting VSS!\n");
    CComPtr<IVssBackupComponents> ptr;
    std::wstring fbmetadatafname;
    std::wstring FBFname = L"";
    
    ShadowPath = GetShadowPath(ShadowPath.c_str(), ptr);
    
    
    
    if (!AttachVolume(Context, VolInfIndex)) {
        printf("Attach volume failed\n");
        goto F_Cleanup;
    }
    Context->Volumes.Data[VolInfIndex].SaveToFile = TRUE;
    Context->Volumes.Data[VolInfIndex].FlushToFile = FALSE;;
    
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
    UINT32 UsedClusterCount = 0;
    UCHAR BitmapMask = 1;
    DWORD ToMove = BufferSize;
    DWORD ClusterSize = Context->Volumes.Data[VolInfIndex].ClusterSize;
    
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
    
    printf("Cluster size %d\n", Context->Volumes.Data[VolInfIndex].ClusterSize);
    printf("Number of clusters to copy -> %d\t%d\n", UsedClusterCount, CurrentIndex);
    printf("MaxCluster count -> %I64d [%d]\n", MaxClusterCount, MaxClusterCount);
    
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
        printf("Could not create file %S\n", FBFname.c_str());
        Result = GetLastError();
        DisplayError(Result);
        goto F_Cleanup;
    }
    
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
        // TODO: Use CopyData function
        CopyData(DiskHandle, OutputHandle, Context->Volumes.Data[VolInfIndex].ClusterSize);
        
    }
    printf("Disk stated saved to file %S\n", FBFname.c_str());
    
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
    
    Return = TRUE;
    F_Cleanup:
    CLEANMEMORY(Bitmap);
    CLEANMEMORY(ClusterIndices);
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
    HANDLE FBackupMDFile = 0; //metadata for full backup
    HANDLE DBackupMDFile = 0; //metadata for diff backup
    HANDLE VolumeToRestore = 0;
    DWORD BufferSize = 1024 * 1024 * 128; //128MB
    DWORD ClusterSize = RestoreInf->ClusterSize;
    DWORD BytesOperated = 0;
    DWORD FileSize = 0;
    UINT ID = RestoreInf->DiffVersion;
    
    data_array<nar_record>* Metadatas = 0;
    
    //This is going to store full backup metadata too, so ID + 1 should be used since
    //ID is identifier for diff backup, since it is zero based, another +1 is needed
    Metadatas = (data_array<nar_record>*)malloc(sizeof(data_array<nar_record>) * (ID + 2));
    void* Buffer = malloc(BufferSize);
    
    std::wstring dbfname = L"";
    std::wstring fbfname = L"";
    std::wstring fbmdfname = L"";
    std::wstring DMDFileName = L"";
    
    if (Metadatas == NULL) {
        printf("Can't allocate memory for metadata buffers\n");
        goto R_Cleanup;
    }
    
    if (Buffer == NULL) {
        printf("Can't allocate buffer\n");
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
        printf("Can't open fullbackup metadata file for restore operation %S\n", fbmdfname.c_str());
        DisplayError(GetLastError());
        goto R_Cleanup;
    }
    
    Metadatas[0] = ReadFBMetadata(FBackupMDFile);
    for (int i = 0; i < Metadatas[0].Count; i++) {
        printf("%d\t%d\n", Metadatas[0].Data[i].StartPos, Metadatas[0].Data[i].Len);
    }
    printf("FBMetadata read\n");
    
    for (int i = 1; i < ID + 2; i++) {
        printf("DB metadata will be read\n");
        DMDFileName = GenerateDBMetadataFileName(RestoreInf->SrcLetter, i - 1);
        DBackupMDFile = CreateFileW(
                                    DMDFileName.c_str(),
                                    GENERIC_READ,
                                    FILE_SHARE_READ,
                                    0, OPEN_EXISTING, 0, 0
                                    );
        
        if (DBackupMDFile == INVALID_HANDLE_VALUE) {
            printf("Can't open diffbackup metadata file for restore operation %S\n", DMDFileName.c_str());
            DisplayError(GetLastError());
            goto R_Cleanup;
        }
        Metadatas[i] = ReadMetadata(DBackupMDFile);
        
        CLEANHANDLE(DBackupMDFile);
    }
    
    
    CLEANHANDLE(FBackupMDFile);
    
    
    WCHAR VTRNameTemp[] = L"\\\\.\\ :", ;
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
    
    {
        
        printf("Minimal copy operation started\n");
        
        //+1 for including fullbackup, +1 for ID is already 0 based
        for (UINT i = 0; i < ID + 2; i++) {
            
            HANDLE SrcHandle = INVALID_HANDLE_VALUE;
            
            if (i != 0) {
                std::wstring fbn = GenerateDBFileName(RestoreInf->SrcLetter, i - 1);
                SrcHandle = CreateFileW(
                                        fbn.c_str(),
                                        GENERIC_READ,
                                        FILE_SHARE_READ,
                                        0, OPEN_EXISTING, 0, 0
                                        );
                if (SrcHandle == INVALID_HANDLE_VALUE) {
                    printf("Can't open diffbackup file for restore operation %S\n", dbfname.c_str());
                    DisplayError(GetLastError());
                    goto R_Cleanup;
                }
            }
            else {
                std::wstring tmp = GenerateFBFileName(RestoreInf->SrcLetter);
                SrcHandle = CreateFileW(
                                        tmp.c_str(),
                                        GENERIC_READ,
                                        FILE_SHARE_READ,
                                        0, OPEN_EXISTING, 0, 0
                                        );
                if (SrcHandle == INVALID_HANDLE_VALUE) {
                    printf("Can't open fullbackup file for restore operation %S\n", dbfname.c_str());
                    DisplayError(GetLastError());
                    goto R_Cleanup;
                }
                
            }
            
            for (int j = 0; j < Metadatas[i].Count; j++) {
                LARGE_INTEGER MoveTo = { 0 };
                LARGE_INTEGER NewFilePointer = { 0 };
                
                MoveTo.QuadPart = Metadatas[i].Data[j].StartPos * RestoreInf->ClusterSize;
                SetFilePointerEx(VolumeToRestore, MoveTo, &NewFilePointer, FILE_BEGIN);
                if (MoveTo.QuadPart != NewFilePointer.QuadPart) {
                    printf("Cant set file pointer to -> %I64d\n", MoveTo.QuadPart);
                    DisplayError(GetLastError());
                    goto R_Cleanup;
                    //TODO
                }
                
                ULONGLONG CopySize = Metadatas[i].Data[j].Len * RestoreInf->ClusterSize;
                if (!CopyData(SrcHandle, VolumeToRestore, CopySize)) {
                    printf("Copy operation failed\n");
                    printf("Tried to copy %I64d\n", CopySize);
                    DisplayError(GetLastError());
                    goto R_Cleanup;
                }
                
            }
            
            
            CLEANHANDLE(SrcHandle);
            
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
    CLEANHANDLE(FBackupMDFile);
    CLEANHANDLE(DBackupMDFile);
    CLEANHANDLE(VolumeToRestore);
    
    CLEANMEMORY(Buffer);
    
    for (int i = 0; i < ID + 2; i++) {
        CLEANMEMORY(Metadatas[i].Data);
    }
    CLEANMEMORY(Metadatas);
    
    printf("Cleaned!\n");
    return Return;
}
#endif


//
//  Main uses a loop which has an assignment in the while
//  conditional statement. Suppress the compiler's warning.
//



BOOLEAN
SetupVSS() {
    BOOLEAN Return = TRUE;
    HRESULT hResult = 0;
    
    hResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (!SUCCEEDED(hResult)) {
        printf("Failed CoInitialize function %d\n", hResult);
        DisplayError(GetLastError());
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
        DisplayError(GetLastError());
        Return = FALSE;
    }
    
    return Return;
}

BOOLEAN
ConnectDriver(PLOG_CONTEXT Ctx) {
    BOOLEAN Result = FALSE;
    DWORD PID = GetCurrentProcessId();
    HRESULT hResult = FilterConnectCommunicationPort(MINISPY_PORT_NAME,
                                                     0,
                                                     &PID, sizeof(PID),
                                                     NULL, &Ctx->Port);
    if (!IS_ERROR(hResult)) {
        
        if (NarCreateThreadCom(Ctx)) {
            Result = TRUE;
        }
        else {
            CloseHandle(Ctx->Port);
            printf("Can't create thread to communicate with driver\n");
            // TODO(Batuhan):
        }
        
    }
    else {
        printf("Could not connect to filter: 0x%08x\n", hResult);
        printf("Program PID is %d\n", PID);
        DisplayError(hResult);
    }
    
    return Result;
}


/*
// TODO(Batuhan): negative error values
Errors:
- Can't open volume
- Can't lock volume

*/
BOOLEAN
OfflineRestore(restore_inf* Inf) {
    BOOLEAN Result = FALSE;
    HRESULT hResult = 0;
    HANDLE VolumeToRestore = INVALID_HANDLE_VALUE;
    HANDLE FBMetadata = INVALID_HANDLE_VALUE;
    
    //int TempLetter  = (int)Inf->TargetLetter;
    //
    //DWORD Drives = GetLogicalDrives();
    //if ((Drives & (1 << (TempLetter - 65)))){
    //    // TODO(Batuhan): exist
    //}
    //else{
    //    // TODO(Batuhan): volume does not exist
    //}
    
    // TODO(Batuhan): case target volume doesnt exist
    // TODO(Batuhan): not enough space
    // TODO(Batuhan): can't lock volume
    
    /*
    NOTE(Batuhan): Volume handle and stream structure will be available for Offline##Restore functions,
    since there are another steps required to complete restore operation,
    diff and incremental backup functions must take care of rest of the operation.
     */
    
    std::wstring a = GenerateFBFileName(Inf->SrcLetter).c_str();
    
    HANDLE FullFile = CreateFileW(
                                  a.c_str(),
                                  GENERIC_READ,
                                  0, 0, OPEN_EXISTING, 0, 0);
    
    if (FullFile == INVALID_HANDLE_VALUE) {
        printf("Can't open fullbackup file %S\n", a.c_str());
        DisplayError(GetLastError());
        goto TERMINATE;
    }
    //printf("File %S opened\n", a.c_str());
    
    wchar_t Temp[] = L"\\\\.\\ :";
    Temp[lstrlenW(Temp) - 2] = Inf->TargetLetter;
    VolumeToRestore = CreateFileW(Temp,
                                  GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ,
                                  0, OPEN_EXISTING, 0, 0
                                  );
    
    if (VolumeToRestore == INVALID_HANDLE_VALUE) {
        printf("Unable to open volume to restore, %S\n", Temp);
        DisplayError(GetLastError());
        goto TERMINATE;
    }
    
    hResult = DeviceIoControl(VolumeToRestore,
                              FSCTL_LOCK_VOLUME,
                              0, 0, 0, 0, 0, 0);
    
    if (!SUCCEEDED(hResult)) {
        printf("Can't lock volume %S\n", Temp);
        DisplayError(GetLastError());
        goto TERMINATE;
    }
    printf("Volume locked for restore\n");
    
    data_array<nar_record> FullRecords;
    
    // NOTE(Batuhan): Since every backup type has to restore fullbackup first, do that operation here, then call spesific restore function
    
    FBMetadata = CreateFile(GenerateFBMetadataFileName(Inf->SrcLetter).c_str(),
                            GENERIC_READ,
                            FILE_SHARE_READ,
                            0, OPEN_EXISTING, 0, 0);
    
    if (FBMetadata != INVALID_HANDLE_VALUE) {
        UINT32 FileSize = GetFileSize(FBMetadata, 0);
        DWORD BytesRead = 0;
        printf("Metadata file size %d\n\n", FileSize);
        FullRecords.Data = (nar_record*)malloc(FileSize);
        FullRecords.Count = FileSize / sizeof(nar_record);
        if (FullRecords.Data == NULL) {
            printf("FATAL: Can't allocate memory !\n");
            goto TERMINATE;
        }
        
        if (ReadFile(FBMetadata, FullRecords.Data, FileSize, &BytesRead, 0) && BytesRead == FileSize) {
            printf("Metadata read, record count -> %d\n", FullRecords.Count);
            
            for (int i = 0; i < FullRecords.Count; i++) {
                
                LARGE_INTEGER MoveTo;
                LARGE_INTEGER NewFilePointer = { 0 };
                
                MoveTo.QuadPart = (ULONGLONG)Inf->ClusterSize * (ULONGLONG)FullRecords.Data[i].StartPos;
                BOOLEAN R = SetFilePointerEx(VolumeToRestore, MoveTo, &NewFilePointer, FILE_BEGIN);
                
                if (R && NewFilePointer.QuadPart == MoveTo.QuadPart) {
                    ULONGLONG CopySize = (ULONGLONG)FullRecords.Data[i].Len * (ULONGLONG)Inf->ClusterSize;
                    if (!CopyData(FullFile, VolumeToRestore, CopySize)) {
                        printf("Unable to copy fullbackup file to volume\n");
                        printf("Tried to copy %I64d, to volume offset %I64d\n", CopySize, MoveTo.QuadPart);
                        goto TERMINATE;
                    }
                    
                }
                else {
                    printf("Can't set file pointer to %I64d, instead set to %I64d\n", MoveTo.QuadPart, NewFilePointer.QuadPart);
                    DisplayError(GetLastError());
                    goto TERMINATE;
                    // TODO(Batuhan): Error
                }
                
                //for loop end
            }
            
            printf("Full restore completed\n");
            if (!Inf->ToFull) {
                // TODO(Batuhan): Close file handles ?
                
                if (Inf->Type == BackupType::Diff) {
                    printf("Offline diff restore will be done\n");
                    if (!OfflineDiffRestore(Inf, VolumeToRestore)) {
                        printf("Can't restore to diff\n");
                        goto TERMINATE;
                    }
                    else {
                        printf("Restored to diff\n");
                    }
                    
                }
                else if (Inf->Type == BackupType::Inc) {
                    printf("Offline inc restore will be done\n");
                    if (!OfflineIncRestore(Inf, VolumeToRestore)) {
                        printf("Can't restore to incremental\n");
                        goto TERMINATE;
                    }
                    else {
                        printf("Restored to incremental\n");
                    }
                    
                }
                
            }
            
        }
        else {
            printf("Unable to read fullbackup metadat file\n"
                   "Metadata size -> %d, Read -> %d\n", FileSize, BytesRead);
            DisplayError(GetLastError());
        }
        
        
    }
    else {
        printf("Can't open fullbackup metadata file\n");
        DisplayError(GetLastError());
        goto TERMINATE;
    }
    
    
    Result = TRUE;
    TERMINATE:
    if (Result == FALSE) {
        printf("Error occured\n");
    }
    
    CLEANMEMORY(FullRecords.Data);
    CLEANHANDLE(VolumeToRestore);
    CLEANHANDLE(FullFile);
    CLEANHANDLE(FBMetadata);
    
    return Result;
}


BOOLEAN
OfflineIncRestore(restore_inf* R, HANDLE V) {
    BOOLEAN Result = FALSE;
    data_array<nar_record> Records = { 0,0 };
    printf("There are %d versions to restore incrementaly\n", R->Version);
    
    for (int i = 0; i <= R->Version; i++) {
        std::wstring FN = GenerateDBMetadataFileName(R->SrcLetter, i);
        
        HANDLE MetadataFile = CreateFileW(FN.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
        
        if (MetadataFile != INVALID_HANDLE_VALUE) {
            //printf("File %S opened\n", FN.c_str());
            
            UINT32 FileSize = GetFileSize(MetadataFile, 0);
            
            Records.Data = (nar_record*)malloc(FileSize);
            Records.Count = FileSize / sizeof(nar_record);
            
            if (Records.Data == NULL) {
                printf("FATAL : Can't allocate memory \n");
                CLEANHANDLE(MetadataFile);
                goto TERMINATE;
            }
            DWORD BytesRead = 0;
            if (!ReadFile(MetadataFile, Records.Data, FileSize, &BytesRead, 0) || BytesRead != FileSize) {
                printf("Can't read metadata\n");
                DisplayError(GetLastError());
                CLEANHANDLE(MetadataFile);
                CLEANMEMORY(Records.Data);
                goto TERMINATE;
            }
            
            
            FN = GenerateDBFileName(R->SrcLetter, i);
            HANDLE BackupFile = CreateFileW(FN.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
            
            if (BackupFile != INVALID_HANDLE_VALUE) {
                //printf("File %S opened\n", FN.c_str());
                
                for (int j = 0; j < Records.Count; j++) {
                    
                    LARGE_INTEGER MoveTo = { 0 };
                    LARGE_INTEGER NewFilePointer = { 0 };
                    MoveTo.QuadPart = (ULONGLONG)Records.Data[j].StartPos * (ULONGLONG)R->ClusterSize;
                    
                    BOOL Temp = SetFilePointerEx(V, MoveTo, &NewFilePointer, FILE_BEGIN);
                    
                    if (Temp && NewFilePointer.QuadPart == MoveTo.QuadPart) {
                        ULONGLONG CopySize = (ULONGLONG)Records.Data[j].Len * (ULONGLONG)R->ClusterSize;
                        
                        if (!CopyData(BackupFile, V, CopySize)) {
                            printf("Unable to copy data from backup file to volume\n"
                                   "Tried to copy %I64d to volume offset %I64d\n", CopySize, MoveTo.QuadPart);
                            goto TERMINATE;
                        }
                        
                    }
                    else {
                        printf("Cant set file pointer to %I64d, instead set to %I64d\n", MoveTo.QuadPart, NewFilePointer.QuadPart);
                        DisplayError(GetLastError());
                        goto TERMINATE;
                        // TODO(Batuhan): error
                    }
                    
                }
                
            }
            else {
                printf("Cant open backup file %S\n");
                // TODO(Batuhan): file name
                DisplayError(GetLastError());
                goto TERMINATE;
            }
            CLEANHANDLE(BackupFile);
            
        }
        else {
            printf("Cant open metadata file %S\n", FN.c_str());
            DisplayError(GetLastError());
            goto TERMINATE;
        }
        
        CLEANHANDLE(MetadataFile);
        
    }
    
    printf("Incremental restore done, MFT will be restored manually\n");
    if (RestoreMFT(R, V)) {
        Result = TRUE;
    }
    else {
        printf("Can't restore MFT\n");
    }
    TERMINATE:
    CLEANMEMORY(Records.Data);
    
    return Result;
}

BOOLEAN
OfflineDiffRestore(restore_inf* R, HANDLE V) {

  BOOLEAN Result = 0;
  data_array<nar_record> Records = { 0,0 };

  HANDLE* F = (HANDLE*)malloc(sizeof(HANDLE) * (R->Version + 1));
  memset(F, 0, sizeof(HANDLE) * (R->Version + 1));

  DWORD* FS = (DWORD*)malloc(sizeof(DWORD) * (R->Version + 1));
  memset(FS, 0, sizeof(DWORD) * (R->Version + 1));

  ULONGLONG TotalFileSize = 0;
  std::wstring FN = L"";

  for (int i = 0; i <= R->Version; i++) {
    FN = GenerateDBMetadataFileName(R->SrcLetter, i);
    F[i] = CreateFileW(FN.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if (F[i] == INVALID_HANDLE_VALUE) {
      printf("File %S can't be opened \n", FN.c_str());
      DisplayError(GetLastError());
      return FALSE;
    }
    
    FS[i] = GetFileSize(F[i], 0);
    TotalFileSize += FS[i];
    printf("File %S has %i bytes\n", FN.c_str(), FS[i]);
  }
  
  ULONGLONG LogRead = 0;
  printf("Total record count -> %i\n", TotalFileSize / sizeof(nar_record));
  Records.Data = (nar_record*)malloc(TotalFileSize);
  Records.Count = TotalFileSize / sizeof(nar_record);
  
  if (Records.Data == NULL) {
    printf("FATAL : Can't allocate memory\n");
    goto TERMINATE;
  }

  //NOTE reading all metadatas
  for (int i = 0; i <= R->Version; i++) {

    FN = GenerateDBMetadataFileName(R->SrcLetter, i);

    DWORD BytesRead = 0;
    if (SetFilePointer(F[i], 0, 0, FILE_BEGIN) == 0) {

      if (LogRead > TotalFileSize / sizeof(nar_record)) {
        printf("Possibly access violation, LogRead -> %i , TotalFileSize -> %i\n", LogRead, TotalFileSize);
      }

      if (ReadFile(F[i], &Records.Data[LogRead], FS[i], &BytesRead, 0) && BytesRead == FS[i]) {
        printf("Advancing iterator by %i\n", FS[i] / sizeof(nar_record));
        LogRead += FS[i] / sizeof(nar_record);
      }
      else {

        printf("Unable to read log file\n");
        printf("File name %S\t FileSize %d\t BytesRead %d\n", FN.c_str(), FS[i], BytesRead);
        DisplayError(GetLastError());
        goto TERMINATE;
        // TODO(Batuhan): error

      }

    }
    else {
      printf("Unable to set file pointer to zero -> %S\n", FN.c_str());
      DisplayError(GetLastError());
      goto TERMINATE;
      // TODO(Batuhan): error
    }

  }
  
  printf("Before qsort and mergeregions count -> %i\n",Records.Count);
  qsort(Records.Data, Records.Count, sizeof(nar_record), CompareNarRecords);
  printf("QSort succeeded\n");
  MergeRegions(&Records);
  
  if (Records.Data == NULL) {
    printf("CANT MERGE REGIONS\n");
    exit(0);
  }

  printf("After mergeregions -> %i \n", Records.Count);

  {      
    FN = GenerateDBFileName(R->SrcLetter, R->Version);
  
    HANDLE BackupFile = CreateFileW(FN.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

    if (BackupFile != INVALID_HANDLE_VALUE) {
      printf("There are %i records to operate\n", Records.Count);
      printf("%S file opened to restore\n", FN.c_str());

      ULONGLONG _TOTAL = 0;
      for (int i = 0; i < Records.Count; i++) {
        _TOTAL += (Records.Data[i].Len * R->ClusterSize);
      }
      
      DWORD METADATASIZE = GetFileSize(BackupFile, 0);
      if (METADATASIZE != _TOTAL) {
        printf("Metadata's given size and actual binary data size doesnt match, M= > %I64d\t B => %I64d\n", _TOTAL, METADATASIZE);
      }
      printf("Bounds checked and passed\n");

      for (int i = 0; i < Records.Count; i++) {

        LARGE_INTEGER MoveTo = { 0 };
        LARGE_INTEGER NewFilePointer = { 0 };
        MoveTo.QuadPart = (ULONGLONG)Records.Data[i].StartPos * (ULONGLONG)R->ClusterSize;

        if (SetFilePointerEx(V, MoveTo, &NewFilePointer, 0) && MoveTo.QuadPart == NewFilePointer.QuadPart) {
          ULONGLONG CopySize = (ULONGLONG)Records.Data[i].Len * (ULONGLONG)R->ClusterSize;

          if (!CopyData(BackupFile, V, CopySize)) {
            printf("Unable to copy data from backup file to volume\n"
              "Tried to copy %I64d to volume offset %I64d\n", CopySize, MoveTo.QuadPart);
            goto TERMINATE;
          }

        }

      }

    }
    else {
      printf("Can't open backup file %S\n", FN.c_str());
      DisplayError(GetLastError());
    }

  }

  if (!RestoreMFT(R, V)) {
    printf("Can't restore MFT\n");
  }
  else {
    printf("MFT restored\n");
    Result = TRUE;
  }
TERMINATE:
  
  for (int i = 0; i <= R->Version; i++) {
    CLEANHANDLE(F[i]);
  }
  CLEANMEMORY(F);
  CLEANMEMORY(FS);
  CLEANMEMORY(Records.Data);

  printf("Terminating..\n");

  return Result;

}


#if 1
int
wmain(
      int argc,
      WCHAR* argv[]
      ) {
  data_array<nar_record> R = { 0,0 };
  R.Insert({ 40,10 });
  R.Insert({ 60,6 });
  R.Insert({ 70,8 });
  R.Insert({ 90,14 });
  R.Insert({ 134,40 });
  R.Insert({ 450,10 });
  R.Insert({ 930,10 });

  qsort(R.Data, R.Count, sizeof(nar_record), CompareNarRecords);
  MergeRegions(&R);


  return 0;

    void* Buff = malloc(1024);
    memset(Buff, 0, 1024);
    InitVolumeInf((volume_backup_inf *)Buff, L'C', Diff);
    
    int thirdindex = -1;
    int count = 0;
    std::wstring LogDOSName = std::wstring(L"\\Device\\HardDiskDevice12\\files\\random.txt");
    
    for (int i = 0; LogDOSName[i] != '\0'; i++) {
        if (LogDOSName[i] == L'\\') count++;
        if (count == 3) {
            thirdindex = i;
            break;
        }
    }
    if (thirdindex == -1) {
        //TODO error log
    }
    
    LogDOSName = LogDOSName.substr(0,thirdindex);
    std::wcout << LogDOSName;
    
    
    UINT* ClusterIndices = 0;
    
    STARTING_LCN_INPUT_BUFFER StartingLCN;
    StartingLCN.StartingLcn.QuadPart = 0;
    ULONGLONG MaxClusterCount = 0;
    DWORD BufferSize = 1024 * 1024 * 64; // 64megabytes
    HANDLE Handle = CreateFileW(L"\\\\.\\E:", GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
    if (Handle == INVALID_HANDLE_VALUE) {
        printf("Can't open volume\n");
        DisplayError(GetLastError());
        return 0;
    }
    data_array<nar_record> Records = { 0 ,0 };
    
    VOLUME_BITMAP_BUFFER* Bitmap = (VOLUME_BITMAP_BUFFER*)malloc(BufferSize);
    if (Bitmap != NULL) {
        HRESULT R = DeviceIoControl(Handle, FSCTL_GET_VOLUME_BITMAP, &StartingLCN,
                                    sizeof(StartingLCN), Bitmap, BufferSize, 0, 0);
        
        if (SUCCEEDED(R))
        {
            
            MaxClusterCount = Bitmap->BitmapSize.QuadPart;
            DWORD ClustersRead = 0;
            UCHAR* BitmapIndex = Bitmap->Buffer;
            UCHAR BitmapMask = 1;
            UINT32 CurrentIndex = 0;
            UINT32 LastActiveCluster = 0;
            
            
            Records.Data = (nar_record*)malloc(sizeof(nar_record));
            Records.Data[0] = { 0,1 };// NOTE(Batuhan): first cluster will always be backed up
            Records.Count = 1;
            
            ClustersRead++;
            BitmapMask <<= 1;
            
            while (ClustersRead < MaxClusterCount) {
                if ((*BitmapIndex & BitmapMask) == BitmapMask) {
                    
                    if (LastActiveCluster == ClustersRead - 1) {
                        Records.Data[Records.Count - 1].Len++;
                    }
                    else {
                        Records.Insert({ ClustersRead,1 });
                    }
                    LastActiveCluster = ClustersRead;
                    
                }
                BitmapMask <<= 1;
                if (BitmapMask == 0) {
                    BitmapMask = 1;
                    BitmapIndex++;
                }
                ClustersRead++;
            }
        }
        
        for (int i = 0; i < Records.Count; i++) printf("Record %d : %d\t%d\n", i, Records.Data[i].StartPos, Records.Data[i].Len);
    }
    return 0;
    
    //wchar_t Temp[512];
    //QueryDosDeviceW(L"C:", Temp, 512);
    //printf("%S\n", Temp);
    
    HRESULT hResult = S_OK;
    DWORD result;
    LOG_CONTEXT context = { 0 };
    sizeof(L"\device\harddiskvolume6");
    
    context.ShutDown = NULL;
    context.Port = INVALID_HANDLE_VALUE;
    context.ShutDown = NULL;
    context.CleaningUp = FALSE;
    context.Thread = 0;
    context.Volumes = { 0,0 };
    
    //
    //  Open the port that is used to talk to
    //  MiniSpy.
    //
    
#if 0
    
    printf("Connecting to filter's port...\n");
    
    DWORD PID = GetCurrentProcessId();
    hResult = FilterConnectCommunicationPort(MINISPY_PORT_NAME,
                                             0,
                                             &PID,
                                             sizeof(PID),
                                             NULL,
                                             &context.Port);
    if (IS_ERROR(hResult)) {
        
        printf("Could not connect to filter: 0x%08x\n", hResult);
        DisplayError(hResult);
        return 0;
    }
    
    
    if (NarCreateThreadCom(&context)) {
        printf("Narcreatethreadcom started\n");
        if (AddVolumeToTrack(&context, L'C', Diff)) {
            volume_backup_inf* V = &context.Volumes.Data[0];
            InitNewLogFile(&context.Volumes.Data[0]);
            AttachVolume(V, TRUE);
            
            while (TRUE) {
                std::string a;
                std::cin >> a;
                if (a == "q") return 0;
                Sleep(50);
            }
            
        }
        else {
            printf("Cant add vol to track\n");
            return 0;
        }
    }
    else {
        printf("Cant create thread\n");
        return 0;
    }
    
    
    for (;;) {
        std::wstring Input = L"restore,E,N,1";
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
    
#endif
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
    
    if (INVALID_HANDLE_VALUE != context.Port) {
        CloseHandle(context.Port);
    }
    return 0;
}

#endif


VOID
DisplayError(
             DWORD Code
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
