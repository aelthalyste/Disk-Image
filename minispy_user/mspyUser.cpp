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
#include <vector>
#include <string>
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

inline void
PrintList(region_chain* Temp) {
    printf("\n######\n");
    while (Temp != NULL) {
        printf("%u\t%u\n", Temp->Rec.StartPos, Temp->Rec.Len);
        Temp = Temp->Next;
    }
    printf("\n######\n");
}

inline void
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
    
    
    
    VolInf->Version = -1;
    VolInf->ClusterSize = 0;
    VolInf->VSSPTR = 0;
    
    VolInf->LogHandle = INVALID_HANDLE_VALUE;
    
    VolInf->Stream.Records = { 0,0 };
    VolInf->Stream.Handle = 0;
    VolInf->Stream.RecIndex = 0;
    VolInf->Stream.ClusterIndex = 0;
    
    VolInf->MFTLCN = { 0 };
    
    DWORD BytesPerSector = 0;
    DWORD SectorsPerCluster = 0;
    DWORD ClusterCount = 0;
    
    wchar_t DN[] = L"C:";
    DN[0] = Letter;
    
    
    wchar_t V[] = L"!:\\";
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
        VolInf->FileWriteMutex = CreateMutexA(NULL, FALSE, NULL);
        if (VolInf->FileWriteMutex != NULL) {
            Return = TRUE;
        }
        else {
            printf("Couldnt create mutex for volume backup information structure\n");
        }

        printf("Initialized volume inf %c, cluster size %i, volume cluster count %i\n", VolInf->Letter, VolInf->ClusterSize, VolInf->VolumeTotalClusterCount);

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


// 
nar_record*
NarGetMFTRegionsByCommandLine(char Letter, int* OutRecordCount){
    
    char CmdBuffer[512];
    char OutputName[] = "naroutput.txt";

    sprintf(CmdBuffer, "fsutil file queryextents %c:\\$MFT > %s", Letter, OutputName);
    system(CmdBuffer);

    file_read FileContents = NarReadFile(OutputName);

    char* str = (char*)FileContents.Data;
    int StrLen = FileContents.Len;

    if (str == NULL || OutRecordCount == NULL) return 0;


    size_t ResultSize = 128 * sizeof(nar_record);
    nar_record* Result = (nar_record*)malloc(ResultSize);
    memset(Result, 0, ResultSize);

    int RecordFound = 0;

    char *prev = str;
    char *next = str;
    
    char ClustersHexStr[64];
    char LCNHexStr[64];
    char Line[128];
    memset(ClustersHexStr, 0, sizeof(ClustersHexStr));
    memset(LCNHexStr, 0, sizeof(LCNHexStr));
    memset(Line, 0, sizeof(Line));

    while(TRUE){

        while(*next != '\n' && *next != 0 && *next != -3){
            next++;
        }

        if(next != prev){
        
            // some lines might be ending with /r/n depending on if file is created as text file in windows. delete /r to make parsing easier 
            int StrLen = (next - prev);

            if (*(next - 1) == '\r') StrLen--;

            memcpy(Line, prev, StrLen);
            Line[StrLen] = 0; // null termination by hand
            

            // there isnt any overheat to search all of them at once, we are dealing with very short strings here
            char *p1 = strstr(Line, "VCN:");
            char *p2 = strstr(Line, "Clusters:");
            char *p3 = strstr(Line, "LCN:");
                        

            if (p1 && p2 && p3) {
                
                p2 += sizeof("Clusters:");
                size_t LCNHexStrLen = (size_t)(p3 - p2) + 1;
                memcpy(LCNHexStr, p2, LCNHexStrLen);
                LCNHexStr[LCNHexStrLen] = 0;


                p3 += sizeof("LCN:");
                size_t ClustersHexStrLen = strlen(p3);
                memcpy(ClustersHexStr, p3, ClustersHexStrLen);
                ClustersHexStr[ClustersHexStrLen] = 0;
                
                long LCNStart = strtol(ClustersHexStr, 0, 16);
                long LCNLen = strtol(LCNHexStr, 0, 16);

                Result[RecordFound].StartPos = LCNStart;
                Result[RecordFound].Len = LCNLen;
                RecordFound++;

                // valid line
            }
            else {
                // invalid line
            }

            prev = (++next);


        }
        else{
            break;
        }

    }

    *OutRecordCount = RecordFound;

    Result = (nar_record*)realloc(Result, RecordFound * sizeof(nar_record));
    FreeFileRead(FileContents);

    return Result;

}


#pragma warning(push)
#pragma warning(disable:4365)
#pragma warning(disable:4244)
//THIS FUNCTION CONTAINS BYTE MASKING AND CODE FOR EXTRACTING NON-STANDART BYTE SIZES (3-5 bytes), BETTER OMIT THIS WARNING

