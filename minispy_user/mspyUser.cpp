/*
  I do unity builds, so I dont check if declared functions in .h file, I just write it in cpp file and if any build gives me error then
  I add it to .h file.

  check if INDEX_ALLOCATION falls inside MFT cluster regions
*/


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

//#define NAR_CONV_TO_BIG_END_4BYTE(n) ((n & 0x000000ff) << 24u) | ((n & 0x0000ff00) << 8u) | ((n & 0x00ff0000) >> 8u) | ((n & 0xff000000) >> 24u)
//#define NAR_EXTRACT_3BYTE_BIG_END_FROM_LOW_END(n) NAR_CONV_TO_BIG_END_4BYTE(n) & 0x00ffffff
//#define NAR_CONV_TO_BIG_END_2BYTE(n) ((n) << 8u) | ((n) >> 8u)
//#define NAR_3BYTE_FROM4(n) (n) >> 8 // converts 4 byte little endian value to 3byte, but since there arent 3byte types in language, we keep using INT32 type. that wont be much problem since we truncated it

                                                                                                                                                              


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

#if 1 // OPTIMIZED INCREMENTAL MERGE ALORITHM

void
MergeRegions(region_chain* Chain) {
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
        //New->Index = 0;
        
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
    
    //  R->Back = 0;
    //  R->Next = 0;
}

void
PrintList(region_chain* Temp) {
    printf("\n######\n");
    while (Temp != NULL) {
        printf("%u\t%u\n", Temp->Rec.StartPos, Temp->Rec.Len);
        Temp = Temp->Next;
    }
    printf("\n######\n");
}