data_array<nar_record>
GetMFTLCN(char VolumeLetter, HANDLE VolumeHandle) {

    BOOLEAN JustExtractMFTRegions = FALSE;

    UINT32 MEMORY_BUFFER_SIZE = 1024LL * 1024LL * 1024LL;
    UINT32 ClusterExtractedBufferSize = 1024 * 1024 * 5;
    INT32 ClusterSize = NarGetVolumeClusterSize(VolumeLetter);

    struct {
        UINT32 Count[8];
        UINT32 Start[8];
    }DEBUG_CLUSTER_INFORMATION;
    
    nar_record* ClustersExtracted = (nar_record*)malloc(ClusterExtractedBufferSize);
    unsigned int ClusterExtractedCount = 0;

    if (ClustersExtracted != 0) {
        memset(ClustersExtracted, 0, ClusterExtractedBufferSize);
    }
    
#define NAR_INSERT_CLUSTER(START, LEN) ClustersExtracted[ClusterExtractedCount++] = {(UINT32)(START), (UINT32)(LEN) };
#define NAR_MFT_DBG_INFO 0


#if NAR_MFT_DBG_INFO
#define DBG_INC(v) (v)++;
#else
#define DBG_INC(v) (v);
#endif

    int DBG_INDX_FOUND = 0;
    int DBG_TOTAL_DIR_FOUND = 0;
    int DBG_CLUSTER_COUNT = 0;
    int DBG_TOTAL_FILES_FOUND = 0;

    UNREFERENCED_PARAMETER(DBG_INDX_FOUND);
    UNREFERENCED_PARAMETER(DBG_TOTAL_DIR_FOUND);
    UNREFERENCED_PARAMETER(DBG_CLUSTER_COUNT);
    UNREFERENCED_PARAMETER(DBG_TOTAL_FILES_FOUND);


    memset(&DEBUG_CLUSTER_INFORMATION, 0, sizeof(DEBUG_CLUSTER_INFORMATION));

//#define DBG_INSERT(START,COUNT) DEBUG_CLUSTER_INFORMATION.Start[DBG_CLUSTER_COUNT] = (UINT32)(START); DEBUG_CLUSTER_INFORMATION.Count[DBG_CLUSTER_COUNT] = (UINT32)(COUNT); (DBG_CLUSTER_COUNT)++;
#define DBG_INSERT(START,COUNT) do{(START);(COUNT);}while(0);
    
    void* FileBuffer = malloc(MEMORY_BUFFER_SIZE);
    UINT32 FileBufferCount = MEMORY_BUFFER_SIZE / 1024LL;

    if (FileBuffer) {
        char VolumeName[64];
        sprintf(VolumeName, "\\\\.\\%c:", VolumeLetter);

        INT16 DirectoryFlag = 0x0002;
        INT32 FileRecordSize = 1024;
        INT16 FlagOffset = 22;
        INT32 NAR_INDEX_ALLOCATION_FLAG = 0xA0;

        
        if (VolumeHandle != INVALID_HANDLE_VALUE) {

            DWORD BR = 0;

            // NOTE(Batuhan): IMPORTANT, first mft entry offset is hardcoded, for different sector sizes that might
            // change. sadly i havent found official document to says like, mft starts at 0x10th sector.
            // on my machine it always starts from 0x10th sector. which is second cluster if your sector size is 512
            // and cluster size is 8*sector


#if 0
            DWORD MFTOffset = 8192; // 0x10th Offset
            if(NarSetFilePointer(VolumeHandle, MFTOffset)){


                if(ReadFile(VolumeHandle, FileBuffer, 1024, &BR, 0) && BR == 1024){

                    void *FileRecord = FileBuffer;

                    INT16 FirstAttributeOffset = *(INT16*)((BYTE*)FileRecord + 20);
                    void* FileAttribute = (BYTE*)FileRecord + FirstAttributeOffset;

                    
                    INT32 RemainingLen = *(INT32*)((BYTE*)FileRecord + 24); // Real size of the file record
                    RemainingLen -= (FirstAttributeOffset + 8); //8 byte for end of record mark, remaining len includes it too.

                    
                    while(RemainingLen > 0){

                        // 0x80 == $DATA
                        if ((*(INT32*)FileAttribute & 0x80) == 0x80){

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
                            
                            DBG_INC(DBG_INDX_FOUND);
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


                                FirstCluster += OldClusterStart;
                                // Update tail
                                OldClusterStart = FirstCluster;

                                D = (BYTE*)D + (FirstClusterSize + ClusterCountSize + 1);
                                DBG_INSERT(FirstCluster, ClusterCount);
                                NAR_INSERT_CLUSTER(FirstCluster, ClusterCount);
                                
                                
                                DBG_INC(DBG_INDX_FOUND);
                                
                            }
                            break;

                        }

                        if ((*(INT32*)FileAttribute & 0x80) > 0x80){
                            printf("Couldnt find 0x80 file attribute for $MFT\n");
                            break;// early termination
                        }



                        // found neither Data nor attr bigger than it, just substract attribute size from remaininglen to determine if we should keep iterating

                        RemainingLen -= (*(INT32*)((BYTE*)FileAttribute + 4));
                        FileAttribute = (BYTE*)FileAttribute + *(INT32*)((BYTE*)FileAttribute + 4);

                    }

                }


            }

#endif


            int RecCountByCommandLine = 0;
            nar_record* TempRecords = NarGetMFTRegionsByCommandLine(VolumeLetter, &RecCountByCommandLine);
            if (TempRecords != 0 && RecCountByCommandLine != 0) {
                memcpy(ClustersExtracted, TempRecords, RecCountByCommandLine*sizeof(nar_record));
                ClusterExtractedCount = RecCountByCommandLine;
            }

            // TODO (Batuhan): remove this after testing on windows server, looks like rest of the code finds some invalid regions on volume.
            if (JustExtractMFTRegions) {
                printf("Found %i regions\n", ClusterExtractedCount);
                for(int indx = 0; indx < ClusterExtractedCount; indx++){
                    printf("0x%X\t0x%X\n", ClustersExtracted[indx].StartPos, ClustersExtracted[indx].Len);
                }
                goto EARLY_TERMINATION;
            }
            

            //nar_region *MFTRegions = 0;
            //goto EARLY_TERMINATION;

            unsigned int MFTRegionCount = ClusterExtractedCount;
            
            for (unsigned int MFTOffsetIndex = 0; MFTOffsetIndex < MFTRegionCount; MFTOffsetIndex++) {

                ULONGLONG Offset = (ULONGLONG)ClustersExtracted[MFTOffsetIndex].StartPos * (UINT64)ClusterSize;
                INT32 FilePerCluster = ClusterSize / 1024;
                ULONGLONG FileCount = (ULONGLONG)ClustersExtracted[MFTOffsetIndex].Len * (UINT64)FilePerCluster;

                // set file pointer to actual records
                if (NarSetFilePointer(VolumeHandle, Offset)) {

                    // NOTE(Batuhan): Check if we can read all file records at once

                    if (FileBufferCount > FileCount) {
                        // We can read all file records for given data region

                        if (ReadFile(VolumeHandle, FileBuffer, FileCount * 1024, &BR, 0) && BR == FileCount * 1024) {


                            for (unsigned int FileRecordIndex = 0; FileRecordIndex < FileCount; FileRecordIndex++) {


                                void* FileRecord = (BYTE*)FileBuffer + (UINT64)FileRecordSize * (UINT64)FileRecordIndex;

                                // file flags are at 22th offset in the record
                                if (*(INT32*)FileRecord != 'ELIF') {
                                    // block doesnt start with 'FILE0', skip
                                    continue;
                                }
                                
                                DBG_INC(DBG_TOTAL_FILES_FOUND);
                                INT16 Flags = *(INT16*)((BYTE*)FileRecord + FlagOffset);
                                

                                if ((Flags & DirectoryFlag) == DirectoryFlag) {
                                    // that is a directory 
                                    DBG_INC(DBG_TOTAL_DIR_FOUND);
                                    
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
                                            
                                            DBG_INC(DBG_INDX_FOUND);
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
                                                DBG_INC(DBG_INDX_FOUND);
                                                
                                            }


                                            // found INDEX_ALLOCATION attribute
                                            break;
                                        }

                                        if ((*(INT32*)FileAttribute & NAR_INDEX_ALLOCATION_FLAG) > NAR_INDEX_ALLOCATION_FLAG) break;// early termination


                                        // found neither indexallocation nor attr bigger than it, just substract attribute size from remaininglen to determine if we should keep iterating
                                        INT32 AttrSize = (*(unsigned short*)((BYTE*)FileAttribute + 4));
                                        RemainingLen -= AttrSize;
                                        FileAttribute = (BYTE*)FileAttribute + AttrSize;
                                        if (AttrSize == 0) break;

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

    
    EARLY_TERMINATION:


    // TODO remove that check

    data_array<nar_record> Result = { 0 };
    ClustersExtracted = (nar_record*)realloc(ClustersExtracted, ClusterExtractedCount * sizeof(nar_record));
    Result.Data = ClustersExtracted;
    Result.Count = ClusterExtractedCount;

    qsort(Result.Data, Result.Count, sizeof(nar_record), CompareNarRecords);
    MergeRegions(&Result);
    
    free(FileBuffer);
    
    ULONGLONG VolumeSize = NarGetVolumeSize(VolumeLetter);
    UINT32 TruncateIndex = 0;
    for (int i = 0; i < Result.Count; i++) {
        if ((ULONGLONG)Result.Data[i].StartPos * (ULONGLONG)ClusterSize + (ULONGLONG)Result.Data[i].Len * ClusterSize > VolumeSize) {
            TruncateIndex = i;
            break;
        }
    }

    if (TruncateIndex > 0) {
        
        printf("INDEX_ALLOCATION blocks exceeds volume size, truncating from index %i\n", TruncateIndex);
        Result.Data = (nar_record*)realloc(Result.Data, TruncateIndex * sizeof(nar_record));
        Result.Count = TruncateIndex;

    }

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
            printf("Volume GUID for %c : %S\n", (char)Letter, Command.VolumeGUIDStr);
            HRESULT hResult = FilterSendMessage(Context->Port, &Command, sizeof(Command), 0, 0, &BytesReturned);
            
            if (SUCCEEDED(hResult)) {
                if (InitVolumeInf(&VolInf, Letter, Type)) {
                    ErrorOccured = FALSE;
                }
                Context->Volumes.Insert(VolInf);
                printf("Volume %c inserted to the list\n", (char)Letter);
            }
            else {
                printf("KERNEL error occured: Couldnt add volume @ kernel side, error code %i, volume %c\n", hResult, (char)Letter);
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
    
    unsigned int VolumesFound = 0;
    unsigned int MaxFit = BufferSize / (unsigned int)sizeof(*Out);
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
        Out[VolumesFound].DiskType = (char)NarGetVolumeDiskType((char)V->Letter);
        Out[VolumesFound].Size = NarGetVolumeSize((char)V->Letter);
        
        VolumesFound++;
        
    }
    
    if (Result == FALSE) {
        memset(Out, 0, BufferSize); //If any error occured, clear given buffer
    }
    else {
        *OutCount = (int)VolumesFound;
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
inline BOOLEAN
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
        V->INVALIDATEDENTRY = TRUE;

        // Since searching volume in context done via checking letters, if I assign 0 to letter, it cant be detected, since its not any character at all, just null termination.
        V->Letter = 0;
        NarRemoveVolumeFromKernelList(L, C->Port);
        
        V->FullBackupExists = FALSE;
        V->FilterFlags.IsActive = FALSE;
        CloseHandle(V->LogHandle);
        
        FreeDataArray(&V->Stream.Records);
        V->Stream.RecIndex = 0;
        V->Stream.ClusterIndex = 0;
        CloseHandle(V->Stream.Handle);
        V->VSSPTR.Release();
        
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
        DisplayError((DWORD)Result);
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


INT32
GetVolumeID(PLOG_CONTEXT C, wchar_t Letter) {
    
    INT ID = NAR_INVALID_VOLUME_TRACK_ID;
    if (C->Volumes.Data != NULL) {
        if (C->Volumes.Count != 0) {
            for (unsigned int i = 0; i < C->Volumes.Count; i++) {
                if (C->Volumes.Data[i].Letter == Letter) {
                    ID = (int)i; // safe conversion
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

    // int ClustersToRead = (int)TotalSize / (int)VolInf->ClusterSize;


    if (TotalSize == 0) {
        printf("Passed totalsize as 0, terminating now\n");
        return TRUE;
    }
    

    unsigned int RemainingSize = TotalSize;
    void* CurrentBufferOffset = Buffer;
    
    if ((UINT)VolInf->Stream.RecIndex >= VolInf->Stream.Records.Count) {
        printf("End of the stream\n", VolInf->Stream.RecIndex, VolInf->Stream.Records.Count);
        return Result;
    }

    while (RemainingSize) {

        if ((UINT)VolInf->Stream.RecIndex >= VolInf->Stream.Records.Count) {
            printf("Rec index was higher than record's count, result %u, rec_index %i rec_count %i\n", Result, VolInf->Stream.RecIndex, VolInf->Stream.Records.Count);
            return Result;
        }

        DWORD BytesReadAfterOperation = 0;

        UINT64 ClustersRemainingByteSize = (UINT64)VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].Len - (UINT64)VolInf->Stream.ClusterIndex;
        ClustersRemainingByteSize *= VolInf->ClusterSize;

        DWORD ReadSize = (DWORD)MIN((UINT64)RemainingSize, ClustersRemainingByteSize); // safe to truncate, since remainingsize's max value is UINT32_MAX, and its MIN macro
        // we expect max value of DWORD.

        ULONGLONG FilePtrTarget = (ULONGLONG)VolInf->ClusterSize * ((ULONGLONG)VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].StartPos + (ULONGLONG)VolInf->Stream.ClusterIndex);
        if (NarSetFilePointer(VolInf->Stream.Handle, FilePtrTarget)) {

            BOOL OperationResult = ReadFile(VolInf->Stream.Handle, CurrentBufferOffset, ReadSize, &BytesReadAfterOperation, 0);
            Result += BytesReadAfterOperation;
            if (!OperationResult || BytesReadAfterOperation != ReadSize) {
              printf("STREAM ERROR: Couldnt read %lu bytes, instead read %lu, error code %i\n", ReadSize, BytesReadAfterOperation, OperationResult);
              printf("rec_index % i rec_count % i, remaining bytes %I64u, offset at disk %I64u\n", VolInf->Stream.RecIndex, VolInf->Stream.Records.Count, ClustersRemainingByteSize, FilePtrTarget);
              printf("Total bytes read for buffer %u\n", Result);

              NarDumpToFile("STREAM_OVERFLOW_ERROR_LOGS", VolInf->Stream.Records.Data, VolInf->Stream.Records.Count*sizeof(nar_record));

              ErrorOccured = TRUE;
          }


        }
        else {
            printf("Couldnt set file pointer to %I64d\n", FilePtrTarget);
            ErrorOccured = TRUE;
        }

        INT32 ClusterToIterate = (INT32)(BytesReadAfterOperation / 4096);
        VolInf->Stream.ClusterIndex += ClusterToIterate;

        if ((UINT32)VolInf->Stream.ClusterIndex == VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].Len) {
            VolInf->Stream.ClusterIndex = 0;
            VolInf->Stream.RecIndex++;
        }
        if ((UINT32)VolInf->Stream.ClusterIndex > VolInf->Stream.Records.Data[VolInf->Stream.RecIndex].Len) {
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
            
            if (SaveMetadata((char)V->Letter, NAR_FULLBACKUP_VERSION, V->ClusterSize, V->BT, V->Stream.Records, V->Stream.Handle, V->MFTLCN)) {
                Return = TRUE;
                V->FullBackupExists = TRUE;
            }
            else{
                printf("Coulndt save metadata\n");
            }
            
        }
        else {
            
            printf("Operation failed, closing log handle\n");
            //Somehow operation failed.
            V->FilterFlags.IsActive = FALSE;
            CLEANHANDLE(V->LogHandle);
            V->FullBackupExists = FALSE;
            
        }
        
        //Termination of fullbackup
        
        //neccecary for incremental backup
        V->LastBackupRegionOffset = 0;
        V->ActiveBackupInf.PossibleNewBackupRegionOffsetMark = 0;
        
        // after 
        V->Version = 0;

    }
    else {
        //Termination of diff-inc backup
        printf("Termination of diff-inc backup\n");
        
        if (Succeeded) {

            // NOTE(Batuhan):
            printf("Will save metadata to working directory, Version : %i\n", V->Version);
            SaveMetadata((char)V->Letter, V->Version, V->ClusterSize, V->BT, V->Stream.Records, V->Stream.Handle, V->MFTLCN);

            /*
         Since merge algorithm may have change size of the record buffer,
            we should overwrite and truncate it
         */
            Return = TRUE;
            printf("Swapping LastBackupRegionOffset and PossibleNewBackupRegionOffsetMark (%I64u, %I64u) \n", V->LastBackupRegionOffset, V->ActiveBackupInf.PossibleNewBackupRegionOffsetMark);
            V->Version++;
            V->LastBackupRegionOffset = V->ActiveBackupInf.PossibleNewBackupRegionOffsetMark;
            V->ActiveBackupInf.PossibleNewBackupRegionOffsetMark = 0;

            //backup operation succeeded condition end (for diff and inc)
        }
        else{
            printf("Backup failed for inc-diff operation\n");
        }
        

    }
    
    CLEANHANDLE(V->Stream.Handle);
    CLEANMEMORY(V->Stream.Records.Data);
    V->Stream.Records.Count = 0;
    V->Stream.RecIndex = 0;
    V->Stream.ClusterIndex = 0;
    V->VSSPTR.Release();
    FreeDataArray(&V->MFTLCN);

    
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
    printf("Entered setup stream for volume %c, version %i\n", (char)L, VolInf->Version);

    VolInf->VSSPTR.Release();
    VolInf->Stream.ClusterIndex = 0;
    VolInf->Stream.RecIndex = 0;
    VolInf->Stream.Records.Count = 0;
    
    auto SetupMFTLCNandAppendStream = [&]() {

        VolInf->MFTLCN = GetMFTLCN(L, VolInf->Stream.Handle);
        if (VolInf->MFTLCN.Data != 0) {

            printf("Parsed MFTLCN for volume %c for version %i, count %u", (wchar_t)VolInf->Letter, VolInf->Version, VolInf->MFTLCN.Count);
            VolInf->Stream.Records.Data = (nar_record*)realloc(VolInf->Stream.Records.Data, (VolInf->Stream.Records.Count + VolInf->MFTLCN.Count) * sizeof(nar_record));
            memcpy(&VolInf->Stream.Records.Data[VolInf->Stream.Records.Count], VolInf->MFTLCN.Data, VolInf->MFTLCN.Count * sizeof(nar_record));

            VolInf->Stream.Records.Count += VolInf->MFTLCN.Count;
            qsort(VolInf->Stream.Records.Data, VolInf->Stream.Records.Count, sizeof(nar_record), CompareNarRecords);
            MergeRegions(&VolInf->Stream.Records);


        }
        else {
            printf("Couldnt parse MFT at setupstream function for volume %c, version %i\n", L, VolInf->Version);

        }


    };

    if (SetupStreamHandle(VolInf)) {

        // NOTE(Batuhan): Experimental feature, from now on (06.10.2020), every binary data MUST contain MFTLCN with extended INDEX_ALLOCATION data.
        // that helps file explorer to search that volume 
        

        printf("Setup stream handle successfully\n");
        if (!VolInf->FullBackupExists) {
            printf("Fullbackup stream is preparing\n");
            //Fullbackup stream
            if (SetFullRecords(VolInf)) {
                SetupMFTLCNandAppendStream();

                Return = TRUE;
            }
            else {
                printf("SetFullRecords failed!\n");
            }
            
            if (SI && Return) {
                
                SI->ClusterCount = 0;
                SI->ClusterSize = (INT32)VolInf->ClusterSize;
                SI->MetadataFileName = GenerateMetadataName(VolInf->Letter, NAR_FULLBACKUP_VERSION);
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
                SI->ClusterSize = (INT32)VolInf->ClusterSize;
                SI->FileName = GenerateBinaryFileName(VolInf->Letter, VolInf->Version);
                SI->MetadataFileName = GenerateMetadataName(VolInf->Letter, VolInf->Version);
                for (unsigned int i = 0; i < VolInf->Stream.Records.Count; i++) {
                    SI->ClusterCount += VolInf->Stream.Records.Data[i].Len;
                }
                
            }
        }
        
    }
    else {
        printf("SetupStreamHandle failed\n");
    }
    

    {
        volume_backup_inf* V = VolInf;
        
        SetupMFTLCNandAppendStream();

        unsigned int TruncateIndex = 0;
        
        for(unsigned int RecordIndex = 0; RecordIndex < V->Stream.Records.Count; RecordIndex++){
            
            if((INT64)V->Stream.Records.Data[RecordIndex].StartPos + (INT64)V->Stream.Records.Data[RecordIndex].Len > (INT64)V->VolumeTotalClusterCount){
                TruncateIndex = RecordIndex;
                break;
            }
            
        }

        if(TruncateIndex > 0){
            printf("Found regions that exceeds volume size, truncating stream record array from %i to %i\n", V->Stream.Records.Count, TruncateIndex);
            V->Stream.Records.Data = (nar_record*)realloc(V->Stream.Records.Data, TruncateIndex*sizeof(nar_record));            
            V->Stream.Records.Count = TruncateIndex;
            
            SI->ClusterCount = 0;
            for (unsigned int i = 0; i < VolInf->Stream.Records.Count; i++) {
                SI->ClusterCount += VolInf->Stream.Records.Data[i].Len;
            }

        }

    }

    printf("Totalbytes should be backed up %I64u\n", (UINT64)SI->ClusterCount*(UINT64)SI->ClusterSize);

    
    return Return;
}


/*
  From given volume handle, extracts region information from volume
  Handle can be VSS handle or normal volume handle. VSS is preferred since volume information might change over time
  returns NULL if any error occurs
*/
nar_record*
GetVolumeRegionsFromBitmap(HANDLE VolumeHandle, UINT32* OutRecordCount) {

    if(OutRecordCount == NULL) return NULL;

    nar_record* Records = NULL;
    UINT32 RecordCount = 0;


    STARTING_LCN_INPUT_BUFFER StartingLCN;
    StartingLCN.StartingLcn.QuadPart = 0;
    ULONGLONG MaxClusterCount = 0;
    DWORD BufferSize = 1024 * 1024 * 64; //  megabytes
    
    VOLUME_BITMAP_BUFFER* Bitmap = (VOLUME_BITMAP_BUFFER*)malloc(BufferSize);
    
    if (Bitmap != NULL) {
    
        HRESULT R = DeviceIoControl(VolumeHandle, FSCTL_GET_VOLUME_BITMAP, &StartingLCN, sizeof(StartingLCN), Bitmap, BufferSize, 0, 0);
        if (SUCCEEDED(R)) {
            
            MaxClusterCount = (ULONGLONG)Bitmap->BitmapSize.QuadPart;
            DWORD ClustersRead = 0;
            UCHAR* BitmapIndex = Bitmap->Buffer;
            UCHAR BitmapMask = 1;
            //UINT32 CurrentIndex = 0;
            UINT32 LastActiveCluster = 0;
            
            UINT32 RecordBufferSize = 128 * 1024 * 1024;
            Records = (nar_record*)malloc(RecordBufferSize);
            
            memset(Records, 0, RecordBufferSize);

            Records[0] = { 0,1 };
            RecordCount++; 

            ClustersRead++;
            BitmapMask <<= 1;

            while (ClustersRead < MaxClusterCount) {
           
                if ((*BitmapIndex & BitmapMask) == BitmapMask) {

                    if (LastActiveCluster == ClustersRead - 1) {
                        Records[RecordCount].Len++;
                    }
                    else {
                        Records[++RecordCount] = { ClustersRead, 1 };
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


        }
        else {
            // NOTE(Batuhan): DeviceIOControl failed
            printf("Get volume bitmap failed [DeviceIoControl].\n");
            printf("Result was -> %d\n", R);
        }

    }
    else {
        printf("Couldn't allocate memory for bitmap buffer, tried to allocate %u bytes!\n", BufferSize);
    }


    free(Bitmap);
    
    if(Records != NULL){
      Records = (nar_record*)realloc(Records, RecordCount*sizeof(nar_record));
      (*OutRecordCount) = RecordCount;
    }
    else{
      (*OutRecordCount) = 0;
    }

    return Records;
}


BOOLEAN
SetFullRecords(volume_backup_inf* V) {
    
    //UINT* ClusterIndices = 0;
    
    UINT32 Count = 0;
    V->Stream.Records.Data  = GetVolumeRegionsFromBitmap(V->Stream.Handle, &Count);
    V->Stream.Records.Count = Count;

    return (V->Stream.Records.Data != NULL);
}

BOOLEAN
SetIncRecords(volume_backup_inf* VolInf) {
    
    BOOLEAN Result = FALSE;
    if(VolInf == NULL){
        printf("SetIncRecords: VolInf was null\n");
        return FALSE;
    }

    VolInf->Stream.Records.Data = 0;
    VolInf->Stream.Records.Count = 0;

    DWORD TargetReadSize = 0;
    DWORD BytesRead = 0;

    DWORD RetVal = WaitForSingleObject(VolInf->FileWriteMutex, 500);
    if(RetVal == WAIT_OBJECT_0) {

        FlushFileBuffers(VolInf->LogHandle);       
        
        // safe to read from V->LogHandle

        if(VolInf->ActiveBackupInf.PossibleNewBackupRegionOffsetMark - VolInf->LastBackupRegionOffset < 0xFFFFFFFFll){
            // NOTE(Batuhan): Assumption made here, change in log file size does not exceed 32 bit max integer value.
            TargetReadSize = (DWORD)(VolInf->ActiveBackupInf.PossibleNewBackupRegionOffsetMark - VolInf->LastBackupRegionOffset);
        
            if(NarSetFilePointer(VolInf->LogHandle, (UINT64)VolInf->LastBackupRegionOffset)){
                
                VolInf->Stream.Records.Data = (nar_record*)malloc(TargetReadSize);
                memset(VolInf->Stream.Records.Data, 0, TargetReadSize);
                VolInf->Stream.Records.Count = TargetReadSize/sizeof(nar_record);

                if(ReadFile(VolInf->LogHandle, VolInf->Stream.Records.Data, TargetReadSize, &BytesRead, 0) && BytesRead == TargetReadSize){
                    
                    qsort(VolInf->Stream.Records.Data, VolInf->Stream.Records.Count, sizeof(nar_record), CompareNarRecords);
                    MergeRegions(&VolInf->Stream.Records);
                    Result = TRUE;

                }
                else{

                    printf("SetIncRecords Couldnt read %lu, instead read %lu\n", TargetReadSize, BytesRead);
                    DisplayError(GetLastError());

                }

            }
            else{
                printf("Couldnt set file pointer\n");
            }

        }
        else{
            printf("Records appended since last backups size exceeds 4GB, size %I64u\n", VolInf->ActiveBackupInf.PossibleNewBackupRegionOffsetMark - VolInf->LastBackupRegionOffset);
            // TODO exceeds 4GB limit
        }


        ReleaseMutex(VolInf->FileWriteMutex);

    }
    else{
        
        printf("Couldnt lock write file, ");
        if(RetVal == WAIT_TIMEOUT){
            printf("TIMEOUT");   
        }
        else if(RetVal == WAIT_FAILED){
            printf("FAILED");
        }
        else if(RetVal == WAIT_ABANDONED){
            printf("ABONDONED");
        }
        else{
            printf("UNDEFINED\n");
        }

        printf("\n");

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


BOOLEAN
SetDiffRecords(volume_backup_inf* V) {

    printf("Entered SetDiffRecords\n");
    BOOLEAN Result = FALSE;

    if (V == NULL || V->LogHandle == INVALID_HANDLE_VALUE) {
        
        if(V){
            if(V->LogHandle == INVALID_HANDLE_VALUE){
                printf("LogHandle has invalid handle value\n");
            }
        }
        else{
            printf("volume backup inf structure was null\n");
        }

        return FALSE;

    }

    DWORD WINAPIRetVAL = WaitForSingleObject(V->FileWriteMutex, 500);
    if(WINAPIRetVAL == WAIT_OBJECT_0){
        FlushFileBuffers(V->LogHandle);
        
        // we are safe to assume file ptr has lower value than 63 bit integer value
        V->ActiveBackupInf.PossibleNewBackupRegionOffsetMark = (INT64)NarGetFilePointer(V->LogHandle);
        if(V->ActiveBackupInf.PossibleNewBackupRegionOffsetMark < 0xFFFFFFFFll){
            
            // NOTE (Batuhan): IMPORTANT, so if total log size exceeds 4GB, we cant backup succcessfully, since reading file larger than 4GB is 
            // problem by itself, one can do partially load file and continuously compress it via qsort & mergeregions. then final log size drastically
            // becames lower.

            DWORD TargetReadSize = (DWORD)V->ActiveBackupInf.PossibleNewBackupRegionOffsetMark; // not so safe to truncate
            DWORD BytesRead = 0;

            if(NarSetFilePointer(V->LogHandle, 0)){

                V->Stream.Records.Data = (nar_record*)malloc(TargetReadSize);
                V->Stream.Records.Count = TargetReadSize / sizeof(nar_record);

                if(ReadFile(V->LogHandle, V->Stream.Records.Data, TargetReadSize, &BytesRead,0) && BytesRead == TargetReadSize){

                    printf("Succ read diff records, will sort and merge regions to save in space\n");
                    qsort(V->Stream.Records.Data, V->Stream.Records.Count, sizeof(nar_record), CompareNarRecords);
                    MergeRegions(&V->Stream.Records);
                    Result = TRUE;

                }
                else{

                    printf("SetDiffRecords Couldnt read %lu, instead read %lu\n", TargetReadSize, BytesRead);
                    DisplayError(GetLastError());
                    
                }

            }
            else{
                
                printf("Couldnt set file pointer to beginning of the file\n");

            }

        }  
        else{
            printf("LogFile size exceeds 4GB, cant process that much log, %I64d\n", V->ActiveBackupInf.PossibleNewBackupRegionOffsetMark);
        }

        ReleaseMutex(V->FileWriteMutex);

    }
    else{

        printf("Couldnt lock write file, ");
        if(WINAPIRetVAL == WAIT_TIMEOUT){
            printf("TIMEOUT");   
        }
        else if(WINAPIRetVAL == WAIT_FAILED){
            printf("FAILED");
        }
        else if(WINAPIRetVAL == WAIT_ABANDONED){
            printf("ABONDONED");
        }
        else{
            printf("UNDEFINED\n");
        }

        printf("\n");

    }



#if 0
    if (V->ActiveBackupInf.PossibleNewBackupRegionOffsetMark > 0xFFFFFFFFll) {
        // TODO(Batuhan) :
    }

    V->ActiveBackupInf.PossibleNewBackupRegionOffsetMark;

    V->Stream.Records.Data = (nar_record*)malloc(V->ActiveBackupInf.PossibleNewBackupRegionOffsetMark);
    V->Stream.Records.Count = V->ActiveBackupInf.PossibleNewBackupRegionOffsetMark / sizeof(nar_record);

    DWORD BytesRead = 0;
    DWORD TargetReadSize = (DWORD)V->ActiveBackupInf.PossibleNewBackupRegionOffsetMark;
    Result = ReadFile(V->LogHandle, V->Stream.Records.Data, TargetReadSize, &BytesRead, 0);

    if (Result && TargetReadSize == BytesRead) {
        // thats it?
    }
    else {
        // TODO(Batuhan): 

    }
#endif


    return Result;
}
    



BOOLEAN
SetupStreamHandle(volume_backup_inf* VolInf) {
    
    if (VolInf == NULL) {
        printf("volume_backup_inf is null\n");
        return FALSE;
    }
    VolInf->VSSPTR.Release();
    
    
    VolInf->FilterFlags.IsActive = TRUE;
    
    if (VolInf->FullBackupExists) {

        DWORD RetVal = WaitForSingleObject(VolInf->FileWriteMutex, 500);
        if(RetVal == WAIT_OBJECT_0){

            LARGE_INTEGER CurrentFilePtr = {0};
            FlushFileBuffers(VolInf->LogHandle);

            if(GetFileSizeEx(VolInf->LogHandle, &CurrentFilePtr) != 0){
                VolInf->ActiveBackupInf.PossibleNewBackupRegionOffsetMark = CurrentFilePtr.QuadPart;
                printf("Current log file size %I64d\n", CurrentFilePtr.QuadPart);
            }
            else{
                printf("Couldnt get log file size\n");
                DisplayError(GetLastError());
            }

            ReleaseMutex(VolInf->FileWriteMutex);

        }
        else{
            printf("Couldnt lock file write mutex @ setupstreamhandle, returning false\n");
            return FALSE;
        }
    
#if 0
        if (!DetachVolume(VolInf)) {
            printf("Detach volume failed\n");
            return FALSE;
        }
        else{

            

        }
#endif

        printf("Volume %c detached!\n", VolInf->Letter);

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
        printf("!FullBackupExists ");
        std::wstring LogFileName = GenerateLogFileName(VolInf->Letter);
        VolInf->LogHandle = CreateFileW(LogFileName.c_str(), GENERIC_WRITE | GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0);
        if(VolInf->LogHandle != INVALID_HANDLE_VALUE){

            DWORD RetVal = WaitForSingleObject(VolInf->FileWriteMutex, 500);
            if(RetVal == WAIT_OBJECT_0){

                LARGE_INTEGER CurrentFilePtr = {0};
                if(GetFileSizeEx(VolInf->LogHandle, &CurrentFilePtr) != 0){
                    VolInf->ActiveBackupInf.PossibleNewBackupRegionOffsetMark = CurrentFilePtr.QuadPart;
                    printf("Log file size %I64d\n", CurrentFilePtr.QuadPart);
                }
                else{
                    printf("COuldnt get log file size, !VolInf->FullBackupExists\n");
                    DisplayError(GetLastError());
                }

                ReleaseMutex(VolInf->FileWriteMutex);
				
            }
            else{
                printf("Couldnt lock file write mutex @ setupstreamhandle, returning false\n");
                return FALSE;
            }

        }
        else{
            printf("Couldnt create log file for volume %c\n", VolInf->Letter);      
            DisplayError(GetLastError());
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
    
    return TRUE;

    // TODO (Batuhan): Remove that thing if 
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
            DisplayError((DWORD)hResult);
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
            
            printf("GPT Disk will be formatted as (Volume size %I64d, EFIPartitionSizeMB %u, RecoverySize %u)\n", M.VolumeSize, M.GPT_EFIPartitionSize/ (1024 * 1024), M.Size.Recovery);

            if (NarCreateCleanGPTBootablePartition(DiskID, (int)(M.VolumeSize / (1024ull * 1024ull)), (int)(M.GPT_EFIPartitionSize / (1024ull * 1024ull)), (int)(M.Size.Recovery / (1024ull * 1024ull)), (char)R->TargetLetter)) {
                Result = TRUE;
                printf("Successfully created bootable gpt partition\n");
            }
            else {
                printf("Can't create bootable GPT partition to restore\n");
            }
            
        }
        if (M.DiskType == NAR_DISKTYPE_MBR) {
            
            printf("MBR Disk will be formatted as (Volume size %I64d, SystemPartitionMB %u, RecoverySize %u)\n", M.VolumeSize, M.MBR_SystemPartitionSize/ (1024 * 1024), M.Size.Recovery);

            if (NarCreateCleanMBRBootPartition(DiskID, (char)R->TargetLetter, (int)(M.VolumeSize / (1024ull * 1024ull)), (int)(M.MBR_SystemPartitionSize / (1024ull * 1024ull)), (int)(M.Size.Recovery / (1024ull * 1024ull)))) {
                Result = TRUE;
            }
            else {
                printf("Can't create bootable MBR partition to restore\n");
            }
            
        }
    }
    else { //Is not OS volume
        if (M.DiskType == NAR_DISKTYPE_GPT) {
            
            if (NarCreateCleanGPTPartition(DiskID, M.VolumeSize / (1024 * 1024), (char)R->TargetLetter)) {
                Result = TRUE;
            }
            else {
                printf("Can't create clean GPT partition to restore\n");
            }
            
        }
        if (M.DiskType == NAR_DISKTYPE_MBR) {
            /*TODO*MBR */

            if ( FALSE) {
                // TODO(Batuhan):
                // Result;
            }
            else {
                printf("Can't create MBR partition to restore\n");
            }
            
        }
    }
    
    // TODO (Batuhan): wait 2 second prior to bcdboot to ensure volume is usable after diskpart operations 
    Sleep(1500); 

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
                NarRemoveLetter(NAR_EFI_PARTITION_LETTER);
                NarRemoveLetter(NAR_RECOVERY_PARTITION_LETTER);

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
    //HANDLE VolumeToRestore = INVALID_HANDLE_VALUE;
    
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
            
            printf("Error RegionsMetadata %c\n", BM.Errors.RegionsMetadata);
            printf("Error Regions %c\n", BM.Errors.Regions);
            printf("Error MFTMetadata %c\n", BM.Errors.MFTMetadata);
            printf("Error MFT %c\n", BM.Errors.MFT);
            printf("Error Recovery %c\n", BM.Errors.Recovery);

            Result = FALSE;
        }
    }
    
    
    if (VolumeSize != 0) {
        
        if (ShouldFormat) {
            
            if (NarFormatVolume((char)R->TargetLetter)) {
                if (NarSetVolumeSize((char)R->TargetLetter, (int)(VolumeSize / (1024ull * 1024ull)))) {
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
        Sleep(2500);

        HANDLE Volume = NarOpenVolume((char)R->TargetLetter);
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

inline BOOLEAN
NarRemoveLetter(char Letter){
    char Buffer[128];

    sprintf(Buffer, 
        "select volume %c\n"
        "remove letter %c\n"
        "exit\n", Letter, Letter);


    char InputFN[] = "NARDPINPUT";
    if(NarDumpToFile(InputFN, Buffer, (unsigned int)strlen(Buffer))){
        sprintf(Buffer, "diskpart /s %s", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }

    printf("COULDNT DUMP CONTENTS TO FILE, RETURNING FALSE CODE (NARREMOVELETTER)\n");
    return FALSE;
}

inline BOOLEAN
NarFormatVolume(char Letter) {
    
    char Buffer[2096];
    memset(Buffer, 0, 2096);
    
    sprintf(Buffer, ""
            "select volume %c\n"
            "format fs = ntfs quick override\n"
            "exit\n", Letter);
    
    char InputFN[] = "NARDPINPUT";
    if (NarDumpToFile(InputFN, Buffer, (unsigned int)strlen(Buffer))) {
        sprintf(Buffer, "diskpart /s %s", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    
    return FALSE;
}

inline void
NarRepairBoot(char OSVolumeLetter) {
    char Buffer[2096];
    sprintf(Buffer, "bcdboot %c:\\Windows /s %c: /f ALL", OSVolumeLetter, NAR_EFI_PARTITION_LETTER);
    system(Buffer);
}


void
FreeBackupMetadataEx(backup_metadata_ex* BMEX) {
    if(BMEX){
        FreeDataArray(&BMEX->RegionsMetadata);
        free(BMEX);
    }
}


file_read
NarReadFile(const char* FileName) {
    file_read Result = { 0 };
    HANDLE File = CreateFileA(FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if (File != INVALID_HANDLE_VALUE) {
        DWORD BytesRead = 0;
        Result.Len = (int)GetFileSize(File, 0); // safe to assume file size < 2 GB
        if (Result.Len == 0) return Result;
        Result.Data = malloc((size_t)Result.Len);
        if (Result.Data) {
            ReadFile(File, Result.Data, (DWORD)Result.Len, &BytesRead, 0);
            if (BytesRead == (DWORD)Result.Len) {
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
        Result.Len = (DWORD)GetFileSize(File, 0); // safe conversion
        if (Result.Len == 0) return Result;
        Result.Data = malloc((size_t)Result.Len);
        if (Result.Data) {
            ReadFile(File, Result.Data, Result.Len, &BytesRead, 0);
            if (BytesRead == (DWORD)Result.Len) {
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

inline void
FreeFileRead(file_read FR) {
    free(FR.Data);
}


inline BOOLEAN
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
    int VolSizeMB = 0;
    char Buffer[1024];
    char FNAME[] = "NARDPSCRPT";
    sprintf(Buffer, "%c:\\", Letter);
    
    ULARGE_INTEGER TOTAL_SIZE = { 0 };
    GetDiskFreeSpaceExA(Buffer, 0, &TOTAL_SIZE, 0);
    VolSizeMB = (int)(TOTAL_SIZE.QuadPart / (1024ull * 1024ull)); // byte to MB

    printf("Volume %c's size is %i, target size %i\n", Letter, VolSizeMB, TargetSizeMB);

    if (VolSizeMB == TargetSizeMB) {
        return TRUE;
    }
    
    /*
  extend size = X // extends volume by X. Doesnt resize, just adds X
  shrink desired = X // shrink volume to X, if X is bigger than current size, operation fails
  */
    if (VolSizeMB > TargetSizeMB) {
        // shrink volume
        ULONGLONG Diff = (unsigned int)(VolSizeMB - TargetSizeMB);
        sprintf(Buffer, "select volume %c\nshrink desired = %I64u\nexit\n", Letter, Diff);
    }
    else if(VolSizeMB < TargetSizeMB){
        // extend volume
        ULONGLONG Diff = (unsigned int)(TargetSizeMB - VolSizeMB);
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
    
    if (NarDumpToFile(FileName, Buffer, (unsigned int)strlen(Buffer))) {
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
    Result.Data = (disk_information*)malloc(sizeof(disk_information) * 32);
    Result.Count = 32;
    memset(Result.Data, 0, sizeof(disk_information) * 32);

    DWORD DriveLayoutSize = 1024 * 4;
    int DGEXSize = 1024 * 4;
    
    DWORD MemorySize = DriveLayoutSize + DGEXSize;
    void* Memory = VirtualAlloc(0, MemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);


    DRIVE_LAYOUT_INFORMATION_EX* DriveLayout = (DRIVE_LAYOUT_INFORMATION_EX*)(Memory);
    DISK_GEOMETRY_EX* DGEX = (DISK_GEOMETRY_EX*)((char*)Memory + DriveLayoutSize);    
    
    
    char StrBuf[255];
    int Found = 0;
    DWORD HELL;
    for (unsigned int DiskID = 0; DiskID < 32; DiskID++){
        
        sprintf(StrBuf, "\\\\?\\PhysicalDrive%i", DiskID);
        HANDLE Disk = CreateFileA(StrBuf, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0,0);
        if(Disk == INVALID_HANDLE_VALUE){
            continue;
        }

        Result.Data[Found].ID = (int)DiskID;
        
        if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DriveLayout, DriveLayoutSize, &HELL, 0)) {

            if (DriveLayout->PartitionStyle == PARTITION_STYLE_MBR) {
                Result.Data[Found].Type = NAR_DISKTYPE_MBR;
            }
            if (DriveLayout->PartitionStyle == PARTITION_STYLE_GPT) {
                Result.Data[Found].Type = NAR_DISKTYPE_GPT;
            }
            if (DriveLayout->PartitionStyle == PARTITION_STYLE_RAW) {
                Result.Data[Found].Type = NAR_DISKTYPE_RAW;
            }

            if(DriveLayout->PartitionStyle != PARTITION_STYLE_RAW
                && DriveLayout->PartitionStyle != PARTITION_STYLE_GPT
                && DriveLayout->PartitionStyle != PARTITION_STYLE_MBR){
                printf("Drive layout style was undefined, %s\n", StrBuf);
                continue;
            }

        }
        else {
            printf("Can't get drive layout for disk %s\n", StrBuf);
        }

        if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, 0, 0, DGEX, (DWORD)DGEXSize, &HELL, 0)) {
            Result.Data[Found].Size = (ULONGLONG)DGEX->DiskSize.QuadPart;
        }
        else {
            printf("Can't get drive layout for disk %s\n", StrBuf);
        }

        CloseHandle(Disk);
        memset(Memory, 0, MemorySize);
        Found++;

    }
    
    
    VirtualFree(Memory, 0, MEM_RELEASE);
    if (Found) {
        Result.Count = Found;
        Result.Data = (disk_information*)realloc(Result.Data, sizeof(Result.Data[0]) * Result.Count);
    }

    
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
    
    unsigned int DGEXSize = 1024 * 1; // 1 KB
    DISK_GEOMETRY_EX* DGEX = (DISK_GEOMETRY_EX*)malloc(DGEXSize);
    if (DGEX == NULL) {
        // TODO(Batuhan): Failed to allocate memory
    }
    
    HANDLE Disk = CreateFileA(StrBuffer,
                              GENERIC_WRITE | GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    
    if (Disk != INVALID_HANDLE_VALUE) {
        if (DeviceIoControl(Disk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, 0, 0, DGEX, DGEXSize, &Temp, 0)) {
            Result = (ULONGLONG)DGEX->DiskSize.QuadPart;
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


inline BOOLEAN
NarCreateCleanGPTBootablePartition(int DiskID, int VolumeSizeMB, int EFISizeMB, int RecoverySizeMB, char Letter) {
    
    char Buffer[2096];
    memset(Buffer, 0, 2096);
    
    {
        ULONGLONG TotalSize = ((ULONGLONG)VolumeSizeMB * 1024ull * 1024ull) + ((ULONGLONG)EFISizeMB * 1024ull * 1024ull) + ((ULONGLONG)RecoverySizeMB * 1024ull * 1024ull);
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
            "gpt attributes = 0x8000000000000001\n"
            "create partition efi size = %i\n" //efi partition, ARG3 EFIPartitionSize
            "format fs = fat32 quick\n"
            "assign letter S\n"
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

inline BOOLEAN
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
NarCreateCleanMBRPartition(int DiskID, char VolumeLetter, int VolumeSize) {

    char Buffer[2048];
    sprintf(Buffer, 
        "select disk %i\n"
        "clean\n"
        "convert mbr\n"
        "create partition primary size %i\n"
        "format fs = ntfs quick\n"
        "assign letter %c\n"
        "exit\n",
        DiskID, VolumeSize, VolumeLetter
        );

    char InputFN[] = "NARDPINPUT";
    if (NarDumpToFile(InputFN, Buffer, strlen(Buffer))) {
        sprintf(Buffer, "diskpart /s %s", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    

    printf("NarCreateCleanMBRPartition IS NOT IMPLEMENTED\n");
    return FALSE;

}


inline BOOLEAN
NarCreateCleanMBRBootPartition(int DiskID, char VolumeLetter, int VolumeSizeMB, int SystemPartitionSizeMB, int RecoveryPartitionSizeMB) {
    
    char Buffer[2048];
    memset(Buffer, 0, 2048);

    sprintf(Buffer, 
        "select disk %i\n"
        "clean\n"
        "convert mbr\n"
        // SYSTEM PARTITION
        "create partition primary size = %i\n"
        "format quick fs = ntfs label = System\n"
        "active"
        // WINDOWS PARTITION
        "create partition primary size %i\n" // in MB
        "format quick fs = ntfs label = Windows\n"
        "assign letter %c\n"
        //Recovery tools
        "create partition primary\n"
        "set id = 27\n"
        "exit\n", 
        DiskID, SystemPartitionSizeMB, VolumeLetter, VolumeSizeMB, VolumeLetter);


    char InputFN[] = "NARDPINPUTFORMBR";
    if (NarDumpToFile(InputFN, Buffer, strlen(Buffer))) {
        sprintf(Buffer, "diskpart /s %s", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }


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
            // printf("Couldn't lock volume %c, returning INVALID_HANDLE_VALUE\n", Letter);
        }
        
        
        if (DeviceIoControl(Volume, FSCTL_DISMOUNT_VOLUME, 0, 0, 0, 0, 0, 0)) {
            
        }
        else {
            // printf("Couldnt dismount volume\n");
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


#define GENERATE_LOG_FILE_NAME(letter) L"LOGFILE_##letter"
inline std::wstring
GenerateLogFileName(wchar_t Letter) {
    std::wstring Result = L"LOGFILE_";
    Result += (Letter);
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


#if 0
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

#endif

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
    std::wstring FileName = RootDir + GenerateMetadataName(Letter, Version);
    
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
        free(BMEX);
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
             data_array<nar_record> BackupRegions, HANDLE VSSHandle, data_array<nar_record> MFTLCN) {
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

    if(MFTLCN.Data == NULL){
        MFTLCN = GetMFTLCN(Letter, VSSHandle);
    }

    

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
    

    /*
layout of disks (fundamental level, gpt might contain extra data partitions)
for extra information visit:
MBR https://docs.microsoft.com/en-us/windows-hardware/manufacture/desktop/configure-biosmbr-based-hard-drive-partitions  
GPT https://docs.microsoft.com/en-us/windows-hardware/manufacture/desktop/configure-uefigpt-based-hard-drive-partitions

    GPT
System
Msr
windows
recovery


MBR
system
windows
recovery

    */
    // NOTE(Batuhan): Recovery partition's data is stored on metadata only if it's both full and OS volume backup

    

#if 1

    if (BM.IsOSVolume && BM.Version == NAR_FULLBACKUP_VERSION) {
        
        // recovery partition exists for both GPT and MBR, AppendRecoveryToFile can detect disk type and append it accordingly
        ULONGLONG OldFilePointer = NarGetFilePointer(MetadataFile);
        if (AppendRecoveryToFile(MetadataFile, BM.Letter)) {
            ULONGLONG CurrentFilePointer = NarGetFilePointer(MetadataFile);
            BM.Size.Recovery = CurrentFilePointer - OldFilePointer;
            printf("Appended recovery partition to metadata, size %I64d\n", BM.Size.Recovery);
        }
        else {
            printf("Error occured while appending recovery partition to backup metadata file for volume %c\n", BM.Letter);
            ULONGLONG CurrentFilePointer = NarGetFilePointer(MetadataFile);
            BM.Size.Recovery = CurrentFilePointer - OldFilePointer;
            BM.Errors.Recovery = TRUE;
        }
        
    }

    if(BM.IsOSVolume){

        GUID GUIDMSRPartition = { 0 }; // microsoft reserved partition guid
        GUID GUIDSystemPartition = { 0 }; // efi-system partition guid
        GUID GUIDRecoveryPartition = { 0 }; // recovery partition guid

        StrToGUID("{e3c9e316-0b5c-4db8-817d-f92df00215ae}", &GUIDMSRPartition);
        StrToGUID("{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}", &GUIDSystemPartition);
        StrToGUID("{de94bba4-06d1-4d40-a16a-bfd50179d6ac}", &GUIDRecoveryPartition);


        int DiskID = NarGetVolumeDiskID(BM.Letter);
        char DiskPath[128];
        sprintf(DiskPath, "\\\\?\\PhysicalDrive%i", DiskID);
        HANDLE DiskHandle = CreateFileA(DiskPath, GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        if(DiskHandle != INVALID_HANDLE_VALUE){

            DWORD DLSize = 1024*2;
            DRIVE_LAYOUT_INFORMATION_EX *DL = (DRIVE_LAYOUT_INFORMATION_EX*)malloc(DLSize);
            DWORD Hell = 0;
            if (DeviceIoControl(DiskHandle, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, DL, DLSize, &Hell, 0)) {
                
                if (DL->PartitionStyle == PARTITION_STYLE_GPT) {

                    for (unsigned int PartitionIndex = 0; PartitionIndex < DL->PartitionCount; PartitionIndex++) {

                        PARTITION_INFORMATION_EX* PI = &DL->PartitionEntry[PartitionIndex];
                        // NOTE(Batuhan): Finding recovery partition via GUID
                        if (IsEqualGUID(PI->Gpt.PartitionType, GUIDRecoveryPartition)) {
                            BM.Size.Recovery = PI->PartitionLength.QuadPart;
                        }
                        if(IsEqualGUID(PI->Gpt.PartitionType, GUIDSystemPartition)){
                            BM.GPT_EFIPartitionSize = PI->PartitionLength.QuadPart;
                        }

                    }

                }
                else if(DL->PartitionStyle == PARTITION_STYLE_MBR){

                    for (unsigned int PartitionIndex = 0; PartitionIndex < DL->PartitionCount; ++PartitionIndex){
                        
                        PARTITION_INFORMATION_EX* PI = &DL->PartitionEntry[PartitionIndex];
                        // System partition
                        if (PI->Mbr.BootIndicator == TRUE) {
                            BM.MBR_SystemPartitionSize = PI->PartitionLength.QuadPart;
                            break;
                        }
                        

                    }

                }
                
                
            }
            else {
                printf("DeviceIoControl with argument IOCTL_DISK_GET_DRIVE_LAYOUT_EX failed for drive %i, cant append recovery file to backup metadata\n", DiskID, Letter);
                DisplayError(GetLastError());
            }
            
            free(DL);
            CloseHandle(DiskHandle);

        }
        else{
            printf("Couldn't open disk %i as file, for volume %c's metadata @ version %i\n", DiskID, BM.Letter, BM.Version);
        }
        
    }

#endif

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
    else{
        printf("Couldnt write %i bytes to metadata, instead written %i bytes\n", sizeof(BM), BytesWritten);
        DisplayError(GetLastError());
    }
    
    Exit:
    CloseHandle(MetadataFile);
    FreeDataArray(&MFTLCN);
    
    printf("SaveMetadata returns %i\n", Result);
        
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

inline ULONGLONG
NarGetFilePointer(HANDLE F) {
    LARGE_INTEGER M = { 0 };
    LARGE_INTEGER N = { 0 };
    SetFilePointerEx(F, M, &N, FILE_CURRENT);
    return N.QuadPart;
}

BOOLEAN
RestoreRecoveryFile(restore_inf R) {
    //GUID GREC = { 0 }; // recovery partition guid
    //StrToGUID("{de94bba4-06d1-4d40-a16a-bfd50179d6ac}", &GREC);
    
    BOOLEAN Result = FALSE;
    HANDLE MetadataFile = INVALID_HANDLE_VALUE;
    
    backup_metadata B = ReadMetadata(R.SrcLetter, NAR_FULLBACKUP_VERSION, R.RootDir);
    //TODO(Batuhan) check integrity of B
    std::wstring MFN = R.RootDir;
    MFN += GenerateMetadataName(R.SrcLetter, NAR_FULLBACKUP_VERSION);
    
    HANDLE RecoveryPartitionHandle = NarOpenVolume(NAR_RECOVERY_PARTITION_LETTER);
    if (RecoveryPartitionHandle == INVALID_HANDLE_VALUE) {
        
        HANDLE MetadataFile = CreateFileW(MFN.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        if (MetadataFile != INVALID_HANDLE_VALUE) {

            if (NarSetFilePointer(MetadataFile, B.Offset.Recovery)) {
                if (CopyData(RecoveryPartitionHandle, MetadataFile, B.Size.Recovery)) {
                    //NOTE(Batuhan): Success
                    printf("Successfully restored recovery partition from backup metadata\n");
                    Result = TRUE;

                }
                else {
                    printf("Couldnt copy recovery file to disk\n");
                }
            }
            else {
                printf("Couldnt set metadata file pointer to where recovery partition resides\n");
            }

        }
        else {
            printf("Cant open metadata file %S\n", MFN.c_str());
        }

    }
    else {
        printf("Couldnt open recovery partition's volume (%c) as volume", NAR_RECOVERY_PARTITION_LETTER);
    }
    
    CloseHandle(MetadataFile);
    CloseHandle(RecoveryPartitionHandle);

    return Result;
}

BOOLEAN
AppendRecoveryToFile(HANDLE File, char Letter) {
    
    GUID GREC = { 0 }; // recovery partition guid
    StrToGUID("{de94bba4-06d1-4d40-a16a-bfd50179d6ac}", &GREC);
    
    BOOLEAN Result = FALSE;
    DWORD BufferSize = 1024 * 2; // 64KB
    
    
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
                            
                            NarSetFilePointer(Disk, (ULONGLONG)PI->StartingOffset.QuadPart);
                            if (CopyData(Disk, File, (ULONGLONG)PI->PartitionLength.QuadPart)) {
                                Result = TRUE;
                                printf("[AppendRecoveryToFile]: Successfully appended recovery partition to given handle\n");
                                //NOTE(Batuhan): Success
                            }
                            else {
                                printf("Couldnt restore recovery partition from backup metadata file\n");
                            }
                            
                        }
                        
                    }

                }
                else if(DL->PartitionStyle == PARTITION_STYLE_MBR){
                    // TODO
                    

                    for (unsigned int PartitionIndex = 0; PartitionIndex < DL->PartitionCount; ++PartitionIndex){
                        
                        PARTITION_INFORMATION_EX* PI = &DL->PartitionEntry[PartitionIndex];
                        
                        //

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
    int MaxBackupCount = BufferSize / (int)sizeof(*B);
    
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
                            

                            BackupFound++;
                            printf("Backup found %S\n", FDATA.cFileName);

#if 0
                            //NOTE(Batuhan): check if actual binary data exists in path and valid in size
                            wcscpy(wstrbuffer, Directory);
                            wcscat(wstrbuffer, L"\\");
                            wcscat(wstrbuffer, GenerateBinaryFileName(B[BackupFound].Letter, B[BackupFound].Version).c_str());
                            if (PathFileExistsW(wstrbuffer)) {
                                if (NarGetFileSize(wstrbuffer) == B[BackupFound].Size.Regions) {
                                }
                            }
#endif

                        }
                        else {
                            //NOTE(Batuhan): File name indicated from metadata and actual file name does NOT match.
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



inline BOOLEAN
AppendMFTFile(HANDLE File, HANDLE VSSHandle, data_array<nar_record> MFTLCN, int ClusterSize) {
    
    BOOLEAN Return = FALSE;
    
    if (File != INVALID_HANDLE_VALUE && VSSHandle != INVALID_HANDLE_VALUE) {
        
        LARGE_INTEGER MoveTo = { 0 };
        LARGE_INTEGER NewFilePointer = { 0 };
        BOOL Result = 0;
        for (unsigned int i = 0; i < MFTLCN.Count; i++) {
            MoveTo.QuadPart = (LONGLONG)MFTLCN.Data[i].StartPos * (LONGLONG)ClusterSize;
            
            Result = SetFilePointerEx(VSSHandle, MoveTo, &NewFilePointer, FILE_BEGIN);
            if (!SUCCEEDED(Result) || MoveTo.QuadPart != NewFilePointer.QuadPart) {
                printf("Set file pointer failed\n");
                printf("Tried to set pointer -> %I64d, instead moved to -> %I64d\n", MoveTo.QuadPart, NewFilePointer.QuadPart);
                printf("Failed at LCN : %i\n", i);
                DisplayError(GetLastError());
                goto Exit;
            }
            
            //printf("Will copy %I64u bytes to append MFT to file\n", (ULONGLONG)MFTLCN.Data[i].Len * (ULONGLONG)ClusterSize);

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
    
    printf("Entered NarLoadBootState\n");
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
                
                printf("Found previous backup files, will initialize CONTEXT accordingly\n");

                DWORD BR = 0;
                nar_boot_track_data* BootTrackData = (nar_boot_track_data*)malloc(Size);
                if (BootTrackData != NULL) {
                    
                    if (ReadFile(File, BootTrackData, Size, &BR, 0) && BR == Size) {
                        volume_backup_inf VolInf;
                        memset(&VolInf, 0, sizeof(VolInf));
                        int BootTrackDataCount = (int)(Size / (DWORD)sizeof(BootTrackData[0]));
                        
                        for (int i = 0; i < BootTrackDataCount; i++) {
                            
                            
                            bResult = InitVolumeInf(&VolInf, (char)BootTrackData[i].Letter, (BackupType)BootTrackData[i].BackupType);
                            if (bResult) {
                                
                                VolInf.FilterFlags.IsActive = TRUE;
                                VolInf.FullBackupExists = TRUE;
                                VolInf.Version = BootTrackData[i].Version;
                                std::wstring LogFileName = GenerateLogFileName(VolInf.Letter);

                                VolInf.LogHandle = CreateFileW(LogFileName.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
                                
                                if (VolInf.LogHandle != INVALID_HANDLE_VALUE) {
                                    // succ created file, we can continue logging.
                                    printf("Succ add volume %c to list at version %i\n", VolInf.Letter, VolInf.Version);
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
            printf("File %s doesnt exist\n", FileNameBuffer);
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
    Saves the current program state into a file, so next time computer boots driver can recognize it 
    and setup itself accordingly.

    saved states:
        - Letter (user-kernel)
        - Version (user)
        - BackupType (user)
        - LastBackupRegionOffset (user)
*/

BOOLEAN
NarSaveBootState(LOG_CONTEXT* CTX) {

    if (CTX == NULL || CTX->Volumes.Data == NULL){
         printf("Either CTX or its volume data was NULL, terminating NarSaveBootState\n");
         return FALSE;
    }
    
    BOOLEAN Result = TRUE;
    nar_boot_track_data Pack;
    
    char FileNameBuffer[64];
    if (GetWindowsDirectoryA(FileNameBuffer, 64)) {
        strcat(FileNameBuffer, "\\");
        strcat(FileNameBuffer, NAR_BOOTFILE_NAME);

        HANDLE File = CreateFileA(FileNameBuffer, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
        if (File != INVALID_HANDLE_VALUE) {
            printf("Created file to write boot information\n");
            for (unsigned int i = 0; i < CTX->Volumes.Count; i++) {
                Pack.Letter = 0;
                Pack.Version = -2;
                
                if (!CTX->Volumes.Data[i].INVALIDATEDENTRY) {
                    
                    Pack.Letter = (char)CTX->Volumes.Data[i].Letter;
                    Pack.Version = (char)CTX->Volumes.Data[i].Version; // for testing purposes, 128 version looks like fine
                    Pack.BackupType = (char)CTX->Volumes.Data[i].BT;
                    Pack.LastBackupOffset = (UINT64)CTX->Volumes.Data[i].LastBackupRegionOffset;

                    DWORD BR = 0;
                    
                    if (WriteFile(File, &Pack, sizeof(Pack), &BR, 0) && BR == sizeof(Pack)) {
                        printf("Successfully saved volume [%c]'s [%i]th version information to boot file\n", Pack.Letter, Pack.Version);
                    }
                    else {
                        printf("Coulndt save volume [%c]'s [%i]th version information to boot file\n", Pack.Letter, Pack.Version);
                        Result = FALSE;
                    }
                    
                }
                else{
                    continue;    
                }

                
            }
            
            
        }
        else {
            printf("Couldnt open file %s, can not save boot information\n", FileNameBuffer);
            Result = FALSE;
        }
        
        CloseHandle(File);
            
    }
    
    
    return Result;
    
}





#if 1

#define REGION(Start, End) nar_record{(Start), (End) - (Start)}

// Intented usage: strings, not optimized for big buffers, used only in kernel mode to detect some special directory activity
BOOLEAN
NarSubMemoryExists(void *mem1, void* mem2, int mem1len, int mem2len){
    
    char *S1 = (char*)mem1;
    char *S2 = (char*)mem2;


    int k = 0;
    int j = 0;

    while (k < mem1len && j < mem2len){
        
        if(S1[k] == S2[j]){
            
            int temp = k;
            int found = FALSE;
            
            while(k < mem1len && j < mem2len){
                if(S1[k] == S2[j]){
                    j++;
                    k++;
                }
                else {
                    break;
                }
            }

            if(j == mem2len){
                return TRUE;
            }
            
            k = temp;

        }
        else{
            k++;
            j = 0;
        }

    }

    return FALSE;
}



void foo(){

    nar_record *VolumeRecords;
    file_read f = NarReadFile("records");
    VolumeRecords = (nar_record*)f.Data; 
    UINT32 RecordCount = f.Len / sizeof(nar_record);

    ULONGLONG TargetFilePointer = 0;

    NarCreateCleanGPTBootablePartition(1, 61440, 100, 529, 'E');

    HANDLE OutputFile = CreateFile(L"BinaryFile", GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if(OutputFile == INVALID_HANDLE_VALUE) {
        printf("Couldnt open outputfile \n");
        return;
    }
    
    if (NarSetFilePointer(OutputFile, 0)) {

        HANDLE VolumeHandle = NarOpenVolume('E');
        
        if (VolumeHandle != INVALID_HANDLE_VALUE) {

            for (int i = 0; i < RecordCount; i++) {

                TargetFilePointer = (ULONGLONG)VolumeRecords[i].StartPos * (ULONGLONG)4096;
                if (NarSetFilePointer(VolumeHandle, TargetFilePointer)) {

                    ULONGLONG CopyLen = (ULONGLONG)VolumeRecords[i].Len * (ULONGLONG)4096;
                    if (!CopyData(OutputFile, VolumeHandle, CopyLen)) {
                        printf("Couldnt copy %I64d bytes from starting pos of %I64d\n", (ULONGLONG)VolumeRecords[i].Len * (ULONGLONG)4096, TargetFilePointer);
                    }

                }

            }

            printf("DONE RESTORE OPERATION\n");

        }
        else {

        }

    }
    else {
        printf("Couldnt set file pointer to beginning of the file\n");
    }


}

inline void
NarPushDirectoryStack(nar_backup_file_explorer_context *Ctx, UINT64 ID) {
    Ctx->HistoryStack.S[++Ctx->HistoryStack.I] = ID;
}

inline void
NarPopDirectoryStack(nar_backup_file_explorer_context* Ctx) {
    Ctx->HistoryStack.I--;
    if (Ctx->HistoryStack.I < 0) {
        Ctx->HistoryStack.I = 0;
    }
}

inline void
NarGetFileListFromMFTID(nar_file_entries_list* EList, UINT64 TargetMFTID, nar_record* MFTRegions, UINT32 MFTRegionCount, UINT32 ClusterSize, nar_fe_volume_handle FEHandle) {

    if (!EList) return;

    if (ClusterSize < 1024) {
        *(int*)0 = 0;
    }

    UINT64 FileCountInRegion = 0;
    UINT32 FileCountPerCluster = ClusterSize / 1024;
    UINT64 AdvancedFileCountSoFar = 0;

    UINT64 FileOffsetFromRegion = 0;
    UINT64 FileOffsetAtVolume = 0;

    for (UINT32 RegionIndex = 0; RegionIndex < MFTRegionCount; RegionIndex++) {

        FileCountInRegion = (UINT64)MFTRegions[RegionIndex].Len * (UINT64)FileCountPerCluster;
        if (FileCountInRegion + AdvancedFileCountSoFar >= TargetMFTID) {
            // found
            FileOffsetFromRegion = TargetMFTID - AdvancedFileCountSoFar;
            FileOffsetAtVolume = (UINT64)MFTRegions[RegionIndex].StartPos * (UINT64)ClusterSize + FileOffsetFromRegion * 1024;
            break;
        }

        AdvancedFileCountSoFar += FileCountInRegion;

    }


    if (NarFileExplorerSetFilePointer(FEHandle, FileOffsetAtVolume)) {

        BYTE Buffer[1024];
        memset(Buffer, 0, sizeof(Buffer));

        UINT32 IndexRegionsFound = 0;

        struct {
            UINT32 Start;
            UINT32 Len;
        }INDX_ALL_REGIONS[64];

        DWORD BytesRead = 0;

        if (NarFileExplorerReadVolume(FEHandle, Buffer, 1024, &BytesRead) && BytesRead == 1024) {

            // do parsing stuff
            void* FileRecord = &Buffer[0];

            INT16 FlagOffset = 22;
            INT32 NAR_INDEX_ALLOCATION_FLAG = 0xA0;

            INT16 Flags = *(INT16*)((BYTE*)FileRecord + FlagOffset);
            INT16 DirectoryFlag = 0x0002;

            if((Flags & DirectoryFlag) == DirectoryFlag){

                INT16 FirstAttributeOffset = (*(INT16*)((BYTE*)FileRecord + 20));
                void* FileAttribute = (char*)FileRecord + FirstAttributeOffset;

                INT32 RemainingLen = *(INT32*)((BYTE*)FileRecord + 24); // Real size of the file record
                RemainingLen -= (FirstAttributeOffset + 8); //8 byte for end of record mark, remaining len includes it too.

                // iterate until file attr 0xA0 found, which is INDEX_ALLOCATION
                // maybe some dir does not contain INDEX_ALLOCATION, like empty dirs, or other things i am not aware of.
                // its better write solid solution

                while (RemainingLen > 0) {


                    // check INDEX_ROOT
                    if ((*(INT32*)FileAttribute & 0x90) == 0x90) {
                        
                        UINT32 LenIncludingHeader = *(UINT32*)((char*)FileAttribute + 4);
                        UINT32 AttributeLen = *(UINT32*)((char*)FileAttribute + 16);
                        if (AttributeLen != 56 || LenIncludingHeader != 88) {
                            // entries start after 64 bytes

                            void *Entry = (char*)FileAttribute + 64;


                            
                            while (1) {


                                UINT16 EntrySize = *(UINT16*)((char*)Entry + 8);
                                UINT16 AttrSize = *(UINT16*)((char*)Entry + 10);
                                if (EntrySize == 16 || AttrSize == 0) {
                                    // end mark
                                    break;
                                }


                                // check if POSIX entry
                                if (*((char*)Entry + NAR_POSIX_OFFSET) == NAR_POSIX) {
                                    Entry = (char*)Entry + EntrySize;
                                    continue;
                                }

                                UINT8 NameLen = *(UINT8*)((char*)Entry + NAR_NAME_LEN_OFFSET);
                                UINT64 MagicNumber = *(UINT64*)Entry;
                                MagicNumber = MagicNumber & (0x0000FFFFFFFFFFFF);

                                // DO MASK AND TRUNCATE 2 BYTES TO FIND MFTINDEX OF THE FILE


                                // useful information here
                                UINT64 CreationTime = *(UINT64*)((char*)Entry + NAR_TIME_OFFSET);
                                UINT64 ModificationTime = *(UINT64*)((char*)Entry+ NAR_TIME_OFFSET + 8);
                                UINT64 FileSize = *(UINT64*)((char*)Entry + NAR_SIZE_OFFSET);

                                char* NamePtr = ((char*)Entry + NAR_NAME_OFFSET);
                                if (*NamePtr == 0) {
                                    Entry = (char*)Entry + EntrySize;
                                    continue;
                                }

                                memset(&EList->Entries[EList->EntryCount], 0, sizeof(EList->Entries[EList->EntryCount]));

                                memcpy(&EList->Entries[EList->EntryCount].Name[0], NamePtr, NameLen * sizeof(wchar_t));

                                // extracted all useful information, push it to the list and continue

                                // push stuff
                                // represents files in directory
                                
                                EList->Entries[EList->EntryCount].MFTFileIndex = MagicNumber;
                                EList->Entries[EList->EntryCount].Size = FileSize;
                                EList->Entries[EList->EntryCount].CreationTime = CreationTime;
                                EList->Entries[EList->EntryCount].LastModifiedTime = ModificationTime;
                                
                                EList->EntryCount++;


                                /*
                                struct FileEntriesList{
                                 UINT32 MFTIndex;
                                 UINT32 EntryCount;
                                 NarFileEntry *Entries;
                                };


                                // could be either file or dir
                                struct NarFileEntry{

                                 BOOLEAN Flags;

                                 UINT64 MFTFileIndex;
                                 UINT64 Size; // file size in bytes

                                 UINT64 CreationTime;
                                 UINT64 LastModifiedTime;

                                 wchar_t Name[MAX_PATH + 1]; // max path + 1 for null termination
                                };
                                */

                                Entry = (char*)Entry + EntrySize;

                            }




                        }
                        else {
                            // index_root is empty
                        }

                    }

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


                        INDX_ALL_REGIONS[IndexRegionsFound].Start = FirstCluster;
                        INDX_ALL_REGIONS[IndexRegionsFound].Len = ClusterCount;
                        IndexRegionsFound++;

                        //DBG_INC(DBG_INDX_FOUND);
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

                            INDX_ALL_REGIONS[IndexRegionsFound].Start = FirstCluster;
                            INDX_ALL_REGIONS[IndexRegionsFound].Len = ClusterCount;
                            IndexRegionsFound++;

                            //DBG_INC(DBG_INDX_FOUND);

                        }


                        // found INDEX_ALLOCATION attribute
                        break;
                    }

                    if ((*(INT32*)FileAttribute & NAR_INDEX_ALLOCATION_FLAG) > NAR_INDEX_ALLOCATION_FLAG) break;// early termination


                    // found neither indexallocation nor attr bigger than it, just substract attribute size from remaininglen to determine if we should keep iterating
                    INT32 AttrSize = (*(unsigned short*)((BYTE*)FileAttribute + 4));
                    RemainingLen -= AttrSize;
                    FileAttribute = (BYTE*)FileAttribute + AttrSize;
                    if (AttrSize == 0) break;

                }


            }



            for (UINT32 IndexRegionIndex = 0; IndexRegionIndex < IndexRegionsFound; IndexRegionIndex++) {

                if (NarFileExplorerSetFilePointer(FEHandle, (UINT64)INDX_ALL_REGIONS[IndexRegionIndex].Start * (UINT64)ClusterSize)) {

                    size_t IndxBufferSize = (UINT64)INDX_ALL_REGIONS[IndexRegionIndex].Len * (UINT64)ClusterSize;

                    void* IndxBuffer = malloc(IndxBufferSize);

                    if (IndxBuffer) {

                        
                        if (NarFileExplorerReadVolume(FEHandle, IndxBuffer, (DWORD)IndxBufferSize, &BytesRead) && BytesRead == IndxBufferSize) {
                            
                            // _mm_prefetch((const char*)IndxBuffer, _MM_HINT_T0);

                            // inspect each clusters in region
                            for (UINT32 IndxRegionClusterIndex = 0; IndxRegionClusterIndex < (IndxBufferSize / 4096); IndxRegionClusterIndex++) {
                                

                                void* IndxCluster = (char*)IndxBuffer + (UINT64)IndxRegionClusterIndex * 4096;

                                // skip first 64 bytes, first file entry, 88 if ROOT's first region, i hate ntfs
                                if (TargetMFTID == 5 && IndexRegionIndex == 0 && IndxRegionClusterIndex == 0) {
                                    IndxCluster = (char*)IndxCluster + 88;
                                }
                                else {
                                    IndxCluster = (char*)IndxCluster + 64;
                                }
                                
                                char ZeroBytes[24];
                                memset(ZeroBytes, 0, sizeof(ZeroBytes));

                                INT EntryParseResult = NAR_FEXP_END_MARK;
                                do {

                                    memset(&EList->Entries[EList->EntryCount], 0, sizeof(EList->Entries[EList->EntryCount]));
                                    EntryParseResult = NarFileExplorerGetFileEntryFromData(IndxCluster, &EList->Entries[EList->EntryCount]);
                                    
                                    if (EntryParseResult == NAR_FEXP_SUCCEEDED || EntryParseResult == NAR_FEXP_POSIX) {
                                        
                                        // do not accept posix files as FILE_ENTRY at all
                                        if (EntryParseResult == NAR_FEXP_SUCCEEDED) {
                                            EList->EntryCount++;
                                        }

                                        UINT16 EntrySize = NAR_FEXP_INDX_ENTRY_SIZE(IndxCluster);
                                        IndxCluster = (char*)IndxCluster + EntrySize;
                                    }
                                    

                                } while (EntryParseResult != NAR_FEXP_END_MARK);

#if 0

                                while (1) {

                                    // check if end of records
                                    if (memcmp(IndxCluster, &ZeroBytes[0], 24) == 0) {
                                        // end of records
                                        break;
                                    }

                                    UINT16 EntrySize = *(UINT16*)((char*)IndxCluster + 8);
                                    UINT16 AttrSize = *(UINT16*)((char*)IndxCluster + 10);
                                    if (EntrySize == 16 || AttrSize == 0) { 
                                        break; 
                                    }

                                    // check if POSIX entry
                                    if (*((char*)IndxCluster + NAR_POSIX_OFFSET) == NAR_POSIX) {
                                        IndxCluster = (char*)IndxCluster + EntrySize;
                                        continue;
                                    }

                                    UINT8 NameLen = *(UINT8*)((char*)IndxCluster + NAR_NAME_LEN_OFFSET);
                                    UINT64 MagicNumber = *(UINT64*)IndxCluster;
                                    MagicNumber = MagicNumber & (0x0000FFFFFFFFFFFF);

                                    // DO MASK AND TRUNCATE 2 BYTES TO FIND MFTINDEX OF THE FILE


                                    // useful information here
                                    UINT64 CreationTime = *(UINT64*)((char*)IndxCluster + NAR_TIME_OFFSET);
                                    UINT64 ModificationTime = *(UINT64*)((char*)IndxCluster + NAR_TIME_OFFSET + 8);
                                    UINT64 FileSize = *(UINT64*)((char*)IndxCluster + NAR_SIZE_OFFSET);

                                    char* NamePtr = ((char*)IndxCluster + NAR_NAME_OFFSET);
                                    if (*NamePtr == 0) {
                                        IndxCluster = (char*)IndxCluster + EntrySize;
                                        continue;
                                    }
                                    
                                    memset(&EList->Entries[EList->EntryCount], 0, sizeof(EList->Entries[EList->EntryCount]));

                                    memcpy(&EList->Entries[EList->EntryCount].Name[0], NamePtr, NameLen * sizeof(wchar_t));
                                    
                                    // extracted all useful information, push it to the list and continue

                                    // push stuff
                                    // represents files in directory
                                    //if (*NamePtr != 0 && TargetMFTID == 221313) {
                                    //    printf("%S\n", EList->Entries[EList->EntryCount].Name);
                                    //}
                                    EList->Entries[EList->EntryCount].MFTFileIndex = MagicNumber;
                                    EList->Entries[EList->EntryCount].Size = FileSize;
                                    EList->Entries[EList->EntryCount].CreationTime = CreationTime;
                                    EList->Entries[EList->EntryCount].LastModifiedTime = ModificationTime;
                                    
                                    EList->EntryCount++;

                                    
                                    /*
                                    struct FileEntriesList{
                                     UINT32 MFTIndex;
                                     UINT32 EntryCount;
                                     NarFileEntry *Entries;
                                    };


                                    // could be either file or dir
                                    struct NarFileEntry{

                                     BOOLEAN Flags;

                                     UINT64 MFTFileIndex;
                                     UINT64 Size; // file size in bytes

                                     UINT64 CreationTime;
                                     UINT64 LastModifiedTime;

                                     wchar_t Name[MAX_PATH + 1]; // max path + 1 for null termination
                                    };
                                    */

                                    IndxCluster = (char*)IndxCluster + EntrySize;

                                }


#endif

                            }



                        }
                        else {
                            // readfile failed for indx region
                        }

                        free(IndxBuffer);
                    }
                    else {
                        // printf("Couldnt allocate memory");
                    }

                }
                else {
                    // couldnt set file pointer for index's ith region
                }

            }


        }
        else {
            // couldnt read file entry
        }

    }
    else {
        // set file pointer failed
    }
    
    //if (1) {
    //    for (int i = 0; i < EList->EntryCount; i++) {
    //        if (EList->Entries[i].Size > 0) {
    //            printf("%S size %I64u %I64u\n", EList->Entries[i].Name, EList->Entries[i].Size, EList->Entries[i].MFTFileIndex);
    //        }
    //    }
    //}

    
}


#define NAR_FE_DIRECTORY_FLAG 0x01

inline void
RestoreFolder(nar_backup_file_explorer_context *ctx, UINT32 SelectedListID, const wchar_t *RootDir){

    NarFileExplorerPushDirectory(ctx, SelectedListID);

    for(int i = 0; i < ctx->EList.EntryCount; i++){

        // check if directory
        if(ctx->EList.Entries[i].Flags & NAR_FE_DIRECTORY_FLAG == NAR_FE_DIRECTORY_FLAG){

        }
        else{

        }


    }

}



inline void
NarFreeFileVersionStack(nar_file_version_stack Stack){
    
    if(Stack.FilePath != NULL) free(Stack.FilePath);
    if(Stack.FileAbsoluteMFTOffset != NULL) free(Stack.FileAbsoluteMFTOffset);

    //if(Stack->LCNRecords != NULL){
    //    
    //    for(int i = 0; i<VersionsFound; i++){
    //        FreeDataArray(Stack->LCNRecords);
    //    }
    //    free(Stack->LCNRecords);
    //
    //}
    

    if(Stack.RootDir != NULL) free(Stack.RootDir);

}

// Path: any path name that contains trailing backslash
// like = C:\\somedir\\thatfile.exe    or    \\relativedirectory\\dir2\\dir3\\ourfile.exe
// OutName: output buffer
// OutMaxLen: in characters
inline void
NarGetFileNameFromPath(const wchar_t* Path, wchar_t *OutName, INT32 OutMaxLen) {
    
    if (!Path || !OutName) return;

    memset(OutName, 0, OutMaxLen);

    size_t PathLen = wcslen(Path);

    if (PathLen > OutMaxLen) return;
    
    INT TrimPoint = PathLen - 1;
    {
        while (Path[TrimPoint] != L'\\' && --TrimPoint != 0);
    }
    
    if (TrimPoint < 0) TrimPoint = 0;

    memcpy(OutName, &Path[TrimPoint], 2 * (PathLen - TrimPoint));

}

/*
    Args: 
    RootDir: Path to backups to be searched
    FileName: File name to be searched in backup volumes, MUST BE FULL PATH LIKE C:\\PROGRAM FILES\\SOME PROGRAM\\RANDOM.EXE
    RestoreTo: Where to restore file, optional, if NULL, file will be restored to current directory


    Notes: This procedure automatically detects backup type and restores file accordingly. If backup type is == DIFFERENTIAL, only information we need is FIRST EXISTING VERSION + LATEST VERSION.
    However if type == INCREMENTAL, we MUST restore each file from FIRST EXISTING VERSION to LATEST BACKUP(which is Version passed by that function)

    For each version to be iterated, read MFT record of the file via FileStack, detect file LCN's, then read existing regions from backup(since backup may not contain every region on volume).
    Then convert these LCN's to VCN's, write it to actual file that exists in file system.

*/
inline BOOLEAN
NarRestoreFileFromBackups(wchar_t *RootDir, wchar_t *FileName, wchar_t *RestoreTo, wchar_t VolumeLetter, INT32 InVersion){


    if(RootDir == NULL || FileName == NULL) return FALSE;

    BOOLEAN Result;

    nar_file_version_stack FileStack = NarSearchFileInVersions(RootDir, VolumeLetter, InVersion, FileName);
    if(FileStack.VersionsFound < 0){
        printf("Failed to create file history stack for file %S for volume %c based on version %i\n", FileName, VolumeLetter, InVersion);
        goto ERROR_TERMINATE;
    }

    INT32 VersionUpTo = FileStack.StartingVersion + FileStack.VersionsFound;
    
    for (
        int VersionID = FileStack.StartingVersion;
        VersionID < VersionUpTo;  
        VersionID++) {
                    
        backup_metadata_ex *BMEX = InitBackupMetadataEx(VolumeLetter, VersionID, RootDir);
        if(BMEX == NULL){
            printf("Couldnt create backup_metadata_ex structure for volume %c for version %i to restore file %S\n", VolumeLetter, VersionID, FileName);
        }

        nar_fe_volume_handle FEHandle = { 0 };

        if (NarInitFEVolumeHandle(&FEHandle, NAR_FE_HAND_OPT_READ_BACKUP_VOLUME, VolumeLetter, VersionID, RootDir)) {
            
            
            

        }

        
        NarFreeFEVolumeHandle(FEHandle);

    }

    

    ERROR_TERMINATE:

    NarFreeFileVersionStack(FileStack);

}


/*
    Gets the intersection of the r1 and r2 arrays, writes new array to r3
*/
inline void
NarGetRegionIntersection(nar_record* r1, nar_record* r2, nar_record* r3, INT32 len1, INT32 len2, INT32 len3, INT32 *RegionsWritten) {
    
    if (!r1 || !r2 || !r3) return;

    int i1, i2, i3;
    i1 = i2 = i3 = 0;

    memset(r3, 0, len3 * sizeof(*r3));

    // logic behind iteration, ALWAYS iterate item that has LOWER END.
    while (TRUE) {
        
        if (i1 >= len1 || i2 >= len2 || i3 >= len3) {
            break;
        }
        
        UINT32 r1end = r1[i1].StartPos + r1[i1].Len;
        UINT32 r2end = r2[i2].StartPos + r2[i2].Len;

        /*
       r2          -----------------
       r1 -----
       not touching, iterate r1
        */
        if (r1end < r2[i2].StartPos) {
            i1++;
            continue;
        }
        /*
      r1          -----------------
      r2 -----
      not touching, iterate r1
       */

        if (r2end < r1[i1].StartPos) {
            i2++;
            continue;
        }
        
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
        UINT32 IntersectionEnd   = MIN(r1end, r2end);
        UINT32 IntersectionStart = MAX(r1[i1].StartPos, r2[i2].StartPos);
        
        r3[i3].StartPos = IntersectionStart;
        r3[i3].Len = IntersectionEnd - IntersectionStart;
        i3++;

        if (r1end < r2end) i1++;
        else               i2++;

    }
    
    *RegionsWritten = i3;

}

struct lcn_from_mft_query_result{
    
    enum{
        FAIL            = 0x0,
        SUCCESS         = 0x1,
        HAS_DATA_IN_MFT = 0x2,
        FILE_FITS_MFT   = 0x4
    };

    INT8 Flags;

    // valid if record contains file data in it
    INT16 DataOffset;
    INT16 DataLen;

    // i dont want to use dynamically allocated array then free it. Since these tasks are disk IO bounded, i can totally neglect cache behavior(thats a big sin) and preallocate big stack array and never have to deal with freeing it
    // probably %95 of it will be empty most of the time
    INT32 RecordCount;
    nar_record Records[1024];

};


inline lcn_from_mft_query_result
ReadLCNFromMFTRecord(void* RecordStart, nar_record* Records, INT32* RecordsFound) {
    // if file can fit in MFT, 

    lcn_from_mft_query_result Result;
    memset(&Result, 0, sizeof(Result));

    *RecordsFound = 0;

    if (Records == NULL || RecordStart == NULL || RecordsFound == NULL) return Result;

    if (*(INT32*)RecordStart != 'ELIF') {
        return Result;
    }

    void* FileRecord = RecordStart;
    INT16 FirstAttributeOffset = *(INT16*)((BYTE*)FileRecord + 20);
    void* FileAttribute = (BYTE*)FileRecord + FirstAttributeOffset;


    INT32 RemainingLen = *(INT32*)((BYTE*)FileRecord + 24); // Real size of the file record
    RemainingLen -= (FirstAttributeOffset + 8); //8 byte for end of record mark, remaining len includes it too.

    auto InsertToResult = [&](UINT32 Start, UINT32 Len) {
        Result.Records[Result.RecordCount++] = {Start, Len};
    };

    while(RemainingLen > 0){

        // 0x80 == $DATA
        if ((*(INT32*)FileAttribute & 0x80) == 0x80){


            // skip if data attribute is smt like Zone.Identifier or another alternate stream.
            UINT8 AttributeNameLen = *((UINT8*)FileAttribute + 9);
            if (AttributeNameLen != 0) {
                goto SKIP_POINT;
            }

            // skip if attribute represents resident data. this means file has some data in it's record itself. 
            UINT8 AttributeResident = *(UINT8*)FileAttribute + 8;
            if(AttributeResident){
                Result.Flags |= Result.HAS_DATA_IN_MFT;
                goto SKIP_POINT;
            }


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
            InsertToResult(FirstCluster, ClusterCount);
            
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


                FirstCluster += OldClusterStart;
                // Update tail
                OldClusterStart = FirstCluster;

                D = (BYTE*)D + (FirstClusterSize + ClusterCountSize + 1);

                InsertToResult(FirstCluster, ClusterCount);
                
                
            }
            break;

        }

        if ((*(INT32*)FileAttribute & 0x80) > 0x80){
            printf("Couldnt find 0x80 file attribute for $MFT\n");
            break;// early termination
        }



        SKIP_POINT:
        // found neither Data nor attr bigger than it, just substract attribute size from remaininglen to determine if we should keep iterating

        RemainingLen -= (*(INT32*)((BYTE*)FileAttribute + 4));
        FileAttribute = (BYTE*)FileAttribute + *(INT32*)((BYTE*)FileAttribute + 4);

    }


    if(Result.RecordCount == 0 
        && Result.Flags != Result.FAIL 
        && (Result.Flags & Result.HAS_DATA_IN_MFT) == Result.HAS_DATA_IN_MFT
        ){
        Result.Flags |= Result.FILE_FITS_MFT;
    }

    return Result;

}


// reads file's data in mft record and writes it to OutData, and sets DataLen to how many bytes written
// files that have exceeded 1KB limit after os file flush(since OS does NOT instantly writes to disk it rather flushes requests and flushes after some period) may have contain their first 1KB info at mft.
inline void
ReadFileDataFromMFT(void *RecordStart, lcn_from_mft_query_result QueryResult, void *OutData, INT32 *DataLen){


    if(RecordStart == NULL || OutData == NULL ||  DataLen == NULL){
        assert(FALSE);
        return;
    }

    if(QueryResult.Flags != QueryResult.FAIL){

        if(QueryResult.DataOffset + QueryResult.DataLen < 1024){            
            void *Data = (char*)RecordStart + QueryResult.DataOffset;
            INT32 DLen = QueryResult.DataLen;

            memcpy(OutData, Data, DLen);
            *DataLen = DLen;
        }

    }

}


/*
    Searches file FileName, in backup versions, which must exist in given RootDir. If RootDir is NULL it searches in
    where program runs, starts from StartingVersion and lowers it until either file is not found or fullbackup id is reached.

    That information is useful for pre restore operation. After that lookup we will be
*/
nar_file_version_stack
NarSearchFileInVersions(wchar_t *RootDir, wchar_t VolumeLetter, INT32 CeilVersion, wchar_t *FileName){

    nar_file_version_stack Result;
    memset(&Result, 0, sizeof(Result));

    if(FileName == NULL){
        return Result;
    }
        
    size_t RootDirSize = RootDir ? wcslen(FileName)*sizeof(wchar_t) : 0; 
    size_t FileNameSize = wcslen(FileName)*sizeof(wchar_t);

    // Reserve space via assuming file exists in all backups back to full backup. Since structure is small there isnt an overheat for memory
    Result.RootDir = (wchar_t*)malloc(RootDirSize);
    Result.FilePath = (wchar_t*)malloc(FileNameSize);

    memset(Result.RootDir, 0, RootDirSize);
    memset(Result.FilePath, 0, FileNameSize);

    

    size_t RelativeOffsetPtrSize = CeilVersion*sizeof(Result.FileAbsoluteMFTOffset);
    // size_t LCNRecordsPtrSize = CeilVersion*sizeof(Result.LCNRecords);

    Result.FileAbsoluteMFTOffset = (UINT64*)malloc(RelativeOffsetPtrSize);
    // Result.LCNRecords = (UINT64*)malloc(LCNRecordsPtrSize);
    
    memset(&Result.FileAbsoluteMFTOffset, 0, RelativeOffsetPtrSize);
    // memset(Result.LCNRecords, 0, LCNRecordsPtrSize);


    INT32 StartingVersion = CeilVersion;

    INT32 VersStackID = 0;
    for(int i = CeilVersion; i!= NAR_FULLBACKUP_VERSION; i--){
        
        if(i < NAR_FULLBACKUP_VERSION){
            printf("Version ID cant be lower than NAR_FULLBACKUP_VERSION(RootDir: %S, FileName: %S, Volume letter: %c, Selected version %i)\n", RootDir, FileName, VolumeLetter, CeilVersion);
            break;
        }

        nar_fe_search_result SearchResult = NarSearchFileInVolume(RootDir, FileName, VolumeLetter, i);
        if(SearchResult.Found == TRUE){
            Result.FileAbsoluteMFTOffset[Result.VersionsFound++] = SearchResult.AbsoluteMFTRecordOffset;
            Result.StartingVersion = i;
        }

    }

    return Result;

}



/*
    Search's for specific file named FileName in given Handle. returns negative value if fails to do
*/
inline nar_fe_search_result
NarSearchFileInVolume(wchar_t* RootDir, wchar_t *FileName, wchar_t VolumeLetter, INT32 Version){

    nar_fe_search_result Result = {0};    
    nar_backup_file_explorer_context Ctx = {0};

    if(NarInitFileExplorerContext(&Ctx, NAR_FE_HAND_OPT_READ_BACKUP_VOLUME, VolumeLetter, Version, RootDir)){

        wchar_t *Token = 0;        
        wchar_t *TokenStatus = 0;
        Token = wcstok(FileName, L"\\", &TokenStatus);
        
        // skip first token since first one will always be volume name skip it.
        Token = wcstok(0, L"\\", &TokenStatus);

        INT32 FoundFileListID = -1;

        while(Token != NULL){

            BOOLEAN FileFound = FALSE;
            // search for file named Token

            INT32 EntryCount = Ctx.EList.EntryCount;

            // TODO(Batuhan) prefetch entry list
            for(int i =0; i< EntryCount; i++){
                
                // TODO(Batuhan): Im not sure about if files name are in alphabetical order, if so we can early terminate searching
                // but since N < 10000 for most cases, that shouldnt be bottleneck. Most bottleneck will come from cache misses probably
                if(wcscmp(Ctx.EList.Entries[i].Name, Token) == 0){
                    FileFound = TRUE;
                    FoundFileListID = i;
                    break;
                }

            }
        
            if(!FileFound) {
                FoundFileListID = -1;
                break;
            }
        
            NarFileExplorerPushDirectory(&Ctx, FoundFileListID);

            Token = wcstok(0, L"\\", &TokenStatus);
        }


        if(FoundFileListID > 0){
            
            // success
            Result.Found = TRUE;
            UINT64 FileMFTID = Ctx.EList.Entries[FoundFileListID].MFTFileIndex;
            
            // NOTE(Batuhan): Find absolute offset from mft records, then use FindPointOffsetInRecords to convert that abs offset to relative one
            INT64 MFTAbsolutePosition = -1;

            INT32 FileRecordPerCluster = Ctx.ClusterSize / 1024;
            INT64 AdvancedFileCountSoFar = 0;

            for(int i = 0; i<Ctx.MFTRecordsCount; i++){
                
                INT64 FileCountInRegion = (UINT64)FileRecordPerCluster * (UINT64)Ctx.MFTRecords[i].Len;
                if(AdvancedFileCountSoFar + FileCountInRegion > FileMFTID){
                    
                    INT64 RemainingFileCountInRegion = FileMFTID - AdvancedFileCountSoFar;

                    INT64 FileOffsetFromRegion = (INT64)Ctx.MFTRecords[i].StartPos * (INT64)FileRecordPerCluster + (INT64)RemainingFileCountInRegion;
                    FileOffsetFromRegion = FileOffsetFromRegion * 1024;
                    MFTAbsolutePosition = FileOffsetFromRegion;
                    break;

                }
                else{
                    // continue iterating
                }

            }

            if(Ctx.FEHandle.BMEX != NULL){
                
                // 
                // succ found absolute position from mft 

                Result.AbsoluteMFTRecordOffset = MFTAbsolutePosition;
                Result.Found = TRUE;
                Result.FileMFTID = FileMFTID;

            }
            else{
                
                printf("NARFE Handle's context was null, terminating early(File: %S, RootDir: %S, VolumeLetter: %c, Version: %i)\n", FileName, RootDir, VolumeLetter, Version);
                // BMEX shouldnt be null

            }


        }
        else{
            printf("Failed to find file %S in in volume %c in version %i in root dir %S\n", FileName, VolumeLetter, Version, RootDir);
        }


    }
    else{
        printf("Couldnt initialize file explorer context for volume %c for version %i to restore file %S from root directory %S\n", VolumeLetter, Version, FileName, RootDir);
    }


    NarReleaseFileExplorerContext(&Ctx);

    return Result;

}




inline void
RestoreFileFromVersion(const wchar_t *FileName, const wchar_t* TargetName, nar_fe_volume_handle Handle, UINT64 FileMFTID, UINT64 FileAbsoluteMFTOffset){



}



/*
    Be careful about return value, its not a fail-pass thing
    Parses INDEX_ALLOCATION data, not actual MFT file data
*/
inline INT
NarFileExplorerGetFileEntryFromData(void *IndxCluster, nar_file_entry *OutFileEntry){

    memset(OutFileEntry, 0, sizeof(*OutFileEntry));

    UINT16 EntrySize = *(UINT16*)((char*)IndxCluster + 8);
    UINT16 AttrSize = *(UINT16*)((char*)IndxCluster + 10);
    if (EntrySize == 16 || EntrySize == 24 || AttrSize == 0) {
        return NAR_FEXP_END_MARK;
    }

    // check if POSIX entry
    if (*((char*)IndxCluster + NAR_POSIX_OFFSET) == NAR_POSIX) {
        return NAR_FEXP_POSIX;
    }

    UINT8 NameLen = *(UINT8*)((char*)IndxCluster + NAR_NAME_LEN_OFFSET);
    UINT64 MagicNumber = *(UINT64*)IndxCluster;
    MagicNumber = MagicNumber & (0x0000FFFFFFFFFFFF);

    // DO MASK AND TRUNCATE 2 BYTES TO FIND MFTINDEX OF THE FILE

    // useful information here
    UINT64 CreationTime = *(UINT64*)((char*)IndxCluster + NAR_TIME_OFFSET);
    UINT64 ModificationTime = *(UINT64*)((char*)IndxCluster + NAR_TIME_OFFSET + 8);
    UINT64 FileSize = *(UINT64*)((char*)IndxCluster + NAR_SIZE_OFFSET);

    char* NamePtr = ((char*)IndxCluster + NAR_NAME_OFFSET);
    if (*NamePtr == 0) {
        // that shouldnt happen
    }

    memcpy(&OutFileEntry->Name[0], NamePtr, NameLen * sizeof(wchar_t));
    
    // push stuff
    OutFileEntry->MFTFileIndex = MagicNumber;
    OutFileEntry->Size = FileSize;
    OutFileEntry->CreationTime = CreationTime;
    OutFileEntry->LastModifiedTime = ModificationTime;

    return NAR_FEXP_SUCCEEDED;


}


inline void 
NarFileExplorerPushDirectory(nar_backup_file_explorer_context* Ctx, UINT32 SelectedListID) {

    if (!Ctx) {
        printf("Context was null\n");
        return;
    }

    if (SelectedListID > Ctx->EList.EntryCount) {
        printf("ID exceeds size\n");
        return;
    }
    if (Ctx->EList.Entries != 0 && Ctx->EList.Entries[SelectedListID].Size != 0) {
        printf("Not a directory\n");
        return;
    }

    UINT64 NewMFTID = Ctx->EList.Entries[SelectedListID].MFTFileIndex;

    printf("Pushing %I64u to stack\n", NewMFTID);

    Ctx->EList.EntryCount = 0;
    
    NarGetFileListFromMFTID(&Ctx->EList, NewMFTID, Ctx->MFTRecords, Ctx->MFTRecordsCount, 4096, Ctx->FEHandle);
    
    if(Ctx->EList.EntryCount > 0) {
        Ctx->EList.MFTIndex = NewMFTID;
        wcscat(Ctx->CurrentDirectory, Ctx->EList.Entries[SelectedListID].Name);
        NarPushDirectoryStack(Ctx, NewMFTID);
    }

}

inline void
NarFileExplorerPopDirectory(nar_backup_file_explorer_context* Ctx) {

    NarPopDirectoryStack(Ctx);

    Ctx->EList.EntryCount = 0;
    Ctx->EList.MFTIndex = Ctx->HistoryStack.S[Ctx->HistoryStack.I];
    NarGetFileListFromMFTID(&Ctx->EList, Ctx->HistoryStack.S[Ctx->HistoryStack.I], Ctx->MFTRecords, Ctx->MFTRecordsCount, Ctx->ClusterSize, Ctx->FEHandle);

}

inline void
NarFileExplorerPrint(nar_backup_file_explorer_context* Ctx) {
    if (!Ctx) return;
    if (Ctx->EList.Entries == 0) return;
    
    for (int i = 0; i < Ctx->EList.EntryCount; i++) {
        printf("%i\t-> ", i);
        if (Ctx->EList.Entries[i].Size == 0) {
            printf("DIR: %-50S\ %I64u KB\n", Ctx->EList.Entries[i].Name, Ctx->EList.Entries[i].Size / 1024);
        }
        else {
            printf("%-50S %I64u KB\n", Ctx->EList.Entries[i].Name, Ctx->EList.Entries[i].Size / 1024);

        }
    }

}

inline void
NarReleaseFileExplorerContext(nar_backup_file_explorer_context* Ctx) {
    
    if (!Ctx) return; 

    if (Ctx->EList.Entries != 0) {
        free(Ctx->EList.Entries);
    }
    if (Ctx->MFTRecords != 0) {
        free(Ctx->MFTRecords);
    }

    NarFreeFEVolumeHandle(Ctx->FEHandle);



    memset(Ctx, 0, sizeof(*Ctx));

}

inline INT32
NarGetVolumeClusterSize(char Letter){
    
    char V[] = "!:\\";
    V[0] = Letter;

    INT32 Result = 0;
    DWORD SectorsPerCluster = 0;
    DWORD BytesPerSector = 0;
    
    if (GetDiskFreeSpaceA(V, &SectorsPerCluster, &BytesPerSector, 0, 0)){
        Result = SectorsPerCluster * BytesPerSector;
    }
    else{
        printf("Couldnt get disk free space for volume %c\n", Letter);
    }

    return Result;
}

// asserts Buffer is large enough to hold all data needed, since caller has information about metadata this isnt problem at all
inline BOOLEAN
ReadMFTLCNFromMetadata(HANDLE FHandle, backup_metadata Metadata, void *Buffer){
    
    BOOLEAN Result = FALSE;
    if(FHandle != INVALID_HANDLE_VALUE){
        
        // Caller must validate metadata integrity, but it's likely that something invalid may be passed here, check it anyway there isnt a cost
        if(NarSetFilePointer(FHandle, Metadata.Offset.MFTMetadata)){

            DWORD BR = 0;        
            if(ReadFile(FHandle, Buffer, Metadata.Size.MFTMetadata, &BR, 0) && BR == Metadata.Size.MFTMetadata){
                Result = TRUE;
            }
            else{
                // readfile failed
                printf("Readfile failed for ReadMFTLCNFromMetadata, tried to read %i bytes, instead read %i\n", Metadata.Size.MFTMetadata, BR);
                DisplayError(GetLastError());
            }

        }
        else{
            printf("Couldnt set file pointer to %I64d to read mftlcn from metadata\n", Metadata.Offset.MFTMetadata);
            // setfileptr failed
        }
        
    }
    else{
        printf("Metadata handle was invalid, terminating ReadMFTLCNFromMetadata\n");
        // handle was invalid
    }

    return Result;
}


/*
    Ctx = output
    HandleOptions
        NAR_READ_MOUNTED_VOLUME = Reads mounted local disk VolumeLetter and ignores rest of the parameters, and makes FEV->BMEX NULL to. this will lead FEV to become normal volume handle
        NAR_READ_BACKUP_VOLUME 2 = Tries to find backup files in RootDir, if one not given, searches current running directory.
                                    Then according to backup region information, handles read-seak operations in wrapped function
    
*/
inline BOOLEAN
NarInitFileExplorerContext(nar_backup_file_explorer_context* Ctx, INT32 HandleOptions, char Letter, int Version, wchar_t *RootDir) {

    if (!Ctx) goto NAR_FAIL_TERMINATE;;
    
    if(HandleOptions != NAR_FE_HAND_OPT_READ_BACKUP_VOLUME && HandleOptions != NAR_FE_HAND_OPT_READ_MOUNTED_VOLUME){
        printf("Passed invalid HandleOptions to NarInitFileExplorerContext\n");
        goto NAR_FAIL_TERMINATE;
    }


    memset(Ctx, 0, sizeof(*Ctx));

    Ctx->EList.Entries = (nar_file_entry*)malloc(75000 * sizeof(nar_file_entry));
    if (Ctx->EList.Entries) {

        memset(Ctx->EList.Entries, 0, 75000 * sizeof(nar_file_entry));

        memset(Ctx->CurrentDirectory, 0, sizeof(Ctx->CurrentDirectory));
        memset(Ctx->RootDir, 0, sizeof(Ctx->RootDir));
        memset(&Ctx->HistoryStack, 0, sizeof(Ctx->HistoryStack));

        if (NarInitFEVolumeHandle(&Ctx->FEHandle, HandleOptions, Letter, Version, RootDir)) {

            BOOLEAN Status = FALSE;

            if(HandleOptions == NAR_FE_HAND_OPT_READ_MOUNTED_VOLUME){
                Ctx->ClusterSize = NarGetVolumeClusterSize(Letter);
                Ctx->MFTRecords = (nar_record*)NarGetMFTRegionsByCommandLine(Letter, &Ctx->MFTRecordsCount);
                Status = TRUE;
            }
            if(HandleOptions == NAR_FE_HAND_OPT_READ_BACKUP_VOLUME){
                Ctx->ClusterSize = Ctx->FEHandle.BMEX->M.ClusterSize;
                HANDLE BackupMetadataHandle = CreateFile(Ctx->FEHandle.BMEX->FilePath.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
                if(BackupMetadataHandle != INVALID_HANDLE_VALUE){
                    
                    Ctx->MFTRecords = (nar_record*)malloc(Ctx->FEHandle.BMEX->M.Size.MFTMetadata * sizeof(nar_record));
                    if(Ctx->MFTRecords){
                        
                        if(ReadMFTLCNFromMetadata(BackupMetadataHandle, Ctx->FEHandle.BMEX->M, Ctx->MFTRecords)){
                            Ctx->MFTRecordsCount = Ctx->FEHandle.BMEX->M.Size.MFTMetadata / sizeof(nar_record);
                            Status = TRUE;
                        }
                        else{
                            printf("Couldnt read MFTLCN from metadata\n");
                        }

                    }
                    else{
                        printf("Failed to allocate memory for MFTLCN to read from backup metadata\n");
                        // failed to allocate memory
                    }
                    
                    CloseHandle(BackupMetadataHandle);
                }
                else{
                    printf("Failed to open backup metadata for volume %c for version %i, filepath was %s\n", Ctx->FEHandle.BMEX->FilePath.c_str());
                    DisplayError(GetLastError());
                }
                // extract MFT regions from backup metadata
            }

            if (Status) {
                
            }
            else {
                // TODO (Batuhan) :
                // MFT Record setup has been failed, abort
            }

            Ctx->EList.EntryCount = 0;
            Ctx->EList.MFTIndex = NAR_ROOT_MFT_ID;
            Ctx->HistoryStack.I = -1;

            NarGetFileListFromMFTID(&Ctx->EList, NAR_ROOT_MFT_ID, Ctx->MFTRecords, Ctx->MFTRecordsCount, Ctx->ClusterSize, Ctx->FEHandle);
            NarPushDirectoryStack(Ctx, NAR_ROOT_MFT_ID);
            
            wchar_t vb[] = L"%c\\";
            vb[0] = (wchar_t)Letter;
            wcscat(Ctx->CurrentDirectory, vb);
            
#if 0 // performance test
            
                Ctx->EList.EntryCount = 0;
                NarGetFileListFromMFTID(&Ctx->EList, 44571, Ctx->MFTRecords, Ctx->MFTRecordsCount, Ctx->ClusterSize, Ctx->FEHandle);

                Ctx->EList.EntryCount = 0;
                NarGetFileListFromMFTID(&Ctx->EList, 120848, Ctx->MFTRecords, Ctx->MFTRecordsCount, Ctx->ClusterSize, Ctx->FEHandle);


                Ctx->EList.EntryCount = 0;
                NarGetFileListFromMFTID(&Ctx->EList, 98328, Ctx->MFTRecords, Ctx->MFTRecordsCount, Ctx->ClusterSize, Ctx->FEHandle);

                Ctx->EList.EntryCount = 0;
                NarGetFileListFromMFTID(&Ctx->EList, 43085, Ctx->MFTRecords, Ctx->MFTRecordsCount, Ctx->ClusterSize, Ctx->FEHandle);

                Ctx->EList.EntryCount = 0;
                NarGetFileListFromMFTID(&Ctx->EList, 204837, Ctx->MFTRecords, Ctx->MFTRecordsCount, Ctx->ClusterSize, Ctx->FEHandle);

                Ctx->EList.EntryCount = 0;
                NarGetFileListFromMFTID(&Ctx->EList, 221313, Ctx->MFTRecords, Ctx->MFTRecordsCount, Ctx->ClusterSize, Ctx->FEHandle);

                Ctx->EList.EntryCount = 0;
                NarGetFileListFromMFTID(&Ctx->EList, 228462, Ctx->MFTRecords, Ctx->MFTRecordsCount, Ctx->ClusterSize, Ctx->FEHandle);

                Ctx->EList.EntryCount = 0;
                NarGetFileListFromMFTID(&Ctx->EList, 229948, Ctx->MFTRecords, Ctx->MFTRecordsCount, Ctx->ClusterSize, Ctx->FEHandle);

#endif

        }
        else {
            goto NAR_FAIL_TERMINATE;
        }
    }
    else {
        goto NAR_FAIL_TERMINATE;
    }
    
    return TRUE;

    NAR_FAIL_TERMINATE:
    if(Ctx){
        NarReleaseFileExplorerContext(Ctx);
    }

}


/*
    Args: FEV = file explorer handle that has initialized via NarInitFEVolumeHandle
        NewFilePointer = Target file pointer to be set in volume, not in actual binary file. So if handle has been initialized as READ_BACKUP_DATA,
        function figures out how to set a file pointer in a way that if caller requests readfile, it acts like if it were actually reading from that offset.
        what caller has to be careful is, there is no guarentee that reading N bytes from that position will give caller sequential data. It is guarenteed there
        are at least one cluster sized data resides at that file offset, but due to nature of the backup system, next clusters may not exist in that particular 
        file.
*/
inline BOOLEAN
NarFileExplorerSetFilePointer(nar_fe_volume_handle FEV, UINT64 NewFilePointer){

    BOOLEAN Result = FALSE;

    if(FEV.VolumeHandle != INVALID_HANDLE_VALUE){
        if(FEV.BMEX == NULL){
            // normal file handle, safe to call NarSetFilePointer
            Result = NarSetFilePointer(FEV.VolumeHandle, NewFilePointer);
        }
        else{

            UINT64 NewFilePtrInCluster = NewFilePointer / FEV.BMEX->M.ClusterSize;
            UINT64 ClusterOffset =  NewFilePointer % FEV.BMEX->M.ClusterSize;
            printf("Converted new file pointer to cluster space, new cluster %I64u, addition in bytes %I64u",NewFilePtrInCluster, ClusterOffset);

            // backup file handle, must calculate relative offset.
            UINT64 RelativePointerInCluster = FindPointOffsetInRecords(FEV.BMEX->RegionsMetadata.Data, FEV.BMEX->RegionsMetadata.Count, NewFilePtrInCluster);
            if(RelativePointerInCluster != NAR_POINT_OFFSET_FAILED){
                
                printf("Converted absolute file ptr %I64u to relative file cluster pointer %I64d, with addition of %I64u bytes\n", NewFilePointer, RelativePointerInCluster, ClusterOffset);
                UINT64 RelativeFilePointerTarget = RelativePointerInCluster * FEV.BMEX->M.ClusterSize + ClusterOffset;

                Result = NarSetFilePointer(FEV.VolumeHandle, RelativeFilePointerTarget);                
                if(!Result){
                    printf("Couldnt set relative file pointer %I64d\n", RelativeFilePointerTarget);
                }
                else{
                    Result = TRUE;
                }

            }
            else{
                printf("Failed to move absolute file ptr(%I64u)\n", NewFilePointer);
                // failed
            }

        }
    }
    else{

    }

    return Result;
}

inline BOOLEAN
NarFileExplorerReadVolume(nar_fe_volume_handle FEV, void* Buffer, DWORD ReadSize, DWORD* OutBytesRead){
    
    // so, since we are handling INDEX_ALLOCATION regions, all file requests should be valid, but i worry about that
    // mft may throw up some special regions for weird directories
    // for now, ill accept as everything will be fine, if any error occurs in file explorer, i might need to implement some safe procedure
    // to be sure that read length doesnt exceed current region

    if(FEV.BMEX == NULL){
        // normal file handle
    }
    else{

    }

    return ReadFile(FEV.VolumeHandle, Buffer, ReadSize, OutBytesRead, 0);
}


/*
    args:
    FEV = output, simple
    HandleOptions
        NAR_READ_MOUNTED_VOLUME = Reads mounted local disk VolumeLetter and ignores rest of the parameters, and makes FEV->BMEX NULL to. this will lead FEV to become normal volume handle
        NAR_READ_BACKUP_VOLUME 2 = Tries to find backup files in RootDir, if one not given, searches current running directory.
                                    Then according to backup region information, handles read-seak operations in wrapped function

*/
inline BOOLEAN 
NarInitFEVolumeHandle(nar_fe_volume_handle *FEV, INT32 HandleOptions, char VolumeLetter, INT32 Version, wchar_t *RootDir) {

    if (!FEV) return FALSE;

    BOOLEAN Result = FALSE;

    if(HandleOptions != NAR_FE_HAND_OPT_READ_MOUNTED_VOLUME && HandleOptions != NAR_FE_HAND_OPT_READ_BACKUP_VOLUME){
        printf("Invalid HandleOptions argument passed to NarInitFEVolumeHandle\n");
        return FALSE;
    }
    
    if(HandleOptions == NAR_FE_HAND_OPT_READ_BACKUP_VOLUME){
        
        wchar_t Path[1024];

        memset(Path, 0, sizeof(Path));

        if(RootDir){
            wcscat(Path, RootDir);
            wcscat(Path, L"\\");
        }

        std::wstring BinaryFileName = GenerateBinaryFileName((wchar_t)VolumeLetter, Version);

        wcscat(Path, BinaryFileName.c_str());
        printf("Target file path to initialize file explorer volume handle : %s\n", Path);

        FEV->VolumeHandle = CreateFileW(Path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);

        if(FEV->VolumeHandle != INVALID_HANDLE_VALUE){

            FEV->BMEX = InitBackupMetadataEx(VolumeLetter, Version, RootDir);
            if(FEV->BMEX != NULL){
                Result = TRUE;
            }
            else{
                printf("Couldnt initialize backup metadata for volume %c for version %i from path %s\n", VolumeLetter, Version, RootDir);
            }

        }  
        else{
            printf("Couldnt open file %s for FE Volume handle\n", Path);
            DisplayError(GetLastError());
        }

    }
    if(HandleOptions == NAR_FE_HAND_OPT_READ_MOUNTED_VOLUME){

        FEV->BMEX = 0;
        FEV->VolumeHandle = NarOpenVolume(VolumeLetter);
        if(FEV->VolumeHandle != INVALID_HANDLE_VALUE){
            Result = TRUE;
        }
        else{
            printf("Failed to open volume for file explorer for volume %c\n", VolumeLetter);
        }
    }


    // failed
    if(!Result){
        if(FEV){
            CloseHandle(FEV->VolumeHandle);
            if(FEV->BMEX){
                FreeBackupMetadataEx(FEV->BMEX);
                FEV->BMEX = 0;
                FEV->VolumeHandle = INVALID_HANDLE_VALUE;
            }
        }
    }

    return Result;
}

inline void 
NarFreeFEVolumeHandle(nar_fe_volume_handle FEV) {
    
    {
        CloseHandle(FEV.VolumeHandle);
        FEV.VolumeHandle = INVALID_HANDLE_VALUE;

        if(FEV.BMEX){
            FreeBackupMetadataEx(FEV.BMEX);
            FEV.BMEX = 0;
        }
    }

}



// input MUST be sorted
// Finds point Offset in relative to Records structure, useful when converting absolue volume offsets to our binary backup data offsets.
// returns NAR_POINT_OFFSET_FAILED if fails to find given offset, 
inline INT64 
FindPointOffsetInRecords(nar_record *Records, INT32 Len, INT64 Offset){

    if(!Records) return NAR_POINT_OFFSET_FAILED;

    INT64 Result = 0;
    BOOLEAN Found = FALSE;
    
    for(int i = 0; i < Len; i++){
        
        if(Offset <= (INT64)Records[i].StartPos + (INT64)Records[i].Len){

            INT64 Diff = (Offset - (INT64)Records[i].StartPos);
            if (Diff < 0) {
                // Exceeded offset, this means we cant relate our Offset and Records data, return failcode
                Found = FALSE;
            }
            else {
                Result += Diff;
                Found = TRUE;
            }

            break;

        }
        

        Result += Records[i].Len;

    }


    return (Found ? Result : NAR_POINT_OFFSET_FAILED);
}


inline BOOLEAN
NarIsDirectoryExists(const wchar_t* Dir){
    // TODO(Batuhan)
    return TRUE;
}




void
TestFindPointOffsetInRecords(){


    nar_record* Recs = new nar_record[100];
    memset(Recs, 0, sizeof(nar_record) * 100);

    Recs[0] = {0, 100};
    Recs[1] = {150, 200};
    Recs[2] = {520, 150};
    Recs[3] = {1200, 55};
    Recs[4] = {1300, 100};
    Recs[5] = {1500, 75};
    Recs[6] = {1700, 420};
    Recs[7] = {5200, 500};

    INT64 Answer = FindPointOffsetInRecords(Recs, 8, 1350);
    printf("HELL ==== %I64d\n", Answer);

    Answer = FindPointOffsetInRecords(Recs, 8, 1000);
    printf("HELL ==== %I64d\n", Answer);

    Answer = FindPointOffsetInRecords(Recs, 8, 70);
    printf("HELL ==== %I64d\n", Answer);

    Answer = FindPointOffsetInRecords(Recs, 8, 5400);
    printf("HELL ==== %I64d\n", Answer);

    Answer = FindPointOffsetInRecords(Recs, 8, 2000);
    printf("HELL ==== %I64d\n", Answer);

    Answer = FindPointOffsetInRecords(Recs, 8, 3000);
    printf("HELL ==== %I64d\n", Answer);


    return;

}


struct{
    UINT64 LastCycleCount;
    UINT64 WorkCycleCount;
    LARGE_INTEGER LastCounter;
    LARGE_INTEGER WorkCounter;
    INT64 GlobalPerfCountFrequency;

}NarPerfCounter;


inline float
GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End){
    float Result = (float)(End.QuadPart - Start.QuadPart)/(float)(NarPerfCounter.GlobalPerfCountFrequency);
    return Result;
}

inline LARGE_INTEGER
GetClock(){
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return Result;
}

inline void
NarInitPerfCounter(){
    NarPerfCounter.LastCycleCount = 0;
    NarPerfCounter.WorkCycleCount = 0;
    NarPerfCounter.LastCounter = {0};
    NarPerfCounter.WorkCounter = {0};
    
    LARGE_INTEGER PerfCountFreqResult;
    QueryPerformanceFrequency(&PerfCountFreqResult);
    NarPerfCounter.GlobalPerfCountFrequency = PerfCountFreqResult.QuadPart;

}

inline void
NarStartPerfCounter(){
    NarPerfCounter.LastCycleCount = __rdtsc();
    NarPerfCounter.LastCounter = GetClock();
}

inline void
NarEndPerfCounter(){
    NarPerfCounter.WorkCycleCount = __rdtsc();
    NarPerfCounter.WorkCounter = GetClock();
}

inline BOOLEAN
Test_NarGetRegionIntersection() {

    BOOLEAN Result = FALSE;

    nar_record r1[16], r2[16], r3[16];

    r1[0] = { 0, 200 };
    r1[1] = { 300, 200 };
    r1[2] = { 800, 100 };
    r1[3] = { 1200, 800 };
    r1[4] = { 2100, 200 };

    r2[0] = { 80, 70 };
    r2[1] = { 250, 100 };
    r2[2] = { 700, 50 };
    r2[3] = { 1100, 300 };
    r2[4] = { 1500, 50 };
    r2[5] = { 1600, 800 };

    INT32 found = 0;
    NarGetRegionIntersection(r1, r2, r3, 5, 6, 16, &found);

    return Result;
}


int
main(
     int argc,
     CHAR* argv[]
     ) {
    
    return Test_NarGetRegionIntersection();
    
    //TestFindPointOffsetInRecords();
    //return 0;
    
    //HANDLE H = CreateFileA("tempfile.txt", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_ALWAYS, 0, 0);
    //
    //UINT64 Bs = Megabyte(256);
    //char *buffer = (char*)malloc(Bs);
    //
    //DWORD Hell = 0;
    //for (int i = 0; i < 20; i++) {
    //    
    //    if (WriteFile(H, buffer, Bs, &Hell, 0) && Hell == Bs) {
    //    
    //    }
    //    else {
    //        printf("failed %i\n");
    //    }
    //
    //}
    //return 0;

    NarInitPerfCounter();

    nar_backup_file_explorer_context ctx;
    NarStartPerfCounter();

    printf("Please take a snapshot\n");
    getchar();

    NarInitFileExplorerContext(&ctx, NAR_FE_HAND_OPT_READ_MOUNTED_VOLUME, 'C', 0, NULL);

    NarEndPerfCounter();

    float time = GetSecondsElapsed(NarPerfCounter.LastCounter, NarPerfCounter.WorkCounter);
    
    INT64 CyclesElapsed = NarPerfCounter.LastCycleCount - NarPerfCounter.WorkCycleCount;
    
    double WorkCyclesElapsed = (double)(CyclesElapsed) / (1000.f * 1000.f);
    double fps = (double)1.0f / (double)time;

    printf("%d, %f, %.02f", WorkCyclesElapsed, time, fps);
    
    NarFileExplorerPrint(&ctx);
    
    int ListID = 0;
    while (1) {
        scanf("%i", &ListID);
        
        if (ListID == -42) {
            break;
        }

        if (ListID < 0) {
            NarFileExplorerPopDirectory(&ctx);
        }
        else {
            NarFileExplorerPushDirectory(&ctx, ListID);
        }

        NarFileExplorerPrint(&ctx);

        Sleep(16);
    }

    NarReleaseFileExplorerContext(&ctx);
    

    NarInitFileExplorerContext(&ctx, NAR_FE_HAND_OPT_READ_MOUNTED_VOLUME, 'C', 0, NULL);
    NarFileExplorerPrint(&ctx);
    NarReleaseFileExplorerContext(&ctx);


    printf("Freed smt\n");
    
    int bloke_optimization;
    scanf("%i", &bloke_optimization);

    printf("optimization blocked %i\n", bloke_optimization);

    return 0;

    wchar_t B[50];
    if(NarGetVolumeGUIDKernelCompatible(L'C', &B[0])){
        printf("%s\n", B);
    }


    return 0;
    

    HRESULT hResult = 0;
    hResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (!SUCCEEDED(hResult)) {
        printf("Failed CoInitialize function %d\n", hResult);
        DisplayError(GetLastError());
        return FALSE;
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
        return FALSE;
    }

    // force bluescreen

    WCHAR Temp[] = L"!:\\";
    Temp[0] = L'C';
    
    HRESULT Result = FilterAttach(MINISPY_NAME, Temp, 0, 0, 0);
    
    if(Result == S_OK) {    
        printf("Successfully attaced volume %c to driver\n");
    }
    else{
        printf("Couldnt attach volume %c to driver, error code %X\n", Temp[0], Result);
    }
    
    CComPtr<IVssBackupComponents> IVSSPTR;

    wchar_t* A = GetShadowPath(Temp, IVSSPTR);

    printf("%S\n", A);

    while (1) {
        Sleep(250);
    }

    
    return 0;

    printf("r to restore\n");
    if(getchar() == 'r') {
        foo();
        return 0;
    }
    
    
   


    wchar_t OutputBinaryFileName[] = L"BinaryFile";
    CComPtr<IVssBackupComponents> VSSPTR;
    
    wchar_t *ShadowPath = GetShadowPath(L"C:\\", VSSPTR);

    HANDLE VolumeHandle = CreateFile(ShadowPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if(VolumeHandle != INVALID_HANDLE_VALUE){

        UINT32 RecordCount = 0;
        nar_record* VolumeRecords = GetVolumeRegionsFromBitmap(VolumeHandle, &RecordCount);
        
        HANDLE OutputFile = CreateFile(OutputBinaryFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_ALWAYS, 0, 0);

        BOOLEAN Result = 0;
        ULONGLONG TargetFilePointer = 0;
        for(int i = 0; i<RecordCount; i++){
            
            TargetFilePointer = (ULONGLONG)VolumeRecords[i].StartPos*(ULONGLONG)4096;

            Result = NarSetFilePointer(VolumeHandle, TargetFilePointer);
            if(Result){

                if(!CopyData(VolumeHandle, OutputFile, (ULONGLONG)VolumeRecords[i].Len*(ULONGLONG)4096)){
                    printf("Couldnt copy %I64d bytes from starting pos of %I64d\n", (ULONGLONG)VolumeRecords[i].Len*(ULONGLONG)4096, TargetFilePointer);
                }
                printf("Copied %i regions out of %i\r", i, RecordCount);

            }
            else{
                printf("Couldnt set file pointer to %I64d, region len was %I64d\n",TargetFilePointer);
            }


        }

        NarDumpToFile("records", VolumeRecords, RecordCount*sizeof(nar_record));


        printf("\n");
        printf("DONE BACKING UP VOLUME C\n");
        
        CloseHandle(VolumeHandle);
        VSSPTR.Release();
        
        while(1){
            
            char input = getchar();
            input = tolower(input);
            if (input != 'r' && input != 'q') {
                printf("Press R to restore volume to volume E, press q to exit\n");
            }

            if (input == 'q') {
                break;
            }

            if (input == 'r') {
                

                NarCreateCleanGPTBootablePartition(1, 61440, 100, 529, 'E');

                if (NarSetFilePointer(OutputFile, 0)) {
                    
                    VolumeHandle = NarOpenVolume('E');
                    if (VolumeHandle != INVALID_HANDLE_VALUE) {
                        
                        
                        for (int i = 0; i < RecordCount; i++) {
                            TargetFilePointer = (ULONGLONG)VolumeRecords[i].StartPos * (ULONGLONG)4096;
                            if (NarSetFilePointer(VolumeHandle, TargetFilePointer)) {
                                
                                ULONGLONG CopyLen = (ULONGLONG)VolumeRecords[i].Len * (ULONGLONG)4096;
                                if (!CopyData(OutputFile, VolumeHandle, CopyLen)) {
                                    printf("Couldnt copy %I64d bytes from starting pos of %I64d\n", (ULONGLONG)VolumeRecords[i].Len * (ULONGLONG)4096, TargetFilePointer);
                                }

                            }

                        }

                        printf("DONE RESTORE OPERATION\n");
                        
                        break;

                    }
                    else {
                    
                    }

                }
                else {
                    printf("Couldnt set file pointer to beginning of the file\n");
                }

            }

            Sleep(250);
        }


    }
    else{
        printf("Couldnt open volume C's shadow path\n");
    }


    printf("DONE WHOLE OPERATION\n");

    return 0;

    data_array<nar_record> R = GetMFTLCN('C', NarOpenVolume('C'));
    
    for(int i= 0; i<R.Count;i++){
        printf("%i\t%i\n", R.Data[i].StartPos, R.Data[i].Len);
    }


    return 0;

    
    // tests
#if 0
    char s1[] = "denemestringi";
    char s2[] = "ingi";
    char s3[] = "denem";
    char s4[] = "nonexistentstringhere";
    char s5[] = "estri";
    char s6[] = "ingix";
    char s7[] = "edenem";


    if(StrFind(s1,s2,strlen(s1),strlen(s2))){
        printf("Found substring s2 in s1\n");
    }

    if(StrFind(s1,s3,strlen(s1),strlen(s3))){
        printf("Found substring s3 in s1\n");
    }
    
    if(StrFind(s1,s4,strlen(s1),strlen(s4))){
        printf("Found substring s4 in s1\n");
    }
    
    if(StrFind(s1,s5,strlen(s1),strlen(s5))){
        printf("Found substring s5 in s1\n");
    }

    if (StrFind(s1, s6, strlen(s1), strlen(s6))) {
        printf("Found substring s6 in s1\n");
    }

    if (StrFind(s1, s7, strlen(s1), strlen(s7))) {
        printf("Found substring s7 in s1\n");
    }
#endif

    data_array<disk_information> Disks = NarGetDisks();

    for(unsigned int i = 0; i<Disks.Count; i++){
        printf("ID:   %i\n", Disks.Data[i].ID);
        printf("Type: %c\n", Disks.Data[i].Type);
        printf("Size: %I64u\n", Disks.Data[i].Size);
        printf("###############\n");
    }



    return 0;    
   
    
    
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
    
    hResult = FALSE;
    
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