void
PrintListReverse(region_chain* Temp) {
    printf("###\n");
    while (Temp->Next) Temp = Temp->Next;
    while (Temp != NULL) {
        printf("%u\t%u\n", Temp->Rec.StartPos, Temp->Rec.Len);
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
    
    //Return += (-M->Data[i - 1].Len + R->StartPos - M->Data[i - 1].StartPos);
    
    return Return;
}

region_chain*
GetListHead(region_chain Any) {
    if (!Any.Back && !Any.Next) return NULL;
    while (Any.Back) Any = *Any.Back;
    if (Any.Next->Back == NULL) return Any.Next;
    return Any.Next->Back;
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
GetOrientation(nar_record M, nar_record S) {
    rec_or Return = (rec_or)0;
    
    UINT32 MEP = M.StartPos + M.Len;
    UINT32 SEP = S.StartPos + S.Len;
    
    if (MEP <= S.StartPos) {
        Return = rec_or::LEFT;
    }
    else if (M.StartPos >= SEP) {
        Return = rec_or::RIGHT;
    }
    else if (SEP >= MEP && S.StartPos <= M.StartPos) {
        Return = rec_or::OVERRUN;
    }
    else if ((SEP < MEP && M.StartPos < S.StartPos)) {
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
    
    if (ID == -1) return;
    
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





BOOLEAN
CopyData(HANDLE S, HANDLE D, ULONGLONG Len) {
    BOOLEAN Return = TRUE;
    
    UINT32 BufSize = 64 * 1024 * 1024; //64 MB
    ULONGLONG TotalCopied = 0;

    void* Buffer = malloc(BufSize);
    if (Buffer != NULL) {
        ULONGLONG BytesRemaining = Len;
        DWORD BytesOperated = 0;
        if (BytesRemaining > BufSize) {
            
            while (BytesRemaining > BufSize) {
                if (ReadFile(S, Buffer, BufSize, &BytesOperated, 0)) {
                    if (!WriteFile(D, Buffer, BufSize, &BytesOperated, 0) || BytesOperated != BufSize) {
                        printf("COPY_DATA: Writefile failed\n");
                        printf("Tried to write -> %I64d, Bytes written -> %d\n", Len, BytesOperated);
                        DisplayError(GetLastError());
                        Return = FALSE;
                        break;
                    }
                    else {
                        TotalCopied += BufSize;
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
            if (ReadFile(S, Buffer, (DWORD)BytesRemaining, &BytesOperated, 0) && BytesOperated == BytesRemaining) {
                
                if (!WriteFile(D, Buffer, (DWORD)BytesRemaining, &BytesOperated, 0) || BytesOperated != BytesRemaining) {
                    printf("COPY_DATA: Error occured while copying end of buffer\n");
                    printf("Bytes written -> %lu , instead of %I64u \n", BytesOperated, BytesRemaining);
                    DisplayError(GetLastError());
                    Return = FALSE;
                }
                else {
                    TotalCopied += BytesRemaining;
                }

            }
            
        }
        
        
    }//If Buffer != NULL
    else {
        printf("Can't allocate memory for buffer\n");
        Return = FALSE;
    }
    
    free(Buffer);
    if (Return == FALSE) {
        printf("COPYFILE error detected, copied %I64d bytes instead of %I64d\n", TotalCopied, Len);
    }
    return Return;
}




inline BOOLEAN
InitNewLogFile(volume_backup_inf* V) {
    BOOLEAN Return = FALSE;
    CloseHandle(V->LogHandle);
    
    V->LogHandle = CreateFileW(GenerateLogFileName(V->Letter, V->CurrentLogIndex).c_str(),
                               GENERIC_READ | GENERIC_WRITE, 0, 0,
                               CREATE_ALWAYS, 0, 0);
    if (V->LogHandle != INVALID_HANDLE_VALUE) {
        V->IncRecordCount = 0;
        Return = TRUE;
    }
    else {
        printf("Couldnt initialize new log file for version %i\n", V->CurrentLogIndex);
    }
    
    return Return;
}


#pragma warning(push)
#pragma warning(disable:4477)
inline void
StrToGUID(const char* guid, GUID* G) {
    if (!G) return;
    sscanf(guid, "{%8X-%4hX-%4hX-%2hX%2hX-%2hX%2hX%2hX%2hX%2hX%2hX}", &G->Data1, &G->Data2, &G->Data3, &G->Data4[0], &G->Data4[1], &G->Data4[2], &G->Data4[3], &G->Data4[4], &G->Data4[5], &G->Data4[6], &G->Data4[7]);
}
#pragma warning(pop)


#if 0
BOOLEAN
SaveExtraPartitions(volume_backup_inf* V) {
    
    //NOTE these guids are fetched from winapi: https://docs.microsoft.com/en-us/windows/win32/api/WinIoCtl/ns-winioctl-partition_information_gpt
    GUID GDAT = { 0 }; // basic data partition
    GUID GMSR = { 0 }; // microsoft reserved partition guid
    GUID GEFI = { 0 }; // efi partition guid
    GUID GREC = { 0 }; // recovery partition guid
    
    StrToGUID("{ebd0a0a2-b9e5-4433-87c0-68b6b72699c7}", &GDAT);
    StrToGUID("{e3c9e316-0b5c-4db8-817d-f92df00215ae}", &GMSR);
    StrToGUID("{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}", &GEFI);
    StrToGUID("{de94bba4-06d1-4d40-a16a-bfd50179d6ac}", &GREC);
    
    BOOLEAN Result = TRUE;
    
    wchar_t VolPath[128];
    wchar_t Vol[] = L" :\\";
    Vol[0] = V->Letter;
    
    VOLUME_DISK_EXTENTS* Ext = NULL;
    DRIVE_LAYOUT_INFORMATION_EX* DL = NULL;
    
    DWORD BS = 1024 * 2;
    DWORD T = 0;
    
    
    GetVolumeNameForVolumeMountPointW(Vol, VolPath, 128);
    VolPath[lstrlenW(VolPath) - 1] = L'\0'; //Remove trailing slash
    HANDLE Drive = CreateFileW(VolPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    
    if (Drive != INVALID_HANDLE_VALUE) {
        printf("Searching disk of volume %S\n", VolPath);
        
        Ext = (VOLUME_DISK_EXTENTS*)malloc(BS);
        DL = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(BS);
        
        if (DeviceIoControl(Drive, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0, Ext, BS, &T, 0)) {
            for (unsigned int i = 0; i < Ext->NumberOfDiskExtents; i++) {
                
                wchar_t DiskPath[] = L"\\\\?\\PhysicalDriveX";
                DiskPath[lstrlenW(DiskPath) - 1] = Ext->Extents[i].DiskNumber + '0';
                
                HANDLE Disk = CreateFileW(DiskPath, GENERIC_READ,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
                if (Disk != INVALID_HANDLE_VALUE) {
                    
                    if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DL, BS, &T, 0)) {
                        
                        for (int k = 0; k < DL->PartitionCount; k++) {
                            PARTITION_INFORMATION_EX* PI = &DL->PartitionEntry[k];
                            
                            if (PI->PartitionStyle == PARTITION_STYLE_MBR) {
                                printf("This version can't backup MBR system partitions\n");
                                // NOTE(Batuhan) : Need MBR OS partition to test this
                                if (0) {// TODO(Batuhan): add mbr filter
                                    std::wstring SPNAME = GenerateSystemPartitionFileName(V->Letter);
                                    HANDLE SP = CreateFileW(SPNAME.c_str(), GENERIC_WRITE, 0, 0, CREATE_NEW, 0, 0);
                                    if (SP != INVALID_HANDLE_VALUE) {
                                        if (!CopyData(Disk, SP, PI->PartitionLength.QuadPart, PI->StartingOffset.QuadPart)) {
                                            printf("Cant copy EFI partition\n");
                                        }
                                    }
                                    else {
                                        printf("Cant open file %S to write\n", SPNAME.c_str());
                                        Result = FALSE;
                                    }
                                    CLEANHANDLE(SP);
                                }
                            }
                            if (PI->PartitionStyle == PARTITION_STYLE_GPT) {
                                
                                
                                if (IsEqualGUID(PI->Gpt.PartitionType, GEFI)) {
                                    //Efi system partition
                                    printf("EFI partition detected\n");
                                    std::wstring EFIFNAME = GenerateSystemPartitionFileName(V->Letter);
                                    HANDLE SP = CreateFileW(EFIFNAME.c_str(), GENERIC_WRITE, 0, 0, CREATE_NEW, 0, 0);
                                    if (SP != INVALID_HANDLE_VALUE) {
                                        if (!CopyData(Disk, SP, PI->PartitionLength.QuadPart, PI->StartingOffset.QuadPart)) {
                                            printf("Cant copy EFI partition\n");
                                        }
                                    }
                                    else {
                                        printf("Cantt open file %S to write\n", EFIFNAME.c_str());
                                        Result = FALSE;
                                    }
                                    CLEANHANDLE(SP);
                                    
                                }
                                
                                if (IsEqualGUID(PI->Gpt.PartitionType, GMSR)) {
                                    printf("MSR partition detected\n");
                                    
                                    //MSR partition
                                    std::wstring MSRFNAME = GenerateMSRFileName(V->Letter);
                                    HANDLE MSRFile = CreateFileW(MSRFNAME.c_str(), GENERIC_WRITE, 0, 0, CREATE_NEW, 0, 0);
                                    if (MSRFile != INVALID_HANDLE_VALUE) {
                                        if (!CopyData(Disk, MSRFile, PI->PartitionLength.QuadPart, PI->StartingOffset.QuadPart)) {
                                            printf("Cant copy MSR partition\n");
                                        }
                                    }
                                    else {
                                        printf("Cant open file %S to write\n", MSRFNAME.c_str());
                                        Result = FALSE;
                                    }
                                    CLEANHANDLE(MSRFile);
                                    
                                }
                                if (IsEqualGUID(PI->Gpt.PartitionType, GREC)) {
                                    printf("Recovery partition detected\n");
                                    std::wstring RECFNAME = GenerateRECFileName(V->Letter);
                                    HANDLE RECFile = CreateFileW(RECFNAME.c_str(), GENERIC_WRITE, 0, 0, CREATE_NEW, 0, 0);
                                    if (RECFile != INVALID_HANDLE_VALUE) {
                                        if (!CopyData(Disk, RECFile, PI->PartitionLength.QuadPart, PI->StartingOffset.QuadPart)) {
                                            printf("Cant copy MSR partition\n");
                                        }
                                    }
                                    else {
                                        printf("Cant open file %S to write\n", RECFNAME.c_str());
                                        Result = FALSE;
                                    }
                                    CLEANHANDLE(RECFile);
                                    
                                    
                                }
                                
#if 0
                                if (PI->Gpt.Attributes != 0) {
                                    printf("Attributes :\n");
                                }
                                if (PI->Gpt.Attributes & GPT_ATTRIBUTE_PLATFORM_REQUIRED) {
                                    printf("\tPlatform required\n");
                                }
                                if (PI->Gpt.Attributes & GPT_BASIC_DATA_ATTRIBUTE_HIDDEN) {
                                    printf("\tData attribute hidden\n");
                                }
                                if (PI->Gpt.Attributes & GPT_BASIC_DATA_ATTRIBUTE_NO_DRIVE_LETTER) {
                                    printf("\tNo drive letter\n");
                                }
                                if (PI->Gpt.Attributes & GPT_BASIC_DATA_ATTRIBUTE_SHADOW_COPY) {
                                    printf("\tShadow copy\n");
                                }
#endif
                                
                            }
                            if (PI->PartitionStyle == PARTITION_STYLE_RAW) {
                                printf("ERROR : Raw partition");
                            }
                            
                        }
                        
                        
                    }
                    else {
                        printf("DeviceIOControl get drive layout failed err => %i\n", GetLastError());
                        Result = FALSE;
                    }
                    
                    CloseHandle(Disk);
                    
                }
                else {
                    printf("Unable to open disk, error => %i\n", GetLastError());
                    Result = FALSE;
                }
                
            }
        }
        else {
            printf("DeviceIoControl, get disk extents failed,"
                   "last error %i\n", GetLastError());
            Result = FALSE;
        }
        
    }
    else {
        printf("Can't open volume\n");
        Result = FALSE;
    }
    
    CLEANHANDLE(Drive);
    free(Ext);
    free(DL);
    
    return Result;
}
#endif


inline BOOLEAN
InitRestoreTargetInf(restore_inf* Inf, wchar_t Letter) {
    
    if (!Inf) return FALSE;
    
    BOOLEAN Return = FALSE;
    WCHAR Temp[] = L"!:\\";
    Temp[0] = Letter;
    
    return Return;
}


inline BOOLEAN
InitVolumeInf(volume_backup_inf* VolInf, wchar_t Letter, BackupType Type) {
    BOOLEAN Return = FALSE;
    
    VolInf->Letter = Letter;
    VolInf->FilterFlags.IsActive = FALSE;
    VolInf->IsOSVolume = FALSE;
    VolInf->INVALIDATEDENTRY = FALSE;
    VolInf->FullBackupExists = FALSE;
    VolInf->BT = Type;
    
    VolInf->FilterFlags.SaveToFile = TRUE;
    VolInf->FilterFlags.FlushToFile = FALSE;
    
    VolInf->CurrentLogIndex = 0;
    
    VolInf->ClusterSize = 0;
    VolInf->RecordsMem.clear();
    VolInf->VSSPTR = 0;
    
    VolInf->LogHandle = INVALID_HANDLE_VALUE;
    VolInf->IncRecordCount = 0;
    
    VolInf->Stream.Records = { 0,0 };
    VolInf->Stream.Handle = 0;
    VolInf->Stream.RecIndex = 0;
    VolInf->Stream.ClusterIndex = 0;
    
    
    DWORD BytesPerSector = 0;
    DWORD SectorsPerCluster = 0;
    DWORD ClusterCount = 0;
    
    wchar_t DN[] = L"C:";
    DN[0] = Letter;
    
    
    wchar_t V[] = L"C:\\";
    V[0] = Letter;
    if (GetDiskFreeSpaceW(V, &SectorsPerCluster, &BytesPerSector, 0, &ClusterCount)) {
        VolInf->ClusterSize = SectorsPerCluster * BytesPerSector;
        VolInf->VolumeTotalClusterCount = ClusterCount;
        WCHAR windir[MAX_PATH];
        GetWindowsDirectory(windir, MAX_PATH);
        if (windir[0] == Letter) {
            //Selected volume contains windows
            VolInf->IsOSVolume = TRUE;
        }
        Return = TRUE;
    }
    else {
        printf("Cant get disk information from WINAPI\n");
        DisplayError(GetLastError());
    }
    // VolInf->IsOSVolume = FALSE;
    
    return Return;
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
    wchar_t* Result = NULL;
    BOOLEAN Error = TRUE;
    VSS_ID sid;
    HRESULT res;
    
    res = CreateVssBackupComponents(&ptr);
    
    if (S_OK == ptr->InitializeForBackup()) {
        if (S_OK == ptr->SetContext(VSS_CTX_BACKUP)) {
            if (S_OK == ptr->StartSnapshotSet(&sid)) {
                if (S_OK == ptr->SetBackupState(false, false, VSS_BACKUP_TYPE::VSS_BT_FULL, false)) {
                    if (S_OK == ptr->AddToSnapshotSet((LPWSTR)Drive.c_str(), GUID_NULL, &sid)) {
                        CComPtr<IVssAsync> Async;
                        if (S_OK == ptr->PrepareForBackup(&Async)) {
                            Async->Wait();
                            CComPtr<IVssAsync> Async2;
                            if (S_OK == ptr->DoSnapshotSet(&Async2)) {
                                Async2->Wait();
                                Error = FALSE;
                            }
                            else {
                                printf("Can't do snapshotset\n");
                            }
                        }
                        else {
                            printf("Can't prepare for backup");
                        }
                    }
                    else {
                        printf("Can't add volume to snapshot set\n");
                    }
                }
                else {
                    printf("Can't set VSS backup state\n");
                }
            }
            else {
                printf("Can't start VSS snapshotset\n");
            }
        }
        else {
            printf("Can't set VSS context\n");
        }
    }
    else {
        printf("Can't initialize VSS for backup\n");
    }
    
    if (!Error) {
        VSS_SNAPSHOT_PROP SnapshotProp;
        ptr->GetSnapshotProperties(sid, &SnapshotProp);
        
        Result = (wchar_t*)malloc(sizeof(wchar_t) * lstrlenW(SnapshotProp.m_pwszSnapshotDeviceObject));
        
        Result = lstrcpyW(Result, SnapshotProp.m_pwszSnapshotDeviceObject);
    }
    
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
    //TODO  REMOVE THIS IF0
    Context->CleaningUp = FALSE;
    
#if 1
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
#endif
    
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


#pragma warning(push)
#pragma warning(disable:4365)
#pragma warning(disable:4244)
//THIS FUNCTION CONTAINS BYTE MASKING AND CODE FOR EXTRACTING NON-STANDART BYTE SIZES (3-5 bytes), BETTER OMIT THIS WARNING

data_array<nar_record>
GetMFTLCN(char VolumeLetter, HANDLE VolumeHandle) {
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

    
    //return Result;

    UINT32 MEMORY_BUFFER_SIZE = 1024LL * 1024LL * 1024LL;
    UINT32 ClusterExtractedBufferSize = 1024 * 1024 * 5;

    struct {
        UINT32 Count[8];
        UINT32 Start[8];
    }DEBUG_CLUSTER_INFORMATION;
    
    nar_record* ClustersExtracted = (nar_record*)malloc(ClusterExtractedBufferSize);
    int ClusterExtractedCount = 0;
    memset(ClustersExtracted, 0, ClusterExtractedBufferSize);
#define NAR_INSERT_CLUSTER(START, LEN) ClustersExtracted[ClusterExtractedCount++] = {(UINT32)(START), (UINT32)(LEN) };
    
    int DBG_INDX_FOUND = 0;
    int DBG_TOTAL_DIR_FOUND = 0;
    
    int DBG_CLUSTER_COUNT = 0;
    int DBG_TOTAL_FILES_FOUND = 0;
    memset(&DEBUG_CLUSTER_INFORMATION, 0, sizeof(DEBUG_CLUSTER_INFORMATION));

//#define DBG_INSERT(START,COUNT) DEBUG_CLUSTER_INFORMATION.Start[DBG_CLUSTER_COUNT] = (UINT32)(START); DEBUG_CLUSTER_INFORMATION.Count[DBG_CLUSTER_COUNT] = (UINT32)(COUNT); (DBG_CLUSTER_COUNT)++;
#define DBG_INSERT

    void* FileBuffer = malloc(MEMORY_BUFFER_SIZE);
    UINT32 FileBufferCount = MEMORY_BUFFER_SIZE / 1024LL;

    if (FileBuffer) {
        char VolumeName[64];
        sprintf(VolumeName, "\\\\.\\%c:", VolumeLetter);

        
        if (VolumeHandle != INVALID_HANDLE_VALUE) {

            // NOTE(Batuhan): IMPORTANT, first mft entry offset is hardcoded, for different sector sizes that might
            // change. sadly i havent found official document to says like, mft starts at 0x10th sector.
            // on my machine it always starts from 0x10th sector. which is second cluster if your sector size is 512
            // and cluster size is 8*sector



             //nar_region *MFTRegions = 0;
            DWORD BR = 0;

            for (unsigned int MFTOffsetIndex = 0; MFTOffsetIndex < Result.Count; MFTOffsetIndex++) {

                // NOTE(Batuhan): IMPORTANT assumption made, cluster size 4096 bytes.
                ULONGLONG Offset = (ULONGLONG)Result.Data[MFTOffsetIndex].StartPos * 4096ULL;
                UINT32 FileCount = (ULONGLONG)Result.Data[MFTOffsetIndex].Len * 4; // its 4 file record per cluster, since file records are 1 KB

                // set file pointer to actual records
                if (NarSetFilePointer(VolumeHandle, Offset)) {

                    // NOTE(Batuhan): Check if we can read all file records at once

                    if (FileBufferCount > FileCount) {
                        // We can read all file records for given data region

                        if (ReadFile(VolumeHandle, FileBuffer, FileCount * 1024, &BR, 0) && BR == FileCount * 1024) {

                            INT16 DirectoryFlag = 0x0002;
                            INT32 FileRecordSize = 1024;
                            INT16 FlagOffset = 22;
                            INT32 NAR_INDEX_ALLOCATION_FLAG = 0xA0;

                            for (unsigned int FileRecordIndex = 0; FileRecordIndex < FileCount; FileRecordIndex++) {


                                void* FileRecord = (BYTE*)FileBuffer + (UINT64)FileRecordSize * (UINT64)FileRecordIndex;

                                // file flags are at 22th offset in the record
                                if (*(INT32*)FileRecord != 'ELIF') {
                                    // block doesnt start with 'FILE0', skip
                                    continue;
                                }
                                
                                

                                DBG_TOTAL_FILES_FOUND++;

                                INT16 Flags = *(INT16*)((BYTE*)FileRecord + FlagOffset);
                                

                                if ((Flags & DirectoryFlag) == DirectoryFlag) {
                                    // that is a directory 
                                    DBG_TOTAL_DIR_FOUND++;
                                    /*
                                    As far as i know, attributes will be sorted. so i can early terminate if i can just compare CurrentAttr>INDEX_ALLOCCATION
                                    */

                                    INT16 FirstAttributeOffset = (*(INT16*)((BYTE*)FileRecord + 20));
                                    void* FileAttribute = (char*)FileRecord + FirstAttributeOffset;

                                    INT32 RemainingLen = *(INT32*)((BYTE*)FileRecord + 24); // Real size of the file record
                                    RemainingLen -= (FirstAttributeOffset + 8); //8 byte for end of record mark, remaining len includes it too.

                                    // iterate until file attr 0xA0 found, which is INDEX_ALLOCATION
                                    // maybe some dir does not contain INDEX_ALLOCATION, like empty dirs, or other things i am not aware of.
                                    // its better write solid solution

                                    while (RemainingLen > 0) {

                                        if ((*(INT32*)FileAttribute & NAR_INDEX_ALLOCATION_FLAG) == NAR_INDEX_ALLOCATION_FLAG) {

                                            INT32 DataRunsOffset = *(INT32*)((BYTE*)FileAttribute + 32);

                                            void* DataRuns = (char*)FileAttribute + DataRunsOffset;

                                            // So it looks like dataruns doesnt actually tells you LCN, to save up space, they kinda use smt like 
                                            // winapi's deviceiocontrol routine, maybe the reason for fetching VCN-LCN maps from winapi is weird because 
                                            // thats how its implemented at ntfs at first place. who knows
                                            // so thats how it looks
                                            /*
                                            first one is always absolute LCN in the volume. rest of it is addition to previous one. if file is fragmanted, value will be
                                            negative. so we dont have to check some edge cases here.
                                            second data run will be lets say 0x11 04 43
                                            and first one                    0x11 10 10
                                            starting lcn is 0x10, but second data run does not start from 0x43, it starts from 0x10 + 0x43

                                            LCN[n] = LCN[n-1] + datarun cluster
                                            */

                                            char Size = *(BYTE*)DataRuns;
                                            INT8 ClusterCountSize = (Size & 0x0F);
                                            INT8 FirstClusterSize = (Size & 0xF0) >> 4;

                                            // Swipe to left to clear extra bits, then swap back to get correct result.
                                            INT64 ClusterCount = *(INT64*)((char*)DataRuns + 1); // 1 byte for size variable
                                            ClusterCount = ClusterCount & ~(0xFFFFFFFFFFFFFFFFULL << (ClusterCountSize * 8));
                                            // cluster count must be > 0, no need to do 2s complement on it

                                            //same operation
                                            INT64 FirstCluster = *(INT64*)((char*)DataRuns + 1 + ClusterCountSize);
                                            FirstCluster = FirstCluster & ~(0xFFFFFFFFFFFFFFFFULL << (FirstClusterSize * 8));
                                            // 2s complement to support negative values
                                            if ((FirstCluster >> ((FirstClusterSize - 1) * 8 + 7)) & 1U) {
                                                FirstCluster = FirstCluster | ((0xFFFFFFFFFFFFFFFULL << (FirstClusterSize * 8)));
                                            }



                                            INT64 OldClusterStart = FirstCluster;
                                            void* D = (BYTE*)DataRuns + FirstClusterSize + ClusterCountSize + 1;


                                            DBG_INSERT(FirstCluster, ClusterCount);
                                            NAR_INSERT_CLUSTER(FirstCluster, ClusterCount);
                                            DBG_INDX_FOUND++;

                                            while (*(BYTE*)D) {

                                                Size = *(BYTE*)D;

                                                //UPDATE: Even tho entry finishes at 8 byte aligment, it doesnt finish itself here, adds at least 1 byte for zero termination to indicate its end
                                                //so rather than tracking how much we read so far, we can check if we encountered zero termination
                                                // NOTE(Batuhan): Each entry is at 8byte aligment, so lets say we thought there must be smt like 80 bytes reserved for 0xA0 attribute
                                                // but since data run's size is not constant, it might finish at not exact multiple of 8, like 75, so rest of the 5 bytes are 0
                                                // We can keep track how many bytes we read so far etc etc, but rather than that, if we just check if size is 0, which means rest is just filler 
                                                // bytes to aligment border, we can break early.
                                                if (Size == 0) break;



                                                // extract 4bit nibbles from size
                                                ClusterCountSize = (Size & 0x0F);
                                                FirstClusterSize = (Size & 0xF0) >> 4;

                                                // Sparse files may cause that, but not sure about what is sparse and if it effects directories. 
                                                // If not, nothing to worry about, branch predictor should take care of that
                                                if (ClusterCountSize == 0 || FirstClusterSize == 0)break;

                                                // Swipe to left to clear extra bits, then swap back to get correct result.
                                                ClusterCount = *(INT64*)((BYTE*)D + 1);
                                                ClusterCount = ClusterCount & ~(0xFFFFFFFFFFFFFFFFULL << (ClusterCountSize * 8));

                                                //same operation
                                                FirstCluster = *(INT64*)((BYTE*)D + 1 + ClusterCountSize);
                                                FirstCluster = FirstCluster & ~(0xFFFFFFFFFFFFFFFFULL << (FirstClusterSize * 8));
                                                if ((FirstCluster >> ((FirstClusterSize - 1) * 8 + 7)) & 1U) {
                                                    FirstCluster = FirstCluster | (0xFFFFFFFFFFFFFFFFULL << (FirstClusterSize * 8));
                                                }


                                                //if(TempVariable > 1 && FirstCluster < 0){
                                                //    dbgbreak
                                                //}


                                                FirstCluster += OldClusterStart;
                                                // Update tail
                                                OldClusterStart = FirstCluster;

                                                D = (BYTE*)D + (FirstClusterSize + ClusterCountSize + 1);
                                                DBG_INSERT(FirstCluster, ClusterCount);
                                                NAR_INSERT_CLUSTER(FirstCluster, ClusterCount);
                                                DBG_INDX_FOUND++;
                                                
                                            }


                                            // found INDEX_ALLOCATION attribute
                                            break;
                                        }

                                        if ((*(INT32*)FileAttribute & NAR_INDEX_ALLOCATION_FLAG) > NAR_INDEX_ALLOCATION_FLAG) break;// early termination


                                        // found neither indexallocation nor attr bigger than it, just substract attribute size from remaininglen to determine if we should keep iterating

                                        RemainingLen -= (*(INT32*)((BYTE*)FileAttribute + 4));
                                        FileAttribute = (BYTE*)FileAttribute + *(INT32*)((BYTE*)FileAttribute + 4);

                                    }


                                }
                                else {
                                    // not a directory, skip
                                    continue;
                                }


                            }



                        }
                        else {
                        printf("Couldnt read file records\n");
#if _DEBUG
                            DebugBreak();
#endif

                        }


                    }
                    else {
#if _DEBUG
                        DebugBreak();
#endif
                        printf("FileCount %i\t FileBufferCount %i\n", FileCount, FileBufferCount);
                        // must iterate 

                    }


                }
                else {
                printf("Couldnt set file pointer to %I64d\n", Offset);
                }



            }






        }
        else {
            // couldnt open volume as file
            printf("VSSVolumeHandle was invalid \n");
            DisplayError(GetLastError());
        }

    }
    else {
    printf("FATAL: Couldnt allocate memory for file buffer\n");
        // NOTE(Batuhan): cant allocate memory
    }

    

    Result.Data = (nar_record*)realloc(Result.Data, (Result.Count + ClusterExtractedCount)*sizeof(nar_record));
    memcpy(Result.Data, ClustersExtracted, ClusterExtractedCount*sizeof(nar_record));
    
    Result.Count = Result.Count + ClusterExtractedCount;
    
    qsort(Result.Data, Result.Count, sizeof(nar_record), CompareNarRecords);
    MergeRegions(&Result);
    
    free(ClustersExtracted);
    free(FileBuffer);
    
    return Result;
}

#pragma warning(pop)




/*
This operation just adds volume to list, does not starts to filter it,
until it's fullbackup is requested. After fullbackup, call AttachVolume to start filtering
*/
BOOLEAN
AddVolumeToTrack(PLOG_CONTEXT Context, wchar_t Letter, BackupType Type) {
    BOOLEAN ErrorOccured = TRUE;
    
    volume_backup_inf VolInf;
    BOOLEAN FOUND = FALSE;
    
    if (Context->Volumes.Data) {
        for (unsigned int i = 0; i < Context->Volumes.Count; i++) {
            if (Context->Volumes.Data[i].Letter == Letter) {
                FOUND = TRUE;
                break;
            }
        }
    }
    
    if (!FOUND) {
        
        NAR_COMMAND Command;
        Command.Type = NarCommandType_AddVolume;
        
        if (NarGetVolumeGUIDKernelCompatible(Letter, Command.VolumeGUIDStr)) {
            
            DWORD BytesReturned;
            printf("Volume GUID: %S\n", Command.VolumeGUIDStr);
            
            HRESULT hResult = FilterSendMessage(Context->Port, &Command, sizeof(Command), 0, 0, &BytesReturned);
            
            if (SUCCEEDED(hResult)) {
                if (InitVolumeInf(&VolInf, Letter, Type)) {
                    ErrorOccured = FALSE;
                }
                Context->Volumes.Insert(VolInf);
                printf("Volume %c inserted to the list\n", (char)Letter);
            }
            else {
                printf("KERNEL error occured: Couldnt add volume @ kernel side, error code %i\n", hResult);
            }
            
        }
        else {
            printf("Couldnt get volume kernel compatible volume GUID\n");
            DisplayError(GetLastError());
        }
        
        
    }
    else {
        printf("Volume %c is already in list\n", (char)Letter);
    }
    
    
    
    return !ErrorOccured;
}


BOOLEAN
GetVolumesOnTrack(PLOG_CONTEXT C, volume_information* Out, unsigned int BufferSize, int* OutCount) {
    
    
    if (!Out || !C || !C->Volumes.Data) {
        return FALSE;
    }
    
    int VolumesFound = 0;
    int MaxFit = BufferSize / sizeof(*Out);
    BOOLEAN Result = FALSE;
    
    for (unsigned int i = 0; i < C->Volumes.Count; i++) {
        
        volume_backup_inf* V = &C->Volumes.Data[i];
        if (MaxFit == VolumesFound) {
            Result = FALSE;
            break;
        }
        
        if (V->INVALIDATEDENTRY) {
            continue;
        }
        
        Out[VolumesFound].Letter = (char)V->Letter;
        Out[VolumesFound].Bootable = V->IsOSVolume;
        Out[VolumesFound].DiskID = NarGetVolumeDiskID((char)V->Letter);
        Out[VolumesFound].DiskType = NarGetVolumeDiskType((char)V->Letter);
        Out[VolumesFound].Size = NarGetVolumeSize((char)V->Letter);
        
        VolumesFound++;
        
    }
    
    if (Result == FALSE) {
        memset(Out, 0, BufferSize); //If any error occured, clear given buffer
    }
    else {
        *OutCount = VolumesFound;
    }
    
    return Result;
}



/*
For some reason, kernel and user GUID is not compatible for one character, which is just question mark (?)
. to be kernel compatible, one must call this function to communicate with kernel, otherwise kernel cant distinguish
given GUID and will return error.

VolumeGUID MUST have size of 98 bytes, (49 unicode char)
*/
BOOLEAN
NarGetVolumeGUIDKernelCompatible(wchar_t Letter, wchar_t *VolumeGUID) {
    
    if (VolumeGUID == NULL) return FALSE;
    BOOLEAN Result = FALSE;
    wchar_t MountPoint[] = L"!:\\";
    MountPoint[0] = Letter;
    
    
    wchar_t LocalGUID[50];
    // This function appends trailing backslash to string, we can ignore that, but still need to pass 50 cch long buffer
    // to not to fail in the function. silly way to do thing
    if (GetVolumeNameForVolumeMountPointW(MountPoint, LocalGUID, 50)) {     
        memcpy(VolumeGUID, LocalGUID, 49*sizeof(wchar_t));
        VolumeGUID[1] = L'?';
        // GetVolumeNameForVolumeMountPointW returns string with trailing backslash (\\), i dont remember that thing's name but it is it
        // but kernel doesnt use that, so just remove it
        if(VolumeGUID[48] == L'\\'){
            VolumeGUID[48] = '\0'; 
        }
        Result = TRUE;
    }
    
    
    return Result;
}



/*
 Just removes volume entry from kernel memory, does not unattaches it.
*/
BOOLEAN
NarRemoveVolumeFromKernelList(wchar_t Letter, HANDLE CommPort) {
    
    if(CommPort == INVALID_HANDLE_VALUE) return FALSE;
    BOOLEAN Result = FALSE;
    
    NAR_COMMAND Command = {};
    Command.Type = NarCommandType_DeleteVolume;
    
    if(NarGetVolumeGUIDKernelCompatible(Letter, Command.VolumeGUIDStr)){
        DWORD BR = 0;
        
        HRESULT hResult = FilterSendMessage(CommPort, &Command, sizeof(NAR_COMMAND), 0,0, &BR);
        if(SUCCEEDED(hResult)){
            Result = TRUE;
        }
        else{
            printf("FilterSendMessage failed, couldnt remove volume from kernel side, err %i\n", hResult);
            DisplayError(GetLastError());
        }
        
    }
    else{
        printf("Couldnt get kernel compatible guid\n");
    }
    
    return FALSE;
}



BOOLEAN
RemoveVolumeFromTrack(LOG_CONTEXT *C, wchar_t L) {
    BOOLEAN Result = FALSE;
    
    int ID = GetVolumeID(C, L);
    
    if (ID >= 0) {
        
        volume_backup_inf* V = &C->Volumes.Data[ID];
        DetachVolume(V);
        // Since searching volume in context done via checking letters, if I assign 0 to letter, it cant be detected, since its not any character at all, just null termination.
        V->Letter = 0;
        NarRemoveVolumeFromKernelList(L, C->Port);
        
        V->FullBackupExists = FALSE;
        V->FilterFlags.SaveToFile = TRUE;
        V->FilterFlags.FlushToFile = FALSE;
        V->FilterFlags.IsActive = FALSE;
        V->CurrentLogIndex = -1;
        CloseHandle(V->LogHandle);
        V->IncRecordCount = 0;
        FreeDataArray(&V->Stream.Records);
        V->Stream.RecIndex = 0;
        V->Stream.ClusterIndex = 0;
        CloseHandle(V->Stream.Handle);
        V->VSSPTR.Release();
        V->INVALIDATEDENTRY = TRUE;
        Result = TRUE;
        
    }
    else {
        printf("Unable to find volume %c in track list \n", L);
        Result = FALSE;
    }
    
    
    return Result;
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
    
    INT ID = NAR_INVALID_VOLUME_TRACK_ID;
    if (C->Volumes.Data != NULL) {
        if (C->Volumes.Count != 0) {
            for (unsigned int i = 0; i < C->Volumes.Count; i++) {
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

UINT32
ReadStream(volume_backup_inf* VolInf, void* Buffer, unsigned int TotalSize) {
    //TotalSize MUST be multiple of cluster size
    UINT32 Result = 0;
    BOOLEAN ErrorOccured = FALSE;

    int ClustersToRead = TotalSize / VolInf->ClusterSize;
    int BufferOffset = 0;
    DWORD BytesOperated = 0;
    if (TotalSize == 0) {
        printf("Passed totalsize as 0, terminating now\n");
        return TRUE;
    }
    

    unsigned int RemainingSize = TotalSize;
    void* CurrentBufferOffset = Buffer;
    
    if (VolInf->Stream.RecIndex >= VolInf->Stream.Records.Count) {
        printf("End of the stream\n", VolInf->Stream.RecIndex, VolInf->Stream.Records.Count);
        return Result;
    }

    while (RemainingSize) {

        if (VolInf->Stream.RecIndex >= VolInf->Stream.Records.Count) {
            printf("Rec index was higher than record's count, result %u, rec_index %i rec_count %i\n", Result, VolInf->Stream.RecIndex, VolInf->Stream.Records.Count);
            return Result;
        }

        DWORD BytesReadAfterOperation = 0;

        UINT64 ClustersRemainingByteSize = VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].Len - VolInf->Stream.ClusterIndex;
        ClustersRemainingByteSize *= VolInf->ClusterSize;

        DWORD ReadSize = MIN((UINT64)RemainingSize, ClustersRemainingByteSize);

        ULONGLONG FilePtrTarget = (ULONGLONG)VolInf->ClusterSize * ((ULONGLONG)VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].StartPos + (ULONGLONG)VolInf->Stream.ClusterIndex);
        if (NarSetFilePointer(VolInf->Stream.Handle, FilePtrTarget)) {

            BOOL OperationResult = ReadFile(VolInf->Stream.Handle, CurrentBufferOffset, ReadSize, &BytesReadAfterOperation, 0);
            Result += BytesReadAfterOperation;
            if (!OperationResult || BytesReadAfterOperation != ReadSize) {
              printf("STREAM ERROR: Couldnt read %lu bytes, instead read %lu, error code %i\n", ReadSize, BytesReadAfterOperation, OperationResult);
              printf("rec_index % i rec_count % i, remaining bytes %I64d, offset at disk %I64d\n", VolInf->Stream.RecIndex, VolInf->Stream.Records.Count, ClustersRemainingByteSize, FilePtrTarget);
              printf("Total bytes read for buffer %u\n", Result);
              ErrorOccured = TRUE;
            }
            

        }
        else {
            printf("Couldnt set file pointer to %I64d\n", FilePtrTarget);
            ErrorOccured = TRUE;
        }

        int ClusterToIterate = BytesReadAfterOperation / 4096;
        VolInf->Stream.ClusterIndex += ClusterToIterate;

        if (VolInf->Stream.ClusterIndex == VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].Len) {
            VolInf->Stream.ClusterIndex = 0;
            VolInf->Stream.RecIndex++;
        }
        if (VolInf->Stream.ClusterIndex > VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].Len) {
            printf("ClusterIndex exceeded region len, that MUST NOT happen at any circumstance\n");
            ErrorOccured = TRUE;
        }

        if (BytesReadAfterOperation % VolInf->ClusterSize != 0) {
            printf("Couldnt read cluster aligned size, error, read %i bytes instead of %i\n", BytesReadAfterOperation, ReadSize);
        }

        RemainingSize -= BytesReadAfterOperation;
        CurrentBufferOffset = (char*)Buffer + (Result);
        if (ErrorOccured) break;

    }

    if (ErrorOccured) return 0;

    return Result;


}

BOOLEAN
TerminateBackup(volume_backup_inf* V, BOOLEAN Succeeded) {
    BOOLEAN Return = FALSE;
    if (!V) return FALSE;
    
    if (!V->FullBackupExists) {
        //Termination of fullbackup
        printf("Fullbackup operation will be terminated\n");
        if (Succeeded) {
            
            if (SaveMetadata((char)V->Letter, NAR_FULLBACKUP_VERSION, V->ClusterSize, V->BT, V->Stream.Records, V->Stream.Handle)) {
                Return = TRUE;
                V->FullBackupExists = TRUE;
            }
            
        }
        else {
            
            //Somehow operation failed.
            V->FilterFlags.IsActive = FALSE;
            V->FilterFlags.SaveToFile = FALSE;
            V->FilterFlags.FlushToFile = FALSE;
            CLEANHANDLE(V->LogHandle);
            V->FullBackupExists = FALSE;
            V->CurrentLogIndex = 0;
            
        }
        
        //Termination of fullbackup
        //We should save records to file
        
    }
    else {
        //Termination of diff-inc backup
        printf("Termination of diff-inc backup\n");
        
        if (Succeeded) {
            // NOTE(Batuhan):
            printf("Will save metadata to working directory, Version : %i\n", V->CurrentLogIndex);
            SaveMetadata((char)V->Letter, V->CurrentLogIndex, V->ClusterSize, V->BT, V->Stream.Records, V->Stream.Handle);
            
            if (V->BT == BackupType::Inc) {
                
                if (SetFilePointer(V->LogHandle, 0, 0, FILE_BEGIN) == 0) {
                    
                    DWORD BytesWritten = 0;
                    DWORD BytesToWrite = V->Stream.Records.Count * sizeof(nar_record);
                    if (WriteFile(V->LogHandle, V->Stream.Records.Data, BytesToWrite, &BytesWritten, 0) && BytesWritten == BytesToWrite) {
                        
                        if (!SetEndOfFile(V->LogHandle)) {
                            printf("Can't set end of file [inc backup termination]\n");
                        }
                        
                        Return = TRUE;
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
            else {
                //Diff backup
                Return = TRUE;
            }
            
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
            
            //backup operation succeeded condition end (for diff and inc)
        }
        
        V->FilterFlags.FlushToFile = TRUE;
        V->FilterFlags.SaveToFile = TRUE;
    }
    
    CLEANHANDLE(V->Stream.Handle);
    CLEANMEMORY(V->Stream.Records.Data);
    V->Stream.Records.Count = 0;
    V->Stream.RecIndex = 0;
    V->Stream.ClusterIndex = 0;
    V->VSSPTR.Release();
    
    
    return Return;
}

BOOLEAN
CheckForErrors(PLOG_CONTEXT C) {
    if (C) {
        return C->DriverErrorOccured;
    }
    return FALSE;
}


/*
Type is optional, after first backup
*/
BOOLEAN
SetupStream(PLOG_CONTEXT C, wchar_t L, BackupType Type, DotNetStreamInf* SI) {
    
    printf("Entered setup stream\n");
    BOOLEAN Return = FALSE;
    
    int ID = GetVolumeID(C, L);
    
    if (ID < 0) {
        //If volume not found, try to add it
        printf("Couldnt find volume %c in list, adding it for stream setup\n", L);
        AddVolumeToTrack(C, L, Type);
        ID = GetVolumeID(C, L);
        if (ID < 0) {
            return FALSE;
        }
        
    }
    
    
    volume_backup_inf* VolInf = &C->Volumes.Data[ID];
    VolInf->VSSPTR.Release();
    VolInf->Stream.ClusterIndex = 0;
    VolInf->Stream.RecIndex = 0;
    VolInf->Stream.Records.Count = 0;
    
    if (SetupStreamHandle(VolInf)) {
        
        printf("Setup stream handle successfully\n");
        if (!VolInf->FullBackupExists) {
            printf("Fullbackup stream is preparing\n");
            //Fullbackup stream
            if (SetFullRecords(VolInf)) {
                Return = TRUE;
            }
            else {
                printf("SetFullRecords failed!\n");
            }
            
            if (SI && Return) {
                
                SI->ClusterCount = 0;
                SI->ClusterSize = VolInf->ClusterSize;
                SI->FileName = GenerateBinaryFileName(VolInf->Letter, NAR_FULLBACKUP_VERSION);
                for (unsigned int i = 0; i < VolInf->Stream.Records.Count; i++) {
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
                else {
                    printf("Couldn't setup diff records for streaming\n");
                }
                
            }
            else if (T == BackupType::Inc) {
                if (SetIncRecords(VolInf)) {
                    Return = TRUE;
                }
                else {
                    printf("Couldn't setup inc records for streaming\n");
                }
                
            }
            
            if (SI && Return) {
                
                SI->ClusterCount = 0;
                SI->ClusterSize = VolInf->ClusterSize;
                SI->FileName = GenerateBinaryFileName(VolInf->Letter, VolInf->CurrentLogIndex);
                for (unsigned int i = 0; i < VolInf->Stream.Records.Count; i++) {
                    SI->ClusterCount += VolInf->Stream.Records.Data[i].Len;
                }
                
            }
        }
        
    }
    else {
        printf("SetupStreamHandle failed\n");
    }
    
    printf("Totalbytes should be backed up %I64u\n", (UINT64)SI->ClusterCount*(UINT64)SI->ClusterSize);
                
    
    return Return;
}

BOOLEAN
SetFullRecords(volume_backup_inf* V) {
    
    BOOLEAN Return = FALSE;
    UINT* ClusterIndices = 0;
    
    STARTING_LCN_INPUT_BUFFER StartingLCN;
    StartingLCN.StartingLcn.QuadPart = 0;
    ULONGLONG MaxClusterCount = 0;
    DWORD BufferSize = 1024 * 1024 * 64; //  megabytes
    
    VOLUME_BITMAP_BUFFER* Bitmap = (VOLUME_BITMAP_BUFFER*)malloc(BufferSize);
    if (Bitmap != NULL) {
        HRESULT R = DeviceIoControl(V->Stream.Handle, FSCTL_GET_VOLUME_BITMAP, &StartingLCN, sizeof(StartingLCN), Bitmap, BufferSize, 0, 0);
        if (SUCCEEDED(R)) {
            
            MaxClusterCount = Bitmap->BitmapSize.QuadPart;
            DWORD ClustersRead = 0;
            UCHAR* BitmapIndex = Bitmap->Buffer;
            UCHAR BitmapMask = 1;
            UINT32 CurrentIndex = 0;
            UINT32 LastActiveCluster = 0;
            
            V->Stream.Records.Data = (nar_record*)malloc(sizeof(nar_record));
            V->Stream.Records.Data[0] = { 0,1 };
            // NOTE(Batuhan): first cluster will always be backed up
            
            V->Stream.ClusterIndex = 0;
            V->Stream.RecIndex = 0;
            V->Stream.Records.Count = 1;
            
            
            /*
                NOTE(Batuhan): Since first cluster added to list via assumption, start iterating from second one

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
            printf("Successfully set fullbackup records\n");
            Return = TRUE;
            
            
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
    
    free(Bitmap);
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
            printf("N of records => %ld \n", FileSize / (UINT32)sizeof(nar_record));
            DisplayError(GetLastError());
        }
        else {
            printf("IncRecordCount -> %i\n", VolInf->Stream.Records.Count);
            qsort(VolInf->Stream.Records.Data, VolInf->Stream.Records.Count, sizeof(nar_record), CompareNarRecords);
            MergeRegions(&VolInf->Stream.Records);
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

#if 0
BOOLEAN
SetupRestoreStream(PLOG_CONTEXT C, wchar_t Letter, void* Metadata, int MSize) {
    BOOLEAN Result = TRUE;
    
    volume_backup_inf* V = NULL;
    for (unsigned int i = 0; i < C->Volumes.Count; i++) {
        if (C->Volumes.Data[i].Letter == Letter) {
            V = &C->Volumes.Data[i];
        }
    }
    
    if (V == NULL) {
        printf("Couldnt find volume in context\n");
        return FALSE;
    }
    
    wchar_t VolumeName[] = L"\\\\.\\!:";
    VolumeName[lstrlenW(VolumeName) - 2] = Letter;
    V->Stream.Handle = CreateFileW(VolumeName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, 0, OPEN_EXISTING, 0);
    if (V->Stream.Handle == INVALID_HANDLE_VALUE) {
        printf("Can't open volume %S\n", VolumeName);
        Result = FALSE;
    }
    else {
        V->Stream.ClusterIndex = 0;
        V->Stream.RecIndex = 0;
        V->Stream.Records.Count = MSize / sizeof(nar_record);
        V->Stream.Records.Data = (nar_record*)malloc(MSize);
        
        memcpy(V->Stream.Records.Data, Metadata, MSize);
    }
    
    return Result;
}

BOOLEAN
WriteStream(restore_inf* RestoreInf, void* Data, int Size) {
    
    void* DataToCopy = Data;
    data_array<nar_record>* Records = &RestoreInf->Stream.Records;
    DWORD BytesWritten = 0;
    
    while (Size > 0) {
        
        if (RestoreInf->Stream.RecIndex == RestoreInf->Stream.Records.Count) {
            printf("ERR: Exceeded record len\n");
            goto TERMINATE;
        }
        if (Size < 0) {
            printf("Remaining bytes can't be lower than zero\n");
            goto TERMINATE;
        }
        
        ULONGLONG Remaining = (Records->Data[RestoreInf->Stream.RecIndex].Len - RestoreInf->Stream.ClusterIndex) * RestoreInf->ClusterSize;
        LARGE_INTEGER MoveTo = { 0 };
        LARGE_INTEGER NewFilePointer = { 0 };
        MoveTo.QuadPart = (ULONGLONG)(Records->Data[RestoreInf->Stream.RecIndex].StartPos + (ULONGLONG)RestoreInf->Stream.ClusterIndex) * (ULONGLONG)RestoreInf->ClusterSize;
        
        SetFilePointerEx(RestoreInf->Stream.Handle, MoveTo, &NewFilePointer, 0);
        
        if (NewFilePointer.QuadPart != MoveTo.QuadPart) {
            printf("Can't set file pointer to %I64d, instead set to %I64d\n", MoveTo.QuadPart, NewFilePointer.QuadPart);
            goto TERMINATE;
        }
        
        
        if (Size > Remaining) {
            //write operation exceeds region len
            if (!WriteFile(RestoreInf->Stream.Handle, DataToCopy, Remaining, &BytesWritten, 0) && BytesWritten != Size) {
                printf("Writefile failed, tried to write %i bytes, instead wrote %i\n", Size, BytesWritten);
                DisplayError(GetLastError());
                goto TERMINATE;
            }
            Size -= Remaining;
            
            RestoreInf->Stream.ClusterIndex = 0;
            RestoreInf->Stream.RecIndex++;
        }
        else {
            //write operation fits current region
            
            if (!WriteFile(RestoreInf->Stream.Handle, DataToCopy, Size, &BytesWritten, 0) && BytesWritten != Size) {
                printf("Writefile failed, tried to write %i bytes, instead wrote %i\n", Size, BytesWritten);
                DisplayError(GetLastError());
                goto TERMINATE;
            }
            
            RestoreInf->Stream.ClusterIndex += (Size / sizeof(nar_record));
            Size = 0;
            
            if (RestoreInf->Stream.ClusterIndex == RestoreInf->Stream.Records.Data[RestoreInf->Stream.RecIndex].Len) {
                //Whole region has been read, iterate next one
                RestoreInf->Stream.ClusterIndex = 0;
                RestoreInf->Stream.RecIndex++;
            }
            
        }
        
        
    }
    
    return TRUE;
    
    TERMINATE:
    printf("Operation failed, remaining bytes %i\n", Size);
    return FALSE;
    
}

#endif


#if 1
BOOLEAN
SetDiffRecords(volume_backup_inf* V) {
    printf("Entered SetDiffRecords\n");
    BOOLEAN Result = FALSE;
    
    HANDLE* F = (HANDLE*)malloc(sizeof(HANDLE) * (V->CurrentLogIndex + 1));
    memset(F, 0, sizeof(HANDLE) * (V->CurrentLogIndex + 1));
    
    DWORD* FS = (DWORD*)malloc(sizeof(DWORD) * (V->CurrentLogIndex + 1));
    memset(FS, 0, sizeof(DWORD) * (V->CurrentLogIndex + 1));
    
    
    DWORD TotalFileSize = 0;
    
    for (int i = 0; i < V->CurrentLogIndex; i++) {
        std::wstring FName = GenerateLogFileName(V->Letter, i).c_str();
        F[i] = CreateFile(FName.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        
        if (F[i] == INVALID_HANDLE_VALUE) {
            printf("Can't open file %S\n", FName.c_str());
            DisplayError(GetLastError());
            return FALSE;
        }
        
        DWORD Temp = GetFileSize(F[i], 0);
        
        printf("Opened file %S, size %i\n", FName.c_str(), Temp);
        
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
        
        std::wstring FN = GenerateLogFileName(V->Letter, i);
        
        DWORD BytesRead = 0;
        if (SetFilePointer(F[i], 0, 0, FILE_BEGIN) == 0) {
            
            if (LogRead > TotalFileSize / sizeof(nar_record)) {
                printf("Possibly access violation, LogRead -> %i , TotalFileSize -> %i\n", LogRead, TotalFileSize);
            }
            
            if (ReadFile(F[i], &V->Stream.Records.Data[LogRead], FS[i], &BytesRead, 0) && BytesRead == FS[i]) {
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
    
    
    // dont free F[V->CurrentLogIndex] since it isnt created in that context, leave caller's resources to caller :]
    
    for (int i = 0; i <= V->CurrentLogIndex; i++) {
        CloseHandle(F[i]);
    }
    
    
    if (Result == TRUE && V->Stream.Records.Count) {
        qsort(V->Stream.Records.Data, V->Stream.Records.Count, sizeof(nar_record), CompareNarRecords);
        MergeRegions(&V->Stream.Records);
        ULONGLONG Size = 0;
        for (int i = 0; i < V->Stream.Records.Count; i++) {
            Size += V->Stream.Records.Data[i].Len;
        }
        printf("TOTAL SIZE TO BE BACKED UP %I64d\n", Size * 4096LL);
        printf("Records count %i, record index %i\n", V->Stream.Records.Count, V->Stream.RecIndex);
    }
    
    free(F);
    free(FS);
    
    return Result;
}
#endif


BOOLEAN
SetupStreamHandle(volume_backup_inf* VolInf) {
    
    if (VolInf == NULL) {
        printf("volume_backup_inf is null\n");
        return FALSE;
    }
    VolInf->VSSPTR.Release();
    
    
    BOOLEAN ErrorOccured = FALSE;
    HRESULT Result = 0;
    
    VolInf->FilterFlags.FlushToFile = FALSE;
    VolInf->FilterFlags.SaveToFile = !VolInf->FullBackupExists;
    VolInf->FilterFlags.IsActive = TRUE;
    
    if (VolInf->FullBackupExists) {
        if (!DetachVolume(VolInf)) {
            printf("Detach volume failed\n");
            return FALSE;
        }
        printf("Volume detached !\n");
    }
    
    WCHAR Temp[] = L"!:\\";
    Temp[0] = VolInf->Letter;
    wchar_t* ShadowPathPtr = GetShadowPath(Temp, VolInf->VSSPTR);
    wchar_t ShadowPath[512];
    StrCpyW(ShadowPath, ShadowPathPtr);
    free(ShadowPathPtr);
    
    if (ShadowPath == NULL) {
        printf("Can't get shadowpath from VSS\n");
        return FALSE;
    }
    
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
    
    VolInf->Stream.Handle = CreateFileW(ShadowPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, NULL, NULL);
    
    
    if (VolInf->Stream.Handle == INVALID_HANDLE_VALUE) {
        printf("Can not open shadow path %S..\n", ShadowPath);
        DisplayError(GetLastError());
        return FALSE;
    }
    
    
    return TRUE;
}



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
    system("net stop minispy");
    system("net start minispy");
    BOOLEAN Result = FALSE;
    HRESULT hResult = FALSE;
    
    DWORD PID = GetCurrentProcessId();
    NAR_CONNECTION_CONTEXT CTX = { 0 };
    
    char Buffer[1024];
    GetWindowsDirectoryA(Buffer, 1024);
    char VolumePath[] = " :";
    VolumePath[0] = Buffer[0];
    char DeviceName[128];
    QueryDosDeviceA(VolumePath, DeviceName, 128);
    char* IntPtr = DeviceName;
    while (IsNumeric(*IntPtr) && *(++IntPtr) != '\0');
    CTX.OsDeviceID = atoi(IntPtr);
    CTX.PID = PID;
    CTX.OsDeviceID = Buffer[0];
    DWORD BytesWritten = 256;
    //GetUserNameW(&CTX.UserName[0], &BytesWritten) && BytesWritten != 0
    // TODO (Batuhan): Pass username as string
    if (TRUE) {
        
        hResult = FilterConnectCommunicationPort(MINISPY_PORT_NAME,
                                                 0,
                                                 &CTX, sizeof(CTX),
                                                 NULL, &Ctx->Port);
        
        if (!IS_ERROR(hResult)) {
            
            if (NarCreateThreadCom(Ctx)) {
                Result = TRUE;
            }
            else {
                CloseHandle(Ctx->Port);
                printf("Can't create thread to communicate with driver\n");
            }
            
        }
        else {
            printf("Could not connect to filter: 0x%08x\n", hResult);
            printf("Program PID is %d\n", PID);
            DisplayError(hResult);
        }
        
    }
    else {
        printf("Cant query username from system\n");
        printf("Bytes written %i, name => %S \n", BytesWritten, CTX.UserName);
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
OfflineRestoreCleanDisk(restore_inf* R, int DiskID) {
    if (R == NULL) {
        printf("Restore structure can't be null\n");
        return FALSE;
    }
    BOOLEAN Result = FALSE;
    
    backup_metadata M = ReadMetadata(R->SrcLetter, R->Version, R->RootDir);
    
    if (M.IsOSVolume) {
        if (M.DiskType == NAR_DISKTYPE_GPT) {
            
            if (NarCreateCleanGPTBootablePartition(DiskID, M.VolumeSize / (1024ull * 1024ull), 99, 529, (char)R->TargetLetter)) {
                Result = TRUE;
                printf("Successfully created bootable gpt partition\n");
            }
            else {
                printf("Can't create bootable GPT partition to restore\n");
            }
            
        }
        if (M.DiskType == NAR_DISKTYPE_MBR) {
            
            if (NarCreateCleanMBRBootPartition()/*TODO*/) {
                Result = TRUE;
            }
            else {
                printf("Can't create bootable MBR partition to restore\n");
            }
            
        }
    }
    else { //Is not OS volume
        if (M.DiskType == NAR_DISKTYPE_GPT) {
            
            if (NarCreateCleanGPTPartition(DiskID, M.VolumeSize / (1024 * 1024), R->TargetLetter)) {
                Result = TRUE;
            }
            else {
                printf("Can't create clean GPT partition to restore\n");
            }
            
        }
        if (M.DiskType == NAR_DISKTYPE_MBR) {
            
            if (NarCreateCleanMBRPartition()/*TODO*/) {
                // TODO(Batuhan):
                Result;
            }
            else {
                printf("Can't create MBR partition to restore\n");
            }
            
        }
    }
    
    if (Result) {
        
        Result = OfflineRestoreToVolume(R, TRUE);
        if (Result) {
            
            if (M.IsOSVolume) {
                if (!RestoreRecoveryFile(*R)) {
                    printf("Couldnt restore recovery partition\n");
                }
                else {
                    printf("Successfully restored recovery partition, continuing on boot repair\n");
                }
                NarRepairBoot((char)R->TargetLetter);
                printf("Restored to volume %c, to version %i\n", (char)R->TargetLetter, R->Version);
            }
            else {
                printf("Skipping boot repair since backup doesnt contain OS\n");
            }
            
        }
        else {
            printf("Can't restore to volume %c, to version %i\n", (char)R->TargetLetter, R->Version);
        }
        
    }
    
    
    return Result;
}

BOOLEAN
OfflineRestoreToVolume(restore_inf* R, BOOLEAN ShouldFormat) {
    
    BOOLEAN Result = FALSE;
    HANDLE VolumeToRestore = INVALID_HANDLE_VALUE;
    
    int Version = -10;
    ULONGLONG VolumeSize = 0;
    BackupType BT = BackupType::Diff;
    
    {
        backup_metadata BM = ReadMetadata(R->SrcLetter, R->Version, R->RootDir);
        if (BM.Errors.RegionsMetadata == 0) {
            Version = BM.Version;
            BT = BM.BT;
            VolumeSize = BM.VolumeSize;
        }
        else {
            printf("Couldnt read backup metadata\n");
            
            printf("Error RegionsMetadata %i\n", BM.Errors.RegionsMetadata);
            printf("Error Regions %i\n", BM.Errors.Regions);
            printf("Error MFTMetadata %i\n", BM.Errors.MFTMetadata);
            printf("Error MFT %i\n", BM.Errors.MFT);
            printf("Error Recovery %i\n", BM.Errors.Recovery);

            Result = FALSE;
        }
    }
    
    
    if (VolumeSize != 0) {
        
        if (ShouldFormat) {
            
            if (NarFormatVolume(R->TargetLetter)) {
                if (NarSetVolumeSize((char)R->TargetLetter, VolumeSize / (1024 * 1024))) {
                    //Success
                }
                else {
                    // NOTE(Batuhan):
                    printf("Couldn't set volume size\n");
                }
            }
            else {
                printf("Couldnt format volume\n");
            }
            
        }
        
        // Wait for diskpart operation to complete
        Sleep(500);

        HANDLE Volume = NarOpenVolume(R->TargetLetter);
        if (Volume != INVALID_HANDLE_VALUE) {
            if (Version == NAR_FULLBACKUP_VERSION) {
                printf("Fullbackup version will be restored\n");
                Result = RestoreVersionWithoutLoop(*R, FALSE, Volume);
                if (Result) {
                    printf("Successfully restored to full backup\n");
                }
            }
            else {
                if (BT == BackupType::Inc) {
                    printf("Incremental restore operation starting..\n");
                    Result = RestoreIncVersion(*R, Volume);
                }
                if (BT == BackupType::Diff) {
                    printf("Differential restore operation starting..\n");
                    Result = RestoreDiffVersion(*R, Volume);
                }
            }
        }
        else {
            Result = FALSE;
            printf("Couldnt open volume, terminating offline restore operation\n");
        }
        
        NarCloseVolume(Volume);
        
    }
    
    
    return Result;
}

BOOLEAN
NarFormatVolume(char Letter) {
    
    char Buffer[2096];
    memset(Buffer, 0, 2096);
    
    sprintf(Buffer, ""
            "select volume %c\n"
            "format fs = ntfs quick override\n"
            "exit\n", Letter);
    
    char InputFN[] = "NARDPINPUT";
    if (NarDumpToFile(InputFN, Buffer, strlen(Buffer))) {
        sprintf(Buffer, "diskpart /s %s", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    
    return FALSE;
}

void
NarRepairBoot(char Letter) {
    char Buffer[2096];
    sprintf(Buffer, "bcdboot %c:\\Windows", Letter);
    system(Buffer);
}


void
FreeBackupMetadataEx(backup_metadata_ex* BMEX) {
    FreeDataArray(&BMEX->RegionsMetadata);
    free(BMEX);
}


file_read
NarReadFile(const char* FileName) {
    file_read Result = { 0 };
    HANDLE File = CreateFileA(FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if (File != INVALID_HANDLE_VALUE) {
        DWORD BytesRead = 0;
        Result.Len = GetFileSize(File, 0);
        if (Result.Len == 0) return Result;
        Result.Data = malloc(Result.Len);
        if (Result.Data) {
            ReadFile(File, Result.Data, Result.Len, &BytesRead, 0);
            if (BytesRead == Result.Len) {
                // NOTE success
            }
            else {
                free(Result.Data);
                printf("Read %i bytes instead of %i\n", BytesRead, Result.Len);
            }
        }
        CloseHandle(File);
    }
    else {
        printf("Can't create file: %s\n", FileName);
    }
    //CreateFileA(FNAME, GENERIC_WRITE, 0,0 ,CREATE_NEW, 0,0)
    return Result;
}


file_read
NarReadFile(const wchar_t* FileName) {
    file_read Result = { 0 };
    HANDLE File = CreateFileW(FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if (File != INVALID_HANDLE_VALUE) {
        DWORD BytesRead = 0;
        Result.Len = GetFileSize(File, 0);
        if (Result.Len == 0) return Result;
        Result.Data = malloc(Result.Len);
        if (Result.Data) {
            ReadFile(File, Result.Data, Result.Len, &BytesRead, 0);
            if (BytesRead == Result.Len) {
                // NOTE success
            }
            else {
                free(Result.Data);
                printf("Read %i bytes instead of %i\n", BytesRead, Result.Len);
            }
        }
        CloseHandle(File);
    }
    else {
        printf("Can't create file: %S\n", FileName);
    }
    return Result;
}

void
FreeFileRead(file_read FR) {
    free(FR.Data);
}


BOOLEAN
NarDumpToFile(const char* FileName, void* Data, unsigned int Size) {
    BOOLEAN Result = FALSE;
    HANDLE File = CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (File != INVALID_HANDLE_VALUE) {
        DWORD BytesWritten = 0;
        WriteFile(File, Data, Size, &BytesWritten, 0);
        if (BytesWritten == Size) {
            Result = TRUE;
        }
        else {
            printf("Written %i bytes instead of %i\n", BytesWritten, Size);
        }
        CloseHandle(File);
    }
    else {
        printf("Can't create file: %s\n", FileName);
    }
    //CreateFileA(FNAME, GENERIC_WRITE, 0,0 ,CREATE_NEW, 0,0)
    return Result;
}


BOOLEAN
NarSetVolumeSize(char Letter, int TargetSizeMB) {
    BOOLEAN Result = 0;
    ULONGLONG VolSizeMB = 0;
    char Buffer[1024];
    char FNAME[] = "NARDPSCRPT";
    sprintf(Buffer, "%c:\\", Letter);
    
    ULARGE_INTEGER TOTAL_SIZE = { 0 };
    ULARGE_INTEGER A, B;
    GetDiskFreeSpaceExA(Buffer, 0, &TOTAL_SIZE, 0);
    VolSizeMB = TOTAL_SIZE.QuadPart / (1024 * 1024); // byte to MB
    if (VolSizeMB == TargetSizeMB) {
        return TRUE;;
    }
    
    /*
  extend size = X // extends volume by X. Doesnt resize, just adds X
  shrink desired = X // shrink volume to X, if X is bigger than current size, operation fails
  */
    if (VolSizeMB > TargetSizeMB) {
        // shrink volume
        ULONGLONG Diff = VolSizeMB - TargetSizeMB;
        sprintf(Buffer, "select volume %c\nshrink desired = %I64u\nexit\n", Letter, Diff);
    }
    else if(VolSizeMB < TargetSizeMB){
        // extend volume
        ULONGLONG Diff = TargetSizeMB - VolSizeMB;
        sprintf(Buffer, "select volume %c\nextend size = %I64u\nexit\n", Letter, Diff);
    }
    else {
        // VolSizeMB == TargetSizeMB
        // do nothing
        return TRUE;
    }
    
    //NarDumpToFile(const char *FileName, void* Data, int Size)
    if (NarDumpToFile(FNAME, Buffer, strlen(Buffer))) {
        char CMDBuffer[1024];
        sprintf(CMDBuffer, "diskpart /s %s", FNAME);
        system(CMDBuffer);
        // TODO(Batuhan): maybe check output
        Result = TRUE;
    }
    
    return Result;
}

BOOLEAN
CreatePartition(int Disk, char Letter, unsigned size) {
    BOOLEAN Result = FALSE;
    char Buffer[1024];
    
    if (Letter >= 'a' && Letter <= 'z') Letter += ('A' - 'a'); //convert to uppercase
    
    DWORD Drives = GetLogicalDrives();
    if (Drives & (1 << (Letter - 'A'))) {
        printf("Volume exists in system\n");
        return FALSE;
    }
    sprintf(Buffer, "select disk %i\n"
            "create partition primary size = %u\n"
            "format fs = \"ntfs\" quick"
            "assign letter = \"%c\"\n", Disk, size, Letter);
    char FileName[] = "NARDPSCRPT";
    
    if (NarDumpToFile(FileName, Buffer, strlen(Buffer))) {
        char CMDBuffer[1024];
        sprintf(CMDBuffer, "diskpart /s %s", FileName);
        system(CMDBuffer);
        // TODO(Batuhan): maybe check output
        Result = TRUE;
    }
    
    return Result;
}


// returns # of disks, returns 0 if information doesnt fit in array
data_array<disk_information>
NarGetDisks() {
    
    data_array<disk_information> Result = { 0, 0 };
    Result.Data = (disk_information*)malloc(sizeof(disk_information) * 1024);
    Result.Count = 1024;
    
    int DiskCount = 0;
    char TextBuffer[1024];
    sprintf(TextBuffer, "list disk\n");
    char DPInputFName[] = "DPINPUT";
    char DPOutputFName[] = "DPOUTPUT";
    
    if (NarDumpToFile(DPInputFName, TextBuffer, strlen(TextBuffer))) {
        char CMDBuffer[1024];
        sprintf(CMDBuffer, "diskpart /s %s >%s", DPInputFName, DPOutputFName);
        system(CMDBuffer);
    }
    
    
    file_read File = NarReadFile(DPOutputFName);
    char* Buffer = (char*)File.Data;
    
    DWORD DriveLayoutSize = 1024 * 4;
    int DGEXSize = 1024 * 4;
    DWORD MemorySize = DriveLayoutSize + DGEXSize;
    void* Memory = VirtualAlloc(0, MemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    DRIVE_LAYOUT_INFORMATION_EX* DriveLayout = (DRIVE_LAYOUT_INFORMATION_EX*)(Memory);
    DISK_GEOMETRY_EX* DGEX = (DISK_GEOMETRY_EX*)((char*)Memory + DriveLayoutSize);//
    
    
    DWORD HELL;
    
    
    //Find first occurance
    char Target[1024];
    int DiskIndex = 0;
    
    sprintf(Target, "Disk %i", DiskIndex);
    
    while (TRUE) {
        
        Buffer = strstr(Buffer, Target);
        if (Buffer != NULL) {
            
            
            Buffer = Buffer + strlen(Target); //bypass "Disk #" string
            
            Result.Data[DiskIndex].ID = DiskIndex;
            
            int NumberLen = 1;
            char Temp[32];
            memset(Temp, ' ', 32);
            
            //TODO increment buffer by sizeof target
            while (!IsNumeric(*(++Buffer)) && *Buffer != '\0'); //Find size
            while (*(++Buffer) != ' ' && *Buffer != '\0') NumberLen++; //Find size's len in char
            
            if (*Buffer == '\0') goto ERROR_TERMINATE; //that shouldnt happen
            
            strncpy(Temp, Buffer - NumberLen, NumberLen);
            Result.Data[DiskIndex].Size = atoi(Temp) * (ULONGLONG)1024 * 1024 * 1024; // Since diskpart returns size in GB(it might return in KB, but take it as 0 anyway)
            
            while (*(++Buffer) == ' ' && *Buffer != '\0');// find size identifier letter
            
            if (*Buffer == '\0') goto ERROR_TERMINATE;
            if (*Buffer != 'G') Result.Data[DiskIndex].Size = 0; //If not GB, we are not interested with this disk
            
            // Find unallocated space
            NumberLen = 1;
            while (!IsNumeric(*(++Buffer)) && *Buffer != '\0');
            while (*(++Buffer) != ' ' && *Buffer != '\0') NumberLen++; //Find size's len in char
            
            if (*Buffer == '\0') goto ERROR_TERMINATE;
            
            strncpy(Temp, Buffer - NumberLen, NumberLen); //Copy number
            Result.Data[DiskIndex].UnallocatedGB = atoi(Temp);
            Buffer++;  // increment to unit identifier, KB or GB
            if (*Buffer != 'G') Result.Data[DiskIndex].UnallocatedGB = 0;
            
            {
                char PHNAME[] = "\\\\?\\PhysicalDrive%i";
                sprintf(PHNAME, "\\\\?\\PhysicalDrive%i", Result.Data[DiskIndex].ID);
                
                HANDLE Disk = CreateFileA(PHNAME, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
                if (Disk != INVALID_HANDLE_VALUE) {
                    if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DriveLayout, DriveLayoutSize, &HELL, 0)) {
                        if (DriveLayout->PartitionStyle == PARTITION_STYLE_MBR) {
                            Result.Data[DiskIndex].Type = NAR_DISKTYPE_MBR;
                        }
                        if (DriveLayout->PartitionStyle == PARTITION_STYLE_GPT) {
                            Result.Data[DiskIndex].Type = NAR_DISKTYPE_GPT;
                        }
                        if (DriveLayout->PartitionStyle == PARTITION_STYLE_RAW) {
                            Result.Data[DiskIndex].Type = NAR_DISKTYPE_RAW;
                        }
                        
                    }
                    else {
                        printf("Can't get drive layout for disk %s\n", PHNAME);
                    }
                    
                    if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, 0, 0, DGEX, DGEXSize, &HELL, 0)) {
                        Result.Data[DiskIndex].Size = DGEX->DiskSize.QuadPart;
                    }
                    else {
                        printf("Can't get drive layout for disk %s\n", PHNAME);
                    }
                    CloseHandle(Disk);
                }
                else {
                    printf("Can't open disk %s\n", PHNAME);
                }
                
            }
            
            DiskIndex++;
            sprintf(Target, "Disk %i", DiskIndex);
            
        }
        else {
            break;
        }
        
    }
    
    
    FreeFileRead(File);
    
    VirtualFree(Memory, 0, MEM_RELEASE);
    Result.Count = DiskIndex;
    return Result;
    
    ERROR_TERMINATE:
    FreeFileRead(File);
    VirtualFree(Memory, 0, MEM_RELEASE);
    
    DiskIndex = 0;
    Result.Count = 0;
    return Result;
    
}


//Returns # of volumes detected
data_array<volume_information>
NarGetVolumes() {
    
    data_array<volume_information> Result = { 0,0 };
    char VolumeString[] = " :\\";
    char WindowsLetter = 'C';
    {
        char WindowsDir[512];
        GetWindowsDirectoryA(WindowsDir, 512);
        WindowsLetter = WindowsDir[0];
    }
    
    DWORD Drives = GetLogicalDrives();
    
    for (int CurrentDriveIndex = 0; CurrentDriveIndex < 26; CurrentDriveIndex++) {
        
        if (Drives & (1 << CurrentDriveIndex)) {
            
            VolumeString[0] = 'A' + (char)CurrentDriveIndex;
            ULARGE_INTEGER TotalSize = { 0 };
            volume_information T = { 0 };
            
            GetDiskFreeSpaceExA(VolumeString, 0, &TotalSize, 0);
            T.Letter = 'A' + CurrentDriveIndex;
            T.Size = NarGetVolumeSize(T.Letter);
            T.Bootable = (WindowsLetter == T.Letter);
            T.DiskType = NarGetVolumeDiskType(T.Letter);
            T.DiskID = NarGetVolumeDiskID(T.Letter);
            
            Result.Insert(T);
            
        }
        
    }
    
    return Result;
    
}


ULONGLONG
NarGetDiskUnallocatedSize(int DiskID) {
    data_array<disk_information> DIArray = NarGetDisks();
    if (DIArray.Data == NULL) return 0;
    ULONGLONG Result = 0;
    
    int DiskIndexAtArray = -1;
    for (unsigned int i = 0; i < DIArray.Count; i++) {
        if (DIArray.Data[i].ID == DiskID) DiskIndexAtArray = i;
    }
    
    if (DiskIndexAtArray != -1) {
        Result = DIArray.Data[DiskIndexAtArray].UnallocatedGB * 1024 * 1024 * 1024;
    }
    else {
        printf("Couldn't find disk %i\n", DiskID);
        Result = 0;
    }
    
    FreeDataArray(&DIArray);
    return Result;
    
}

LONGLONG
NarGetDiskTotalSize(int DiskID) {
    
    ULONGLONG Result = 0;
    DWORD Temp = 0;
    char StrBuffer[128];
    sprintf(StrBuffer, "\\\\?\\PhysicalDrive%i", DiskID);
    
    unsigned int DGEXSize = 1024 * 4;
    DISK_GEOMETRY_EX* DGEX = (DISK_GEOMETRY_EX*)malloc(DGEXSize);
    if (DGEX == NULL) {
        // TODO(Batuhan): Failed to allocate memory
    }
    
    HANDLE Disk = CreateFileA(StrBuffer,
                              GENERIC_WRITE | GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    
    if (Disk != INVALID_HANDLE_VALUE) {
        if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, 0, 0, DGEX, DGEXSize, &Temp, 0)) {
            Result = DGEX->DiskSize.QuadPart;
        }
        else {
            printf("Failed to call DeviceIoControl with IOCTL_DISK_GET_DRIVE_GEOMETRY_EX parameter for disk %i\n", DiskID);
        }
    }
    else {
        printf("Can't open %s\n", StrBuffer);
    }
    
    CloseHandle(Disk);
    free(DGEX);
    
    return Result;
    
}


BOOLEAN
NarCreateCleanGPTBootablePartition(int DiskID, int VolumeSizeMB, int EFISizeMB, int RecoverySizeMB, char Letter) {
    //TODO win10 MSR partition created by default, take it as parameter and format accordingly.
    
    char Buffer[2096];
    memset(Buffer, 0, 2096);
    
    {
        ULONGLONG TotalSize = ((ULONGLONG)VolumeSizeMB * 1024 * 1024) + ((ULONGLONG)EFISizeMB * 1024 * 1024) + ((ULONGLONG)RecoverySizeMB * 1024 * 1024);
        if (NarGetDiskTotalSize(DiskID) < TotalSize) {
            printf("Can't fit bootable partition and it's extra partitions to disk %i\n", DiskID);
        }
    }
    
    sprintf(Buffer, ""
            "select disk %i\n" // ARG0 DiskID
            "clean\n"
            "convert gpt\n" // format disk as gpt
            "select partition 1\n"
            "delete partition override\n"
            "create partition primary size = %i\n" // recovery partition, ARG1 RecoveryPartitionSize
            "assign letter R\n"
            "format fs = ntfs quick\n"
            "remove letter R\n"
            "set id = \"de94bba4-06d1-4d40-a16a-bfd50179d6ac\"\n"
            "gpt attributes=0x8000000000000001\n"
            "create partition efi size = %i\n" //efi partition, ARG3 EFIPartitionSize
            //"format fs=fat32 quick\n"
            "create partition msr size = 16\n" // win10
            "create partition primary size = %i\n" // ARG4 PrimaryPartitionSize
            "assign letter = %c\n" // ARG5 VolumeLetter
            "format fs = \"ntfs\" quick\n"
            "exit\n", DiskID, RecoverySizeMB, EFISizeMB, VolumeSizeMB, Letter);
    
    char InputFN[] = "NARDPINPUT";
    if (NarDumpToFile(InputFN, Buffer, strlen(Buffer))) {
        sprintf(Buffer, "diskpart /s %s", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    
    return FALSE;
    
}

BOOLEAN
NarCreateCleanGPTPartition(int DiskID, int VolumeSizeMB, char Letter) {
    char Buffer[2096];
    memset(Buffer, 0, 2096);
    
    
    // Convert MB to byte
    if (NarGetDiskTotalSize(DiskID) < (ULONGLONG)VolumeSizeMB * 1024 * 1024) {
        printf("Not enough space to create partition\n");
        return FALSE;
    }
    
    
    
    sprintf(Buffer, ""
            "select disk %i\n"
            "clean\n"
            "convert gpt\n" // format disk as gpt
            "create partition primary size = %i\n"
            "assign letter = %c\n"
            "format fs = \"ntfs\" quick\n"
            "exit\n", DiskID, VolumeSizeMB, Letter);
    
    
    char InputFN[] = "NARDPINPUT";
    if (NarDumpToFile(InputFN, Buffer, strlen(Buffer))) {
        sprintf(Buffer, "diskpart /s %s", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    
    return FALSE;
}


BOOLEAN
NarCreateCleanMBRPartition() {
    printf("NarCreateCleanMBRPartition IS NOT IMPLEMENTED\n");
    return FALSE;
}

BOOLEAN
NarCreateCleanMBRBootPartition() {
    printf("NarCreateCleanMBRBootPartition IS NOT IMPLEMENTED\n");
    return FALSE;
}


#if 0
BOOLEAN
RestoreSystemPartitions(restore_inf* Inf) {
    
    GUID GDAT = { 0 }; // basic data partition
    GUID GMSR = { 0 }; // microsoft reserved partition guid
    GUID GEFI = { 0 }; // efi partition guid
    GUID GREC = { 0 }; // recovery partition guid
    
    StrToGUID("{ebd0a0a2-b9e5-4433-87c0-68b6b72699c7}", &GDAT);
    StrToGUID("{e3c9e316-0b5c-4db8-817d-f92df00215ae}", &GMSR);
    StrToGUID("{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}", &GEFI);
    StrToGUID("{de94bba4-06d1-4d40-a16a-bfd50179d6ac}", &GREC);
    
    DWORD BS = 1024;
    DWORD T;
    DRIVE_LAYOUT_INFORMATION_EX* DL = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(BS);
    
    BOOLEAN Result = FALSE;
    
    char DiskPath[32];
    //sprintf(DiskPath, "\\\\?\\PhysicalDrive%i", Inf->DiskID);
    printf("Opening disk : %s\n", DiskPath);
    
    HANDLE Disk = CreateFileA(DiskPath, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (Disk != INVALID_HANDLE_VALUE) {
        
        if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DL, BS, &T, 0)) {
            
            for (int k = 0; k < DL->PartitionCount; k++) {
                PARTITION_INFORMATION_EX* PI = &DL->PartitionEntry[k];
                
                if (PI->PartitionStyle == PARTITION_STYLE_GPT) {
                    
                    if (IsEqualGUID(PI->Gpt.PartitionType, GEFI)) {
                        //Efi system partition
                        printf("EFI partition detected\n");
                        printf("Partition offset %I64d\n", PI->StartingOffset.QuadPart);
                        std::wstring EFIFNAME = GenerateSystemPartitionFileName(Inf->SrcLetter);
                        HANDLE SP = CreateFileW(EFIFNAME.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
                        if (SP != INVALID_HANDLE_VALUE) {
                            LARGE_INTEGER NewFilePointer = { 0 };
                            SetFilePointerEx(Disk, PI->StartingOffset, &NewFilePointer, 0);
                            
                            if (NewFilePointer.QuadPart != PI->StartingOffset.QuadPart) {
                                printf("Can't set file pointer to EFI starting offset\n");
                            }
                            else {
                                if (!CopyData(SP, Disk, PI->PartitionLength.QuadPart)) {
                                    printf("Can't copy EFI data to disk, file : %S\n", EFIFNAME.c_str());
                                }
                            }
                            CloseHandle(SP);
                        }
                    }
                    
                    if (IsEqualGUID(PI->Gpt.PartitionType, GMSR)) {
                        //found MSR
                        printf("MSR partition detected\n");
                        
                        std::wstring MSRFNAME = GenerateMSRFileName(Inf->SrcLetter);
                        HANDLE SP = CreateFileW(MSRFNAME.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
                        if (SP != INVALID_HANDLE_VALUE) {
                            LARGE_INTEGER NewFilePointer = { 0 };
                            SetFilePointerEx(Disk, PI->StartingOffset, &NewFilePointer, 0);
                            if (NewFilePointer.QuadPart == PI->StartingOffset.QuadPart) {
                                if (!CopyData(SP, Disk, PI->PartitionLength.QuadPart)) {
                                    printf("Cant copy MSR partition\n");
                                }
                            }
                            else {
                                printf("Can't set file pointer to MSR offset\n");
                            }
                        }
                        else {
                            printf("Can't open msr to write, file : %S\n", MSRFNAME.c_str());
                            Result = FALSE;
                        }
                        CloseHandle(SP);
                    }
                    
                    if (IsEqualGUID(PI->Gpt.PartitionType, GREC)) {
                        //Rec partition
                        printf("Found recovery partition\n");
                        //std::wstring RECFNAME = GenerateRECFileName(Inf->SrcLetter);
                        //HANDLE SP = CreateFileW(RECFNAME.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
                        //if (SP != INVALID_HANDLE_VALUE) {
                        //    LARGE_INTEGER NewFilePointer = { 0 };
                        //    SetFilePointerEx(Disk, PI->StartingOffset, &NewFilePointer, 0);
                        //    if (NewFilePointer.QuadPart == PI->StartingOffset.QuadPart) {
                        //        if (!CopyData(SP, Disk, PI->PartitionLength.QuadPart)) {
                        //            printf("Cant copy recovery partition\n");
                        //        }
                        //    }
                        //    else {
                        //        printf("Can't set file pointer to recovery offset\n");
                        //    }
                        //}
                        //else {
                        //    printf("Can't open recovery to write\n", RECFNAME.c_str());
                        //    Result = FALSE;
                        //}
                        //CloseHandle(SP);
                        
                        
                    }
                    
                }
                if (PI->PartitionStyle == PARTITION_STYLE_RAW) {
                    printf("ERROR : Raw partition");
                }
                
            }
            
            
        }
        else {
            printf("DeviceIOControl get drive layout failed err => %i\n", GetLastError());
            Result = FALSE;
        }
        
        CloseHandle(Disk);
        
    }
    else {
        printf("Unable to open disk, error => %i\n", GetLastError());
        Result = FALSE;
    }
    
    free(DL);
    
    return TRUE;
}
#endif

inline BOOLEAN
NarIsOSVolume(char Letter) {
    char windir[256];
    GetWindowsDirectoryA(windir, 256);
    return windir[0] == Letter;
}

/*
Returns:
'R'aw
'M'BR
'G'PT
*/
inline int
NarGetVolumeDiskType(char Letter) {
    char VolPath[512];
    char Vol[] = "!:\\";
    Vol[0] = Letter;
    
    int Result = NAR_DISKTYPE_RAW;
    DWORD BS = 1024 * 8; //8 KB
    DWORD T = 0;
    
    void* Buf = VirtualAlloc(0, 2 * BS, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    VOLUME_DISK_EXTENTS* Ext = (VOLUME_DISK_EXTENTS*)(Buf);
    DRIVE_LAYOUT_INFORMATION_EX* DL = (DRIVE_LAYOUT_INFORMATION_EX*)((char*)Buf + BS);
    
    
    
    GetVolumeNameForVolumeMountPointA(Vol, VolPath, 512);
    VolPath[strlen(VolPath) - 1] = '\0'; //Remove trailing slash
    HANDLE Drive = CreateFileA(VolPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    HANDLE Disk = INVALID_HANDLE_VALUE;
    
    if (Drive != INVALID_HANDLE_VALUE) {
        
        if (DeviceIoControl(Drive, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0, Ext, BS, &T, 0)) {
            wchar_t DiskPath[512];
            // L"\\\\?\\PhysicalDrive%i";
            wsprintfW(DiskPath, L"\\\\?\\PhysicalDrive%i", Ext->Extents[0].DiskNumber);
            Disk = CreateFileW(DiskPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
            if (Disk != INVALID_HANDLE_VALUE) {
                
                if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DL, BS, &T, 0)) {
                    if (DL->PartitionStyle == PARTITION_STYLE_MBR) {
                        Result = NAR_DISKTYPE_MBR;
                    }
                    if (DL->PartitionStyle == PARTITION_STYLE_GPT) {
                        Result = NAR_DISKTYPE_GPT;
                    }
                    if (DL->PartitionStyle == PARTITION_STYLE_RAW) {
                        Result = NAR_DISKTYPE_RAW;
                    }
                }
                else {
                    printf("DeviceIOControl GET_DRIVE_LAYOUT_EX failed\n");
                    DisplayError(GetLastError());
                }
                
            }
            else {
                printf("Can't open disk as file\n");
                //TODO(Batuhan): Ccan't open disk as file
            }
            
        }
        
        
    }
    else {
        //TODO(Batuhan): can't open drive as file
        printf("Cant open drive %c as file\n", Letter);
    }
    
    VirtualFree(Buf, 0, MEM_RELEASE);
    CloseHandle(Drive);
    CloseHandle(Disk);
    return Result;
}


inline unsigned char
NarGetVolumeDiskID(char Letter) {
    
    wchar_t VolPath[512];
    wchar_t Vol[] = L"!:\\";
    Vol[0] = Letter;
    
    int Result = NAR_INVALID_DISK_ID;
    DWORD BS = 1024 * 2; //1 KB
    DWORD T = 0;
    
    VOLUME_DISK_EXTENTS* Ext = (VOLUME_DISK_EXTENTS*)VirtualAlloc(0, BS, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    
    if(NarGetVolumeGUIDKernelCompatible((wchar_t)Letter,VolPath)){
        
        HANDLE Drive = CreateFileW(VolPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        
        if (Drive != INVALID_HANDLE_VALUE) {
            
            if (DeviceIoControl(Drive, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0, Ext, BS, &T, 0)) {
                wchar_t DiskPath[512];
                // L"\\\\?\\PhysicalDrive%i";
                Result = Ext->Extents[0].DiskNumber;
            }
            else {
                printf("DeviceIoControl failed with argument VOLUME_GET_VOLUME_DISK_EXTENTS for volume %c\n", Letter);
            }
            
        }
        else {
            printf("Cant open drive %c as file\n", Letter);
        }
    
        CloseHandle(Drive);

    }
    else{
        printf("Couldnt get kernel compatible volume GUID\n");
        DisplayError(GetLastError());
    }
    
    VirtualFree(Ext, 0, MEM_RELEASE);
    
    return Result;
}

inline BOOLEAN
NarIsFullBackup(int Version) {
    return Version < 0;
}

inline std::wstring
GenerateMetadataName(wchar_t Letter, int Version) {
    std::wstring Result = WideMetadataFileNameDraft;
    Result += Letter;
    if (Version == NAR_FULLBACKUP_VERSION) {
        Result += L"FULL";
    }
    else {
        Result += std::to_wstring(Version);
    }
    //printf("Generated metadata name %S\n", Result.c_str());
    return Result;
}


inline BOOLEAN
NarSetFilePointer(HANDLE File, ULONGLONG V) {
    LARGE_INTEGER MoveTo = { 0 };
    MoveTo.QuadPart = V;
    LARGE_INTEGER NewFilePointer = { 0 };
    SetFilePointerEx(File, MoveTo, &NewFilePointer, FILE_BEGIN);
    return MoveTo.QuadPart == NewFilePointer.QuadPart;
}



HANDLE
NarOpenVolume(char Letter) {
    char VolumePath[512];
    sprintf(VolumePath, "\\\\.\\%c:", Letter);
    
    HANDLE Volume = CreateFileA(VolumePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
    if (Volume != INVALID_HANDLE_VALUE) {
        
        if (DeviceIoControl(Volume, FSCTL_LOCK_VOLUME, 0, 0, 0, 0, 0, 0)) {
            
        }
        else {
            // NOTE(Batuhan): this isnt an error, tho prohibiting volume access for other processes would be great.
            printf("Couldn't lock volume %c, returning INVALID_HANDLE_VALUE\n", Letter);
        }
        
        
        if (DeviceIoControl(Volume, FSCTL_DISMOUNT_VOLUME, 0, 0, 0, 0, 0, 0)) {
            
        }
        else {
            printf("Couldnt dismount volume\n");
        }
        
        
    }
    else {
        printf("Couldn't open volume %c\n", Letter);
        DisplayError(GetLastError());
    }
    
    return Volume;
}

void
NarCloseVolume(HANDLE V) {
    DeviceIoControl(V, FSCTL_UNLOCK_VOLUME, 0, 0, 0, 0, 0, 0);
    CloseHandle(V);
}

inline std::wstring
GenerateBinaryFileName(wchar_t Letter, int Version) {
    std::wstring Result = WideBackupFileNameDraft;
    Result += (Letter);
    if (Version == NAR_FULLBACKUP_VERSION) {
        Result += L"FULL";
    }
    else {
        Result += std::to_wstring(Version);
    }
    //printf("Generated binary file name %S\n", Result.c_str());
    return Result;
}

inline std::string
GenerateMetadataName(char Letter, int Version) {
    std::string Result = MetadataFileNameDraft;
    Result += (Letter);
    if (Version == NAR_FULLBACKUP_VERSION) {
        Result += "FULL";
    }
    else {
        Result += std::to_string(Version);
    }
    return Result;
}

inline std::wstring
GenerateLogFileName(wchar_t Letter, int Version) {
    std::wstring Result = L"LOGFILE_";
    Result += (Letter);
    if (Version == NAR_FULLBACKUP_VERSION) {
        Result += L"FULL";
    }
    else {
        Result += std::to_wstring(Version);
    }
    return Result;
}

// Volume optional, might pass INVALID_HANDLE_VALUE
BOOLEAN
RestoreVersionWithoutLoop(restore_inf R, BOOLEAN RestoreMFT, HANDLE Volume) {
    
    BOOLEAN IsVolumeLocal = (Volume == INVALID_HANDLE_VALUE); // Determine if we should close volume handle at end of function
    
    if (R.RootDir.length() > 0) {
        if (R.RootDir[R.RootDir.length() - 1] != L'\\') {
            R.RootDir += L"\\";
        }
    }
    if (Volume == INVALID_HANDLE_VALUE) {
        printf("Volume handle was invalid for backup ID %i, creating new handle\n", R.Version);
        Volume = NarOpenVolume((char)R.TargetLetter);
    }
    
    backup_metadata_ex* BMEX = InitBackupMetadataEx(R.SrcLetter, R.Version, R.RootDir);
    if (BMEX == NULL) {
        printf("Couldn't init backupmetadata\n");
        return FALSE;
    }
    printf("Initialized backup metadata for version %i\n", BMEX->M.Version);
    
    
    int CopyErrorsOccured = 0;
    
    std::wstring BinaryFileName = R.RootDir;
    BinaryFileName += GenerateBinaryFileName(R.SrcLetter, R.Version);
    
    
    HANDLE RegionsFile = CreateFile(BinaryFileName.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    
    if (Volume != INVALID_HANDLE_VALUE && RegionsFile != INVALID_HANDLE_VALUE) {
        
        for (int i = 0; i < BMEX->RegionsMetadata.Count; i++) {
            
            if (NarSetFilePointer(Volume, (ULONGLONG)BMEX->RegionsMetadata.Data[i].StartPos * (ULONGLONG)BMEX->M.ClusterSize)) {
                
                if (!CopyData(RegionsFile, Volume, (ULONGLONG)BMEX->RegionsMetadata.Data[i].Len * BMEX->M.ClusterSize)) {
                    printf("Error occured @ %i'th index, total of %i\n", i, BMEX->RegionsMetadata.Count);
                    CopyErrorsOccured++;
                    break;
                }
            }
            else {
                printf("Couldn't set file pointer\n");
            }
        }
        
    }
    else {
        // NOTE(Batuhan): couldnt open either regionsfile or volume, maybe both
        if (Volume == INVALID_HANDLE_VALUE) {
            printf("Volume handle is invalid: %c\n", R.TargetLetter);
        }
        if (RegionsFile == INVALID_HANDLE_VALUE) {
            printf("Couldn't open regions binary file: %S\n", BinaryFileName.c_str());
        }
        CopyErrorsOccured++;
    }
    
    if (CopyErrorsOccured) {
        printf("%i errors occured during copy operation\n", CopyErrorsOccured);
    }
    
    if (RestoreMFT && BMEX->M.Version != NAR_FULLBACKUP_VERSION) {
        // TODO(Batuhan): handle error returned from this function
        
        if (NarRestoreMFT(BMEX, Volume)) {
            printf("MFT Restored\n");
        }
        else {
            printf("Error occured while restoring MFT\n");
        }
    }
    
    
    //NOTE(BATUHAN): Zero out fullbackup's MFT region if target version isnt fb, since its mft will be overwritten that shouldnt be a problem
    //Later, I should replace this code with smt that detects MFT regions from fullbackup metadata, then excudes it from it, so we dont have to zero it after at all.
    //For testing purpose this should do the work.
#if 1
    if (!RestoreMFT && BMEX->M.Version != NAR_FULLBACKUP_VERSION) {
        
        HANDLE BMFile = CreateFile(BMEX->FilePath.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
        if (BMFile != INVALID_HANDLE_VALUE) {
            
            if (NarSetFilePointer(BMFile, BMEX->M.Offset.MFTMetadata)) {
                
                data_array<nar_record> MFTLCN = { 0 };
                MFTLCN.Data = (nar_record*)malloc(BMEX->M.Size.MFTMetadata);
                MFTLCN.Count = BMEX->M.Size.MFTMetadata / sizeof(nar_record);
                
                if (MFTLCN.Data) {
                    DWORD BytesRead = 0;
                    if (ReadFile(BMFile, MFTLCN.Data, BMEX->M.Size.MFTMetadata, &BytesRead, 0) && BytesRead == BMEX->M.Size.MFTMetadata) {
                        printf("Warning, experimental feature, may cause severe usage of memory\n");
                        
                        for (int i = 0; i < MFTLCN.Count; i++) {
                            ULONGLONG LEN = (ULONGLONG)MFTLCN.Data[i].Len * (ULONGLONG)BMEX->M.ClusterSize;
                            void* Buffer = malloc(LEN);
                            memset(Buffer, 0, LEN);
                            
                            NarSetFilePointer(Volume, (ULONGLONG)MFTLCN.Data[i].StartPos * (ULONGLONG)BMEX->M.ClusterSize);
                            
                            if (WriteFile(Volume, Buffer, LEN, &BytesRead, 0) && BytesRead == LEN) {
                                //printf("Starting clusternumber %i\t", MFTLCN.Data[i].StartPos);
                                //printf("Zeroed from %I64d, with lenght %I64d\n", (ULONGLONG)MFTLCN.Data[i].StartPos * (ULONGLONG)BMEX->M.ClusterSize, LEN);
                            }
                            else {
                                //printf("Couldnt zero specific MFT region, contiuning operation\n");
                            }
                            
                            free(Buffer);
                            
                        }
                        
                    }
                    else {
                        printf("Couldnt read mftlcn metadata from backup metadata file\n");
                    }
                    
                }
                else {
                    printf("Couldnt allocate memory for MFTLCN, cant zero out fullbackup's MFT regions\n");
                }
                
                FreeDataArray(&MFTLCN);
            }
            else {
                printf("Cant set file pointer to MFT LCN metadata at backup_metadata file\n");
            }
            
            
        }
        else {
            printf("Cant open backup metadata file, cant zero out fullbackup's MFT regions\n");
        }
        
        
        CloseHandle(BMFile);
        
    }
#endif
    
    CloseHandle(RegionsFile);
    if (IsVolumeLocal) { NarCloseVolume(Volume); }
    
    FreeBackupMetadataEx(BMEX);
    
    return CopyErrorsOccured == 0;
}



data_array<nar_record>
NARDEBUGResolveShadow(nar_record Src, nar_record* Shadow, int N, int* NewN) {
    data_array<nar_record> Result = { 0 };
    
    if (!NewN) {
        return data_array<nar_record>{ 0, 0 };
    }
    
    int i = 0;
    BOOLEAN TerminateOp = FALSE;
    
    while (TRUE) {
        
        if (i == N || TerminateOp) {
            if (Src.Len != 0) {
                Result.Insert(Src);
            }
            break;
        }
        
        rec_or O = GetOrientation(Src, Shadow[i]);
        if (O == LEFT) {
            TerminateOp = TRUE;
        }
        if (O == RIGHT) {
            //That shouldnt happen, caller should pass Shadow[0] as first colliding region, but in case happens just try to find manually.
            i++;
        }
        if (O == SPLIT) {
            // Append left side to result, since it cant collide with any more regions
            // Update src, make it right side of the splitted region, then keep iterating Shadow
            
            UINT32 LeftStart = Src.StartPos;
            UINT32 LeftLen = Shadow[i].StartPos - Src.StartPos;
            
            UINT32 NewStartPos = Shadow[i].StartPos + Shadow[i].Len;
            Src.StartPos = NewStartPos;
            Src.Len = Src.StartPos + Src.Len - NewStartPos;
            
            Result.Insert(nar_record{ LeftStart, LeftLen });
            
            i++;
        }
        if (O == OVERRUN) {
            Src = nar_record{ 0,0 };
            TerminateOp = TRUE;
        }
        if (O == COLLISION) {
            
            UINT32 SrcEndPoint = Src.StartPos + Src.Len;
            UINT32 ShadowEndPoint = Shadow[i].StartPos + Shadow[i].Len;
            
            if (SrcEndPoint >= ShadowEndPoint) {
                //Collision deleted left side of the old region, so shrink it from left side then iterate for next shadow
                
                UINT32 NewStart = ShadowEndPoint;
                UINT32 NewLen = Src.Len - (ShadowEndPoint - Src.StartPos);
                Src = nar_record{ NewStart, NewLen };
                i++;
                
            }
            else {
                //Collision deleted right side of the region, which also indicates end of operation, since next shadow cant collide with src.
                UINT32 NewStart = Src.StartPos;
                UINT32 NewLen = Shadow[i].StartPos - Src.StartPos;
                
                Result.Insert(nar_record{ NewStart, NewLen });
                TerminateOp = TRUE;
                
            }
            
            
        }
        
    }
    
    *NewN = i;
    
    return Result;
}

/*
Excludes regions E from source S, useful when determining non-used at the instant but changed disk sectors in past,
like deleted files, deleted file might have been changed in past, but at the instant it doesnt exist, so we better exclude it's regions
S and E MUST be sorted and merged beforehand
*/
data_array<nar_record>
NARDEBUGExcludeRegions(data_array<nar_record> S, data_array<nar_record> E) {
    
    if (!S.Data || !E.Data) {
        return data_array<nar_record>{0};
    }
    //NOTE(Batuhan): Assumption S and E sorted and merged beforehand
    
    int SI = 0;
    int EI = 0;
    data_array<nar_record> R = { 0 };
    nar_record TempRecord = { 0 };
    
    
    while (TRUE) {
        
        if (SI >= S.Count || EI >= E.Count) {
            break;
        }
        
        rec_or O = GetOrientation(S.Data[SI], E.Data[EI]);
        if (O == rec_or::SPLIT) {
            //If Ei splits Si, Ei+1 might split Si too, so rather than iterating to next S, just update it with truncated version and
            //recheck it with Ei
            UINT32 LeftStart = S.Data[SI].StartPos;
            UINT32 LeftLen = E.Data[EI].StartPos - S.Data[SI].StartPos;
            
            R.Insert(nar_record{ LeftStart,LeftLen });
            
            UINT32 RightStartPos = E.Data[EI].StartPos + E.Data[EI].Len;
            UINT32 RightLen = S.Data[SI].StartPos + S.Data[SI].Len - RightStartPos;
            S.Data[SI] = nar_record{ RightStartPos, RightLen };
            
            EI++;
            
            
        }
        if (O == rec_or::LEFT) {
            R.Insert(S.Data[SI]);
            SI++;
        }
        if (O == rec_or::RIGHT) {
            EI++;
        }
        if (O == rec_or::OVERRUN) {
            SI++;
        }
        if (O == rec_or::COLLISION) {
            
            data_array<nar_record> T = NARDEBUGResolveShadow(S.Data[SI], &E.Data[EI], E.Count - EI, &EI);
            Append(&R, T);
            SI++;
            
        }
        
    }
    
    return R;
    
}


// Volume optional, might pass INVALID_HANDLE_VALUE
BOOLEAN
RestoreDiffVersion(restore_inf R, HANDLE Volume) {
    BOOLEAN Result = TRUE;
    BOOLEAN IsVolumeLocal = (Volume == INVALID_HANDLE_VALUE);
    
    if (IsVolumeLocal) {
        printf("Passed volume argument was invalid, creating new handle for %c\n", R.TargetLetter);
        Volume = NarOpenVolume(R.TargetLetter);
        if (Volume == INVALID_HANDLE_VALUE) {
            printf("Couldnt create local volume handle, terminating now\n");
            Result = FALSE;
        }
    }
    
    if (Result) {
        
        restore_inf SubVersionR = R;
        SubVersionR.Version = NAR_FULLBACKUP_VERSION;
        if (RestoreVersionWithoutLoop(SubVersionR, FALSE, Volume)) {
            Result = RestoreVersionWithoutLoop(R, TRUE, Volume);
        }
        else {
            printf("Couldn't restore to full backup\n");
        }
        
        if (IsVolumeLocal) {
            printf("Closing diff backup local volume handle\n");
            NarCloseVolume(Volume);
        }
    }
    
    return Result;
}


// Volume optional, might pass INVALID_HANDLE_VALUE
BOOLEAN
RestoreIncVersion(restore_inf R, HANDLE Volume) {
    
    if (R.Version == NAR_FULLBACKUP_VERSION && R.Version <= 0) {
        printf("Invalid version ID (%i) for incremental restore\n", R.Version);
        return FALSE;
    }
    
    BOOLEAN Result = TRUE;
    BOOLEAN IsVolumeLocal = (Volume == INVALID_HANDLE_VALUE);
    
    if (IsVolumeLocal) {
        printf("Passed volume argument was invalid, creating new handle for %c\n", R.TargetLetter);
        Volume = NarOpenVolume(R.TargetLetter);
        if (Volume == INVALID_HANDLE_VALUE) {
            printf("Couldnt create local volume handle, terminating now\n");
            Result = FALSE;
        }
    }
    
    
    
    // NOTE(Batuhan): restores to full first, then incrementally builds volume to version
    
    restore_inf SubVersionR;
    SubVersionR.RootDir = R.RootDir;
    SubVersionR.SrcLetter = R.SrcLetter;
    SubVersionR.TargetLetter = R.TargetLetter;
    SubVersionR.Version = NAR_FULLBACKUP_VERSION;
    // NOTE(Batuhan): restore to fullbackup first
    printf("Before fullbackup restore breakpoint\n");
    if (RestoreVersionWithoutLoop(SubVersionR, FALSE, Volume)) {
        printf("Successfully restored to fullbackup\n");
        
        for (int VersionID = 0; VersionID < R.Version; VersionID++) {
            SubVersionR.Version = VersionID;
            if (!RestoreVersionWithoutLoop(SubVersionR, FALSE, Volume)) {
                Result = FALSE;
                printf("Couldn't restore to incremental version %i\n", SubVersionR.Version);
            }
            
        }
        
        //NOTE(Batuhan):Restore target version with RestoreMFT flag on
        if (!RestoreVersionWithoutLoop(R, TRUE, Volume)) {
            printf("Couldn't restore to target version\n");
        }
        
    }
    else {
        Result = FALSE;
        printf("Couldn't restore to fullbackup\n");
    }
    
    return Result;
}


BOOLEAN
NarRestoreMFT(backup_metadata_ex* BMEX, HANDLE Volume) {
    if (!BMEX) {
        return FALSE;
    }
    
    // NOTE(Batuhan): check errors flags first
    if (BMEX->M.Errors.MFT || BMEX->M.Errors.MFTMetadata) {
        printf("Detected error flags for MFT data, terminating now\n");
        return FALSE;
    }
    int CopyErrorsOccured = 0;
    
    data_array<nar_record> MFTLCN = ReadMFTLCN(BMEX);
    HANDLE MFTFile = CreateFileW(BMEX->FilePath.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    
    if (MFTFile != INVALID_HANDLE_VALUE
        && Volume != INVALID_HANDLE_VALUE) {
        
        NarSetFilePointer(MFTFile, BMEX->M.Offset.MFT);
        
        for (int i = 0; i < MFTLCN.Count; i++) {
            if (NarSetFilePointer(Volume, (ULONGLONG)MFTLCN.Data[i].StartPos * (ULONGLONG)BMEX->M.ClusterSize)) {
                if (!CopyData(MFTFile, Volume, (ULONGLONG)MFTLCN.Data[i].Len * (ULONGLONG)BMEX->M.ClusterSize)) {
                    CopyErrorsOccured++;
                    break;
                }
            }
        }
        
    }
    else {
        if (MFTFile == INVALID_HANDLE_VALUE) {
            printf("Can't open mft file\n");
        }
        if (Volume == INVALID_HANDLE_VALUE) {
            printf("Volume argument is invalid\n");
        }
    }
    
    if (CopyErrorsOccured != 0) {
        printf("%i errors occured while restoring MFT to volume\n", CopyErrorsOccured);
    }
    
    CloseHandle(MFTFile);
    FreeDataArray(&MFTLCN);
    return !CopyErrorsOccured;
}

backup_metadata
ReadMetadata(wchar_t Letter, int Version, std::wstring RootDir) {
    if (RootDir.length() > 0) {
        if (RootDir[RootDir.length() - 1] != L'\\') {
            RootDir += L"\\";
        }
    }
    
    BOOLEAN ErrorOccured = 0;
    DWORD BytesOperated = 0;
    std::wstring FileName = RootDir;
    FileName += GenerateMetadataName(Letter, Version);
    
    backup_metadata BM = { 0 };
    HANDLE F = CreateFile(FileName.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (F != INVALID_HANDLE_VALUE) {
        if (ReadFile(F, &BM, sizeof(BM), &BytesOperated, 0) && BytesOperated == sizeof(BM)) {
            ErrorOccured = FALSE;
        }
        else {
            ErrorOccured = TRUE;
            printf("Unable to read metadata, read %i bytes instead of %i\n", BytesOperated, (UINT32)sizeof(BM));
            memset(&BM, 0, sizeof(BM));
            memset(&BM.Errors, 1, sizeof(BM.Errors)); // Set all error flags
        }
    }
    else {
        printf("Unable to open metadata file %S\n", FileName.c_str());
        memset(&BM, 0, sizeof(BM));
        memset(&BM.Errors, 1, sizeof(BM.Errors)); // Set all error flags
    }
    
    CloseHandle(F);
    return BM;
}

backup_metadata_ex*
InitBackupMetadataEx(wchar_t Letter, int Version, std::wstring RootDir) {
    
    
    if (RootDir.length() > 0) {
        if (RootDir[RootDir.length() - 1] != L'\\') {
            RootDir += L"\\";
        }
    }
    
    // NOTE(Batuhan): its easier to set this flag of in one space rather than
    
    BOOLEAN ErrorOccured = TRUE;
    
    backup_metadata_ex* BMEX = new backup_metadata_ex;
    
    std::wstring FileName = RootDir;
    FileName += GenerateMetadataName(Letter, Version);
    BMEX->FilePath = FileName;
    
    DWORD BytesOperated = 0;
    HANDLE File = CreateFile(FileName.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if (File != INVALID_HANDLE_VALUE) {
        ReadFile(File, &BMEX->M, sizeof(BMEX->M), &BytesOperated, 0);
        
        if (BytesOperated == sizeof(BMEX->M)) {
            
            if (NarSetFilePointer(File, BMEX->M.Offset.RegionsMetadata)) {
                
                BMEX->RegionsMetadata.Data = (nar_record*)malloc(BMEX->M.Size.RegionsMetadata);
                
                if (BMEX->RegionsMetadata.Data != NULL) {
                    BMEX->RegionsMetadata.Count = 0;
                    BytesOperated = 0;
                    
                    if (ReadFile(File, BMEX->RegionsMetadata.Data, BMEX->M.Size.RegionsMetadata, &BytesOperated, 0) && BytesOperated == BMEX->M.Size.RegionsMetadata) {
                        ErrorOccured = FALSE;
                        BMEX->RegionsMetadata.Count = BMEX->M.Size.RegionsMetadata / sizeof(nar_record);
                    }
                    else {
                        printf("Couldn't read regions metadata\n");
                        // NOTE(Batuhan): Couldn't read enough bytes for regions
                    }
                }
                
            }
            else {
                printf("Couldn't set file pointer to regions metadata offset\n");
                // NOTE(Batuhan): Couldn't set file pointer to regions metadata offset
            }
            
        }
        else {
            printf("Couldn't read backup metadata\n");
        }
    }
    else {
        printf("Couldn't open file %S\n", BMEX->FilePath.c_str());
    }
    if (ErrorOccured) {
        printf("Error occured while initializing metadata extended structure, freeing regions metadata memory\n");
    }
    
    CloseHandle(File);
    return BMEX;
}


data_array<nar_record>
ReadMFTLCN(backup_metadata_ex* BMEX) {
    if (!BMEX) {
        return { 0, 0 };
    }
    
    data_array<nar_record> Result = { 0 , 0 };
    HANDLE File = CreateFileW(BMEX->FilePath.c_str(), GENERIC_READ, 0, 0, OPEN_ALWAYS, 0, 0);
    if (File != INVALID_HANDLE_VALUE) {
        if (NarSetFilePointer(File, BMEX->M.Offset.MFTMetadata)) {
            DWORD BytesRead = 0;
            Result.Count = BMEX->M.Size.MFTMetadata / sizeof(nar_record);
            Result.Data = (nar_record*)malloc(BMEX->M.Size.MFTMetadata);
            if (Result.Data) {
                ReadFile(File, Result.Data, BMEX->M.Size.MFTMetadata, &BytesRead, 0);
                if (BytesRead == BMEX->M.Size.MFTMetadata) {
                    // NOTE(Batuhan): Success!
                }
                else {
                    printf("Can't read MFT metadata, read %ul bytes instead of %I64u\n", BytesRead, BMEX->M.Size.MFTMetadata);
                    free(Result.Data);
                    Result.Count = 0;
                }
            }
            else {
                printf("Can't allocate memory for MFT metadata size\n");
            }
        }
        else {
            printf("Can't set file pointer\n");
        }
    }
    
    CloseHandle(File);
    return Result;
}

ULONGLONG
NarGetVolumeSize(char Letter) {
    char Temp[] = "!:\\";
    Temp[0] = Letter;
    ULARGE_INTEGER L = { 0 };
    GetDiskFreeSpaceExA(Temp, 0, &L, 0);
    return L.QuadPart;
}

/*
Version: indicates full backup if <0
ClusterSize: default 4096
BackupRegions: Must have, this data determines how i must map binary data to the volume at restore operation

// NOTE(Batuhan): file that contains this struct contains:
-- RegionMetadata:
-- MFTMetadata: (optional)
-- MFT: (optional)
-- Recovery: (optional)

*/
BOOLEAN
SaveMetadata(char Letter, int Version, int ClusterSize, BackupType BT,
             data_array<nar_record> BackupRegions, HANDLE VSSHandle) {
    // TODO(Batuhan): convert letter to uppercase
    
    if (ClusterSize <= 0 || ClusterSize % 512 != 0) {
        return FALSE;
    }
    if (Letter < 'A' || Letter > 'Z') {
        return FALSE;
    }
    
    DWORD BytesWritten = 0;
    backup_metadata BM = { 0 };
    ULONGLONG BaseOffset = sizeof(BM);
    data_array<nar_record> MFTLCN = { 0 };
    
    BOOLEAN Result = FALSE;
    char StringBuffer[1024];
    std::string MetadataFilePath = GenerateMetadataName(Letter, Version);
    HANDLE MetadataFile = CreateFileA(MetadataFilePath.c_str(), GENERIC_WRITE | GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0);
    if (MetadataFile == INVALID_HANDLE_VALUE) {
        printf("Couldn't create metadata file : %s\n", MetadataFilePath.c_str());
        goto Exit;
    }
    
    // NOTE(Batuhan): Reserve first bytes for metadata struct
    WriteFile(MetadataFile, StringBuffer, sizeof(BM), &BytesWritten, 0);
    if (BytesWritten != sizeof(BM)) {
        printf("Couldn't reserve bytes for metadata struct, written %i instead of %i\n", BytesWritten, (int)sizeof(BM));
        goto Exit;
    }
    
    
    BM.MetadataInf.Size = sizeof(BM);
    BM.MetadataInf.Version = GlobalBackupMetadataVersion;
    
    BM.Version = Version;
    BM.ClusterSize = ClusterSize;
    BM.BT = BT;
    BM.Letter = Letter;
    BM.DiskType = NarGetVolumeDiskType(BM.Letter);
    
    BM.IsOSVolume = NarIsOSVolume(Letter);
    
    BM.VolumeSize = NarGetVolumeSize(BM.Letter);
    
    memset(&BM.Size, 0, sizeof(BM.Size));
    memset(&BM.Offset, 0, sizeof(BM.Offset));
    memset(&BM.Errors, 0, sizeof(BM.Errors));
    
    
    // NOTE(Batuhan): fill BM.Size struct
    
    // NOTE(Batuhan): Backup regions and it's metadata sizes
    if (BackupRegions.Count > 0) {
        BM.Size.RegionsMetadata = BackupRegions.Count * sizeof(nar_record);
        for (int i = 0; i < BackupRegions.Count; i++) {
            BM.Size.Regions += (ULONGLONG)BackupRegions.Data[i].Len * BM.ClusterSize;
        }
    }
    else {
        // NOTE(Batuhan): BackupRegions shouldnt have 0 entry
        BM.Errors.RegionsMetadata = TRUE;
        BM.Errors.Regions = TRUE;
        printf("BackupRegions shouldn't have 0 entry\n");
    }
    
    
    /*
    // NOTE(Batuhan): MFT metadata and it's binary sizes, checks if non-fullbackup, otherwise skips it
  Since it metadata file must contain MFT for non-full backups, we forwardly declare it's size here, then try to
  write it to the file, if any error occurs during this operation, BM.Size.MFT wil be corrected and marked as corrupt
  @BM.Errors.MFT
      */
    MFTLCN = GetMFTLCN(Letter, VSSHandle);
    
    if (MFTLCN.Count > 0) {
        BM.Size.MFTMetadata = MFTLCN.Count * sizeof(nar_record);
        for (int i = 0; i < MFTLCN.Count; i++) {
            BM.Size.MFT += (ULONGLONG)MFTLCN.Data[i].Len * (ULONGLONG)BM.ClusterSize;
        }
    }
    else {
        // NOTE(Batuhan): Couldn't fetch MFT LCN's
        BM.Errors.MFTMetadata = TRUE;
        BM.Errors.MFT = TRUE;
        printf("Couldn't fetch MFT LCN's\n");
    }
    
    
    
    
    {
        WriteFile(MetadataFile, BackupRegions.Data, BM.Size.RegionsMetadata, &BytesWritten, 0);
        if (BytesWritten != BM.Size.RegionsMetadata) {
            printf("Couldn't save regionsmetata to file\n");
            BM.Errors.RegionsMetadata = TRUE;
            BM.Size.RegionsMetadata = BytesWritten;
        }
    }
    
    
    // NOTE(Batuhan): since MFT size is > 0 if only non-full backup, we dont need to check Version < 0
    if (BM.Size.MFT && !BM.Errors.MFT && MFTLCN.Data != NULL) {
        //Save current file pointer in case of failure, so we can roll back to old pointer.
        ULONGLONG OldFilePointer = NarGetFilePointer(MetadataFile);
        if (WriteFile(MetadataFile, MFTLCN.Data, BM.Size.MFTMetadata, &BytesWritten, 0) && BytesWritten == BM.Size.MFTMetadata) {
            OldFilePointer = NarGetFilePointer(MetadataFile);
            
            if (AppendMFTFile(MetadataFile, VSSHandle, MFTLCN, ClusterSize)) {
                printf("Successfully appended MFT to metadata\n");
                // NOTE(Batuhan): successfully appended mft to the metadata file
            }
            else {
                BM.Errors.MFT = TRUE;
                if (NarTruncateFile(MetadataFile, OldFilePointer)) {
                    // NOTE(Batuhan): successfully truncated file, we are safe to set MFT size to 0
                    BM.Size.MFT = 0;
                }
                else {
                    // NOTE(Batuhan): couldnt even truncate file, so set size to how many bytes has written @AppendMFTFile
                    ULONGLONG NewFilePointer = NarGetFilePointer(MetadataFile);
                    BM.Size.MFT = OldFilePointer - NewFilePointer;
                }
            }
        }
        else {
            printf("Couldn't append MFTMetadata to file\n");
            // NOTE(Batuhan) : Couldnt append MFTMetadata to file
            BM.Errors.MFTMetadata = TRUE;
            BM.Errors.MFT = TRUE;
            
            // Try to truncate file to old position,
            if (NarTruncateFile(MetadataFile, OldFilePointer)) {
                BM.Size.MFTMetadata = 0;
            }
            else {
                ULONGLONG NewFilePointer = NarGetFilePointer(MetadataFile);
                BM.Size.MFTMetadata = OldFilePointer - NewFilePointer;
            }
            
        }
        
    }
    else {
        // NOTE(Batuhan): that's not an error
    }
    
    // NOTE(Batuhan): Recovery partition's data is stored on metadata only if it's both full and OS volume backup
    if (BM.IsOSVolume && BM.Version == NAR_FULLBACKUP_VERSION) {
        
        ULONGLONG OldFilePointer = NarGetFilePointer(MetadataFile);
        if (AppendRecoveryToFile(MetadataFile, BM.Letter)) {
            ULONGLONG CurrentFilePointer = NarGetFilePointer(MetadataFile);
            BM.Size.Recovery = CurrentFilePointer - OldFilePointer;
        }
        else {
            printf("Error occured while appending recovery partition to backup metadata file for volume %c\n", BM.Letter);
            ULONGLONG CurrentFilePointer = NarGetFilePointer(MetadataFile);
            BM.Size.Recovery = CurrentFilePointer - OldFilePointer;
            BM.Errors.Recovery = TRUE;
        }
        
        
        // TODO(Batuhan): Backup recovery partition
    }
    
    /*
    // NOTE(Batuhan): fill BM.Offset struct
    offset[n] = offset[n-1] + size[n-1]
  offset[0] = baseoffset = struct size
  */
    
    BM.Offset.RegionsMetadata = BaseOffset;
    BM.Offset.MFTMetadata = BM.Offset.RegionsMetadata + BM.Size.RegionsMetadata;
    BM.Offset.MFT = BM.Offset.MFTMetadata + BM.Size.MFTMetadata;
    BM.Offset.Recovery = BM.Offset.MFT + BM.Size.MFT;
    
    SetFilePointer(MetadataFile, 0, 0, FILE_BEGIN);
    WriteFile(MetadataFile, &BM, sizeof(BM), &BytesWritten, 0);
    if (BytesWritten == sizeof(BM)) {
        Result = TRUE;
    }
    
    Exit:
    CloseHandle(MetadataFile);
    FreeDataArray(&MFTLCN);
    return Result;
    
}

// NOTE(Batuhan): truncates file F to size TargetSize
BOOLEAN
NarTruncateFile(HANDLE F, ULONGLONG TargetSize) {
    
    LARGE_INTEGER MoveTo = { 0 };
    MoveTo.QuadPart = TargetSize;
    LARGE_INTEGER NewFilePointer = { 0 };
    SetFilePointerEx(F, MoveTo, &NewFilePointer, FILE_BEGIN);
    if (MoveTo.QuadPart == NewFilePointer.QuadPart) {
        return SetEndOfFile(F);
    }
    else {
        printf("Couldn't truncate file to %I64d, instead set it's end to %I64d\n", TargetSize, NewFilePointer.QuadPart);
    }
    
    return FALSE;

}

inline ULONGLONG
NarGetFileSize(const wchar_t* Path) {
    ULONGLONG Result = 0;
    HANDLE F = CreateFileW(Path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (F != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER L = { 0 };
        GetFileSizeEx(F, &L);
        Result = L.QuadPart;
    }
    CloseHandle(F);
    return Result;
}

inline ULONGLONG
NarGetFileSize(const char* Path) {
    ULONGLONG Result = 0;
    HANDLE F = CreateFileA(Path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (F != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER L = { 0 };
        GetFileSizeEx(F, &L);
        Result = L.QuadPart;
    }
    CloseHandle(F);
    return Result;
}

ULONGLONG
NarGetFilePointer(HANDLE F) {
    LARGE_INTEGER M = { 0 };
    LARGE_INTEGER N = { 0 };
    SetFilePointerEx(F, M, &N, FILE_CURRENT);
    return N.QuadPart;
}

BOOLEAN
RestoreRecoveryFile(restore_inf R) {
    GUID GREC = { 0 }; // recovery partition guid
    StrToGUID("{de94bba4-06d1-4d40-a16a-bfd50179d6ac}", &GREC);
    
    BOOLEAN Result = FALSE;
    DWORD BufferSize = 1024 * 1;
    
    DRIVE_LAYOUT_INFORMATION_EX* DL = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(BufferSize);
    HANDLE Disk = INVALID_HANDLE_VALUE;
    
    backup_metadata B = ReadMetadata(R.SrcLetter, NAR_FULLBACKUP_VERSION, R.RootDir);
    //TODO(Batuhan) check integrity of B
    std::wstring MFN = R.RootDir;
    MFN += GenerateMetadataName(R.SrcLetter, NAR_FULLBACKUP_VERSION);
    
    HANDLE MetadataFile = CreateFileW(MFN.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (MetadataFile != INVALID_HANDLE_VALUE) {
        
        int DiskID = NarGetVolumeDiskID(B.Letter);
        
        
        if (DiskID != NAR_INVALID_DISK_ID) {
            char DiskPath[512];
            sprintf(DiskPath, "\\\\?\\PhysicalDrive%i", DiskID);
            
            Disk = CreateFileA(DiskPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
            if (Disk != INVALID_HANDLE_VALUE) {
                
                DWORD Hell = 0;
                if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DL, BufferSize, &Hell, 0)) {
                    if (DL->PartitionStyle == PARTITION_STYLE_GPT) {
                        for (unsigned int PartitionIndex = 0; PartitionIndex < DL->PartitionCount; PartitionIndex++) {
                            PARTITION_INFORMATION_EX* PI = &DL->PartitionEntry[PartitionIndex];
                            
                            // NOTE(Batuhan): Finding recovery partition via GUID
                            if (IsEqualGUID(PI->Gpt.PartitionType, GREC)) {
                                if (PI->PartitionLength.QuadPart != B.Size.Recovery) {
                                    printf("Recovery partition size on disk and partition size on metadata doesnt match, on disk %I64d, on metadata %I64d\n", PI->PartitionLength.QuadPart, B.Size.Recovery);
                                }
                                NarSetFilePointer(Disk, PI->StartingOffset.QuadPart);
                                NarSetFilePointer(MetadataFile, B.Offset.Recovery);
                                if (CopyData(MetadataFile, Disk, B.Size.Recovery)) {
                                    Result = TRUE;
                                    //NOTE(Batuhan): Success
                                }
                                else {
                                    printf("Couldnt copy recovery file to disk\n");
                                }
                                
                            }
                            
                        }
                    }
                    
                }
                else {
                    printf("DeviceIoControl with argument IOCTL_DISK_GET_DRIVE_LAYOUT_EX failed for drive %i, cant restore recovery partition\n", B.Letter);
                }
                
            }
            else {
                printf("Couldnt open disk %s as file, cant restore recovery partition\n", DiskPath);
            }
            
        }
        else {
            printf("Couldnt find valid Disk ID for volume letter %c\n", B.Letter);
        }
    }
    else {
        printf("Cant open metadata file %S\n", MFN.c_str());
    }
    
    
    free(DL);
    CloseHandle(Disk);
    CloseHandle(MetadataFile);
    return Result;
}

BOOLEAN
AppendRecoveryToFile(HANDLE File, char Letter) {
    
    GUID GREC = { 0 }; // recovery partition guid
    StrToGUID("{de94bba4-06d1-4d40-a16a-bfd50179d6ac}", &GREC);
    
    BOOLEAN Result = FALSE;
    DWORD BufferSize = 1024 * 2; // 64KB
    
    char VolPath[128];
    sprintf(VolPath, "%c:\\", Letter);
    
    DRIVE_LAYOUT_INFORMATION_EX* DL = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(BufferSize);
    HANDLE Disk = INVALID_HANDLE_VALUE;
    int DiskID = NarGetVolumeDiskID(Letter);
    
    if (DiskID != NAR_INVALID_DISK_ID) {
        char DiskPath[512];
        sprintf(DiskPath, "\\\\?\\PhysicalDrive%i", DiskID);
        
        Disk = CreateFileA(DiskPath, GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        if (Disk != INVALID_HANDLE_VALUE) {
            
            DWORD Hell = 0;
            if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DL, BufferSize, &Hell, 0)) {
                
                if (DL->PartitionStyle == PARTITION_STYLE_GPT) {
                    for (unsigned int PartitionIndex = 0; PartitionIndex < DL->PartitionCount; PartitionIndex++) {
                        PARTITION_INFORMATION_EX* PI = &DL->PartitionEntry[PartitionIndex];
                        
                        // NOTE(Batuhan): Finding recovery partition via GUID
                        if (IsEqualGUID(PI->Gpt.PartitionType, GREC)) {
                            
                            NarSetFilePointer(Disk, PI->StartingOffset.QuadPart);
                            if (CopyData(Disk, File, PI->PartitionLength.QuadPart)) {
                                Result = TRUE;
                                //NOTE(Batuhan): Success
                            }
                            else {
                                printf("Couldnt restore recovery partition from backup metadata file\n");
                            }
                            
                        }
                        
                    }
                }
                
                
            }
            else {
                printf("DeviceIoControl with argument IOCTL_DISK_GET_DRIVE_LAYOUT_EX failed for drive %i, cant append recovery file to backup metadata\n", Letter);
                DisplayError(GetLastError());
            }
            
        }
        else {
            printf("Couldnt open disk %s as file, cant append recovery file to backup metadata\n", DiskPath);
        }
        
    }
    else {
        printf("Couldnt find valid Disk ID for volume letter %c\n", Letter);
    }
    
    free(DL);
    CloseHandle(Disk);
    return Result;
}

BOOLEAN
NarGetBackupsInDirectory(const wchar_t* Directory, backup_metadata* B, int BufferSize, int* FoundCount) {
    
    BOOLEAN Result = TRUE;
    
    wchar_t WildcardDir[280];
    wchar_t wstrbuffer[512];
    memset(wstrbuffer, 0, 512 * sizeof(wchar_t));
    
    wcscpy(WildcardDir, Directory);
    //So for some reason, directory string MUST end with * to be valid for iteration.
    wcscat(WildcardDir, L"\\*");
    
    WIN32_FIND_DATAW FDATA;
    HANDLE FileIterator = FindFirstFileW(WildcardDir, &FDATA);
    
    int BackupFound = 0;
    int MaxBackupCount = BufferSize / sizeof(*B);
    
    if (FileIterator != INVALID_HANDLE_VALUE) {
        
        while (FindNextFileW(FileIterator, &FDATA) != 0) {
            memset(wstrbuffer, 0, 512 * sizeof(wchar_t));
            
            //NOTE(Batuhan): Do not search for sub-directories, just skip folders.
            if (FDATA.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue;
            }
            
            if (BackupFound == MaxBackupCount) {
                Result = FALSE;
            }
            
            //printf("Found file %S\n", FDATA.cFileName);
            
            //NOTE(Batuhan): Compare file name for metadata draft prefix NAR_ , if prefix found, try to read it
            if (wcsncmp(FDATA.cFileName, WideMetadataFileNameDraft, wcslen(WideMetadataFileNameDraft)) == 0) {
                
                wcscpy(wstrbuffer, Directory);
                wcscat(wstrbuffer, L"\\");
                wcscat(wstrbuffer, FDATA.cFileName);
                
                HANDLE F = CreateFileW(wstrbuffer, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
                if (F != INVALID_HANDLE_VALUE) {
                    DWORD BR = 0;
                    if (ReadFile(F, &B[BackupFound], sizeof(*B), &BR, 0) && BR == sizeof(*B)) {
                        //NOTE(Batuhan): Since we just compared metadata file name draft, now, just compare full name, to determine its filename, since it might be corrupted
                        
                        std::wstring T = GenerateMetadataName((wchar_t)B[BackupFound].Letter, B[BackupFound].Version);
                        
                        if (wcscmp(FDATA.cFileName, T.c_str()) == 0) {
                            //NOTE(Batuhan): File name indicated from metadata and actual file name matches
                            //Even though metadatas match, actual binary data may not exist at all or even, metadata itself might be corrupted too, or missing. Check it
                            
                            //NOTE(Batuhan): check if actual binary data exists in path and valid in size
                            wcscpy(wstrbuffer, Directory);
                            wcscat(wstrbuffer, L"\\");
                            wcscat(wstrbuffer, GenerateBinaryFileName(B[BackupFound].Letter, B[BackupFound].Version).c_str());
                            if (PathFileExistsW(wstrbuffer)) {
                                if (NarGetFileSize(wstrbuffer) == B[BackupFound].Size.Regions) {
                                    BackupFound++;
                                    printf("Backup found %S\n", FDATA.cFileName);
                                }
                            }
                            
                        }
                        else {
                            //NOTE(Batuhan): File name indicated from metadata and acutla file name does NOT match.
                            memset(&B[BackupFound], 0, sizeof(*B));
                        }
                        
                    }
                    
                }
                
                CloseHandle(F);
            }
            
        }
        
    }
    else {
        printf("Cant iterate directory\n");
        Result = FALSE;
    }
    
    if (FoundCount) {
        *FoundCount = BackupFound;
    }
    
    FindClose(FileIterator);
    return TRUE;
}



BOOLEAN
AppendMFTFile(HANDLE File, HANDLE VSSHandle, data_array<nar_record> MFTLCN, int ClusterSize) {
    
    BOOLEAN Return = FALSE;
    
    if (File != INVALID_HANDLE_VALUE && VSSHandle != INVALID_HANDLE_VALUE) {
        
        LARGE_INTEGER MoveTo = { 0 };
        LARGE_INTEGER NewFilePointer = { 0 };
        BOOL Result = 0;
        DWORD BytesOperated = 0;
        for (unsigned int i = 0; i < MFTLCN.Count; i++) {
            MoveTo.QuadPart = (ULONGLONG)MFTLCN.Data[i].StartPos * (ULONGLONG)ClusterSize;
            
            Result = SetFilePointerEx(VSSHandle, MoveTo, &NewFilePointer, FILE_BEGIN);
            if (!SUCCEEDED(Result) || MoveTo.QuadPart != NewFilePointer.QuadPart) {
                printf("Set file pointer failed\n");
                printf("Tried to set pointer -> %I64d, instead moved to -> %I64d\n", MoveTo.QuadPart, NewFilePointer.QuadPart);
                printf("Failed at LCN : %i\n", i);
                DisplayError(GetLastError());
                goto Exit;
            }
            
            if (!CopyData(VSSHandle, File, (ULONGLONG)MFTLCN.Data[i].Len * (ULONGLONG)ClusterSize)) {
                printf("Error occured while copying MFT file\n");
                goto Exit;
            }
            
        }
        
        return TRUE;
    }
    else {
        if (File == INVALID_HANDLE_VALUE) {
            printf("File to append MFT isnt valid\n");
        }
        if (VSSHandle == INVALID_HANDLE_VALUE) {
            printf("VSSHandle was invalid [AppendMFTToFile]\n");
        }
    }
    
    Exit:
    FreeDataArray(&MFTLCN);
    return Return;
    
}


/*
Its not best way to initialize a struct
*/
LOG_CONTEXT*
NarLoadBootState() {
    
    LOG_CONTEXT* Result;
    BOOLEAN bResult = FALSE;
    Result = (LOG_CONTEXT*)malloc(sizeof(LOG_CONTEXT));
    memset(Result, 0, sizeof(LOG_CONTEXT));
    Result->Port = INVALID_HANDLE_VALUE;
    Result->ShutDown = NULL;
    Result->Thread = NULL;
    Result->CleaningUp = FALSE;
    Result->Volumes = { 0,0 };
    
    
    char FileNameBuffer[64];
    if (GetWindowsDirectoryA(FileNameBuffer, 64)) {
        
        strcat(FileNameBuffer, "\\");
        strcat(FileNameBuffer, NAR_BOOTFILE_NAME);
        HANDLE File = CreateFileA(FileNameBuffer, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
        if (File != INVALID_HANDLE_VALUE) {
            
            // Check aligment, not a great way to check integrity of file
            DWORD Size = GetFileSize(File, 0);
            if (Size > sizeof(nar_boot_track_data) * NAR_MAX_VOLUME_COUNT) {
                printf("Bootfile too large, might be corrupted\n");
            }
            else if (Size % sizeof(nar_boot_track_data) != 0) {
                printf("Bootfile is not aligned\n");
            }
            else {
                
                DWORD BR = 0;
                nar_boot_track_data* BootTrackData = (nar_boot_track_data*)malloc(Size);
                if (BootTrackData != NULL) {
                    
                    if (ReadFile(File, BootTrackData, Size, &BR, 0) && BR == Size) {
                        volume_backup_inf VolInf;
                        memset(&VolInf, 0, sizeof(VolInf));
                        int BootTrackDataCount = Size / sizeof(BootTrackData[0]);
                        
                        for (unsigned int i = 0; i < BootTrackDataCount; i++) {
                            
                            
                            bResult = InitVolumeInf(&VolInf, BootTrackData[i].Letter, (BackupType)BootTrackData[i].BackupType);
                            if (bResult) {
                                
                                
                                VolInf.FilterFlags.IsActive = TRUE;
                                VolInf.FilterFlags.SaveToFile = TRUE;
                                
                                VolInf.FullBackupExists = TRUE;
                                VolInf.CurrentLogIndex = BootTrackData[i].Version;
                                std::wstring LogFileName = GenerateLogFileName(VolInf.Letter, VolInf.CurrentLogIndex);
                                VolInf.LogHandle = CreateFileW(LogFileName.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
                                
                                if (VolInf.LogHandle != INVALID_HANDLE_VALUE) {
                                    // succ created file, we can continue logging.
                                    Result->Volumes.Insert(VolInf);
                                    
                                    bResult = TRUE;
                                    
                                }
                                else {
                                    printf("Couldnt open existing log file %S\n", LogFileName.c_str());
                                    DisplayError(GetLastError());
                                }
                                
                            }
                            else{
                                printf("Couldnt initialize volume backup inf for volume %c\n", BootTrackData[i].Letter);
                            }
                            
                        }
                        
                        
                    }
                    else {
                        printf("Couldnt read boot file, read %i, file size was %i\n", BR, Size);
                        DisplayError(GetLastError());
                    }
                    
                }
                else {
                    printf("Couldnt allocate memory for nar_boot_track_data \n");
                    BootTrackData = 0;
                }
                
                free(BootTrackData);
                
            }
            
        }
        else {
            // File doesnt exist
        }
        
        CloseHandle(File);
        
    }
    else {
        printf("Couldnt get windows directory, error %i\n", GetLastError());
    }
    
    if (!Result) {
        free(Result);
        Result = NULL;
    }
    
    return Result;
}


/*
    Saves the current program state into a file, so next time computer boots driver can recognize it and setup itself accordingly.
*/

BOOLEAN
NarSaveBootState(LOG_CONTEXT* CTX) {
    if (CTX == NULL && CTX->Volumes.Data == 0) return FALSE;
    
    BOOLEAN Result = TRUE;
    
    nar_boot_track_data Pack;
    
    char FileNameBuffer[64];
    if (GetWindowsDirectoryA(FileNameBuffer, 64)) {
        strcat(FileNameBuffer, "\\");
        strcat(FileNameBuffer, NAR_BOOTFILE_NAME);
        HANDLE File = CreateFileA(FileNameBuffer, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
        
        if (File != INVALID_HANDLE_VALUE) {
            
            for (unsigned int i = 0; i < CTX->Volumes.Count; i++) {
                Pack.Letter = 0;
                Pack.Version = -2;
                
                if (CTX->Volumes.Data[i].INVALIDATEDENTRY) {
                    
                    Pack.Letter = (char)CTX->Volumes.Data[i].Letter;
                    Pack.Version = CTX->Volumes.Data[i].CurrentLogIndex;
                    Pack.BackupType = (char)CTX->Volumes.Data[i].BT;
                    DWORD BR = 0;
                    
                    if (WriteFile(File, &Pack, sizeof(Pack), &BR, 0) && BR == sizeof(Pack)) {
                        
                    }
                    else {
                        Result = FALSE;
                        // TODO error
                    }
                    
                }
                
                continue;
                
            }
            
            
        }
        else {
            // GetWindowsdirectory failed
            Result = FALSE;
        }
        
        CloseHandle(File);
        
        
    }
    
    
    
    
    
    return Result;
    
}





#if 1

#define REGION(Start, End) nar_record{(Start), (End) - (Start)}


int
main(
     int argc,
     CHAR* argv[]
     ) {
    
    wchar_t test[500];
    
    auto metadata = ReadMetadata('C', 0, L"");
    
    int n = 2;
    INT64 Test = 0xFAEC;
    // 0x4000000000000000 sign mask
    Test = (Test & ~(0xFFFFFFFFFFFFFFFULL << (n * 8)));

    if ((Test >> ((n - 1) * 8 + 7)) & 1U) {
        Test = Test | ((0xFFFFFFFFFFFFFFFULL << (n * 8)));
    }

    printf("%I64d\n", Test);
    //CComPtr<IVssBackupComponents> ptr;
    
    if (TRUE) {
        //wchar_t* path = GetShadowPath(L"C:\\", ptr);

        HANDLE VSSHandle = CreateFileW(L"\\\\.\\C:", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_READ_ATTRIBUTES, 0);
        if (1) {

            if (VSSHandle != INVALID_HANDLE_VALUE) {
                GetMFTLCN('C', VSSHandle);
            }
            else {
                DebugBreak();
            }

        }
        else {
            
        }

        
    }
    else {
        DebugBreak();
    }

    
    
    return 0;

    union nar_long{
        char b[4];
        LONG l;
    };

    nar_long a;
    a.l = 0xaabbccdd;
    nar_long b;
    b = a;
    long number = 0xaabbccdd;


    
    {
        LOG_CONTEXT* CTX = NarLoadBootState();
        if (CTX != NULL) {
            printf("Succ loaded CTX\n");
        }
        else {
            printf("Couldnt load CTX\n");
        }
    }
    
    char STRB[512];
    GetWindowsDirectoryA(STRB, 512);
    strcat(STRB, "\\");
    strcat(STRB, NAR_BOOTFILE_NAME);
    HANDLE File = CreateFileA(STRB, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (File != INVALID_HANDLE_VALUE) {
        printf("Succ created file\n");
    }
    else {
        DisplayError(GetLastError());
    }
    CloseHandle(File);
    //system("net stop minispy");
    //system("net start minispy");
    
    HRESULT hResult = FALSE;
    
    HANDLE Port = INVALID_HANDLE_VALUE;
    
    hResult = FilterConnectCommunicationPort(MINISPY_PORT_NAME,
                                             0,
                                             0, 0,
                                             NULL, &Port);
    Sleep(10);
    
    if (!IS_ERROR(hResult)) {
        
        DWORD HELL = 0;
        WCHAR VolumeMountPoint[] = L"C:\\";
        WCHAR VolumeGUIDString[50];
        memset(&VolumeGUIDString[0], 0, 100);
        
        NAR_COMMAND MessageCommand;
        memset(&MessageCommand, 0, sizeof(MessageCommand));
        MessageCommand.Type = NarCommandType_AddVolume;
        if (!GetVolumeNameForVolumeMountPointW(VolumeMountPoint, &MessageCommand.VolumeGUIDStr[0], 50)) {
            printf("Couldnt get volume name from mount point\n");
        }
        
        MessageCommand.VolumeGUIDStr[1] = L'?';
        
        printf("%S\n", VolumeGUIDString);
        
        void* OutBuffer = malloc(Megabyte(1));
        hResult = FilterSendMessage(Port, &MessageCommand, sizeof(NAR_COMMAND), OutBuffer, Megabyte(1), &HELL);
        
        printf("Send message from user size\n");
        if (!SUCCEEDED(hResult)) {
            printf("Couldnt send message to filter: %i\n", hResult);
        }
        Sleep(20);
        
        hResult = FilterAttach(MINISPY_NAME, VolumeMountPoint, 0, 0, 0);
        if (SUCCEEDED(hResult) || hResult == ERROR_FLT_INSTANCE_NAME_COLLISION) {
            printf("Successfully attached to kernel\n");
        }
        else {
            printf("Can't attach filter\n");
            DisplayError(GetLastError());
            return 0;
        }
        
        
    }
    else {
        printf("Could not connect to filter: 0x%08x\n", hResult);
        DisplayError(GetLastError());
        return 0;
    }
    
    printf("Successfully setup driver, entering infinite loop, press q to exit.\n");
    while (1) {
        if (getchar() == 'q') {
            printf("Closing..");
            break;
        }
    }
    FilterDetach(MINISPY_NAME, L"C:\\", 0);
    Sleep(100);
    
    printf("Successfully detached volume\n");
    CloseHandle(Port);
    
    return 0;
    
    restore_inf R;
    R.RootDir = std::wstring();
    R.TargetLetter = 'G';
    R.SrcLetter = 'C';
    R.Version = 1;
    RestoreRecoveryFile(R);
    return 0;
    
    int BufferSize = 1024 * 1024 * 128; // 128KB
    int Found = 0;
    backup_metadata* B = (backup_metadata*)malloc(BufferSize);
    
    NarGetBackupsInDirectory(L"C:\\Users\\Batuhan\\Desktop\\termproject", B, BufferSize, &Found);
    
    data_array<nar_record> S = { 0 };
    data_array<nar_record> E = { 0 };
    
    S.Insert(REGION(0, 150));
    S.Insert(REGION(220, 340));
    S.Insert(REGION(400, 600));
    S.Insert(REGION(750, 800));
    
    E.Insert(REGION(80, 140));
    E.Insert(REGION(200, 250));
    E.Insert(REGION(385, 520));
    E.Insert(REGION(550, 800));
    
    NARDEBUGExcludeRegions(S, E);
    return 0;
    
    return 0;
}

#endif





VOID
DisplayError(DWORD Code) {
    
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
