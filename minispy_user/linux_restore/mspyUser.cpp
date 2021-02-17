/*
  I do unity builds, so I dont check if declared functions in .h file, I just write it in cpp file and if any build gives me error then
  I add it to .h file.
*/
/*
*  // TODO(Batuhan): URGENT, ZEROING OUT MFT REGIONS THAT EXCEEDS 4GB IS IMPOSSIBLE NOW, GO TO  RestoreVersionWithoutLoop and fix it 
 *  // TODO(Batuhan): We are double sorting regions, just to append indx and lcn regions. Rather than doing that, remove sort from setupdiffrecords & setupincrecords and do it in the setupstream function, this way we would just append then sort + merge once, not twice(which may be too expensive for regions files > 100M)
*/

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#if 0
#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif
#endif


#include <stdlib.h>
#include <stdio.h>
#include <cstdio>
#include <assert.h>
#include <functional>


#include <vector>
#include <string>
#include <string.h>

#include <fstream>
#include <streambuf>
#include <sstream>

#include "mspyLog.h"

#if 0
inline void* 
narmalloc(size_t len){
    TIMED_BLOCK();
    return malloc(len);
}
inline void
narfree(void* m){
    TIMED_BLOCK();
    free(m);
}
#define malloc narmalloc
#define free narfree

#endif



/*
    ASSUMES RECORDS ARE SORTED
THIS FUNCTION REALLOCATES MEMORY VIA realloc(), DO NOT PUT MEMORY OTHER THAN ALLOCATED BY MALLOC, OTHERWISE IT WILL CRASH THE PROGRAM
*/
inline void
MergeRegions(data_array<nar_record>* R) {
    
    UINT32 MergedRecordsIndex = 0;
    UINT32 CurrentIter = 0;
    
    for (;;) {
        if (CurrentIter >= R->Count) {
            break;
        }
        
        UINT32 EndPointTemp = R->Data[CurrentIter].StartPos + R->Data[CurrentIter].Len;
        
        if (IsRegionsCollide(R->Data[MergedRecordsIndex], R->Data[CurrentIter])) {
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
CopyData(FILE *S, FILE *D, unsigned long long Len) {
    
    BOOLEAN Return = TRUE;
    
    UINT32 BufSize = 8*1024*1024; //8 MB
    ULONGLONG TotalCopied = 0;
    
    void* Buffer = malloc(BufSize);
    
    if (Buffer != NULL) {
        ULONGLONG BytesRemaining = Len;
        DWORD BytesOperated = 0;
        if (BytesRemaining > BufSize) {
            
            /*
            Following is the declaration for fread() function.

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)

Parameters

    ptr − This is the pointer to a block of memory with a minimum size of size*nmemb bytes.

    size − This is the size in bytes of each element to be read.

    nmemb − This is the number of elements, each one with a size of size bytes.

    stream − This is the pointer to a FILE object that specifies an input stream.

            */
            while (BytesRemaining > BufSize) {
                if (fread(Buffer, BufSize, 1, S) == 1) {
                    //size_t fwrite ( const void * ptr, size_t size, size_t count, FILE * stream );
                    bool bResult = (1 == fwrite(Buffer, BufSize, 1, D));
                    if (!bResult) {
                        printf("COPY_DATA: fwrite failed with code %X\n", bResult);
                        printf("Tried to write -> %I64lld, Bytes written -> %ld\n", Len, BytesOperated);
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
            if (1==fread(Buffer,BytesRemaining, 1, S)) {
            
                if (1!=fwrite(Buffer, BytesRemaining, 1, D)) {
                    printf("COPY_DATA: Error occured while copying end of buffer\n");
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
        printf("COPYFILE error detected, copied %I64lld bytes instead of %I64llu\n", TotalCopied, Len);
    }
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

inline BOOLEAN
IsRegionsCollide(nar_record R1, nar_record R2) {
    
    BOOLEAN Result = FALSE;
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


/*
       Caller MUST CALL NarFreeMFTRegionsByCommandLine to free memory allocated by this function, otherwise, it will be leaked
*/
nar_record*
NarGetMFTRegionsByCommandLine(char Letter, unsigned int* OutRecordCount){
    
    char CmdBuffer[512];
    char OutputName[] = "naroutput.txt";
    
    sprintf(CmdBuffer, "fsutil file queryextents %c:\\$MFT > %s", Letter, OutputName);
    system(CmdBuffer);
    
    file_read FileContents = NarReadFile(OutputName);
    
    char* str = (char*)FileContents.Data;
    
    if (str == NULL || OutRecordCount == NULL) return 0;
    
    
    size_t ResultSize = 128 * sizeof(nar_record);
    nar_record* Result = (nar_record*)malloc(ResultSize);
    memset(Result, 0, ResultSize);
    
    unsigned int RecordFound = 0;
    
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
            int StrLen = (int)(next - prev); // acceptable conversion
            
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

inline void 
NarFreeMFTRegionsByCommandLine(nar_record *records){
    free(records);
}


BOOLEAN
OfflineRestore(restore_inf* R) {
    
    // TODO(Batuhan): CHECK IF TARGET VOLUME EXISTS, IF NOT CREATE ONE 
    BOOLEAN Result = FALSE;
    
    int Version = -10;
    ULONGLONG VolumeTotalSize = 0;
    BackupType BT = BackupType::Diff;
    
    {
        backup_metadata BM = ReadMetadata(R->BackupID, R->Version, R->RootDir);
        if (BM.Errors.RegionsMetadata == 0) {
            Version = BM.Version;
            BT = BM.BT;
            VolumeTotalSize= BM.VolumeTotalSize;
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
    
    // TODO(Batuhan): This is dumb check, fix that urgently!
    if (VolumeTotalSize != 0) {
        
#if 0
        if (ShouldFormat) {
            
            if (NarFormatVolume((char)R->TargetLetter)) {
                if (NarSetVolumeSize((char)R->TargetLetter, (int)(VolumeTotalSize / (1024ull * 1024ull)))) {
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
#endif        
        
        std::string volname = "/dev/";
        volname += R->TargetPartition;
        FILE* Volume = fopen(volname.c_str(), "rb");
        if (NULL != Volume) {
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
        
        fclose(Volume);
        
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
        printf("%s\n", Buffer);
        system(Buffer);
        return TRUE;
    }
    
    printf("COULDNT DUMP CONTENTS TO FILE, RETURNING FALSE CODE (NARREMOVELETTER)\n");
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
    FILE *File = fopen(FileName, "rb");
    if (File != 0) {
        
        if(0 == fseek(File, 0, SEEK_END)){
        	Result.Len = ftell(File);
			if(0 != fseek(File, 0, SEEK_SET)){
				printf("Unable to set file pointer to beginning of the file\n");
				Result.Len = 0;
			}
		}
			
		
        if (Result.Len == 0){
        	printf("Length was zero, early terminating readfile for file %s\n", FileName);
        	return Result;
        }
		
        Result.Data = malloc((size_t)Result.Len);
        if (Result.Data) {
        	int fresult = fread(Result.Data, Result.Len, 1, File);
            if (1 == fresult) {
                // NOTE success
            }
            else {
            	printf("Unable to read file %s, err code %d\n", FileName, ferror(File));
                free(Result.Data);
				Result.Data = 0;
				Result.Len = 0;
            }
        }
        fclose(File);

    }
    else {
        printf("Can't create file: %s\n", FileName);
    }
    return Result;
}


file_read
NarReadFile(const wchar_t* FileName) {
    file_read Result = { 0 };
#if _MSC_VER
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
#else
	return NarReadFile(wstr2str(FileName).c_str());
#endif
    return Result;
}

inline void
FreeFileRead(file_read FR) {
    free(FR.Data);
}


inline BOOLEAN
NarDumpToFile(const char* FileName, void* Data, unsigned int Size) {
    BOOLEAN Result = FALSE;
#if _MSC_VER
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
#else
	FILE *F = fopen(FileName, "rb");
	if(NULL != F){
		if(1 == fwrite(Data, Size, 1, F)){
			Result = TRUE;			
		}
		else{
			printf("Couldnt write data to file %s\n", FileName);
		}
		fclose(F);
	}
	else{
		printf("Couldnt open file %s\n", FileName);
	}
#endif
    return Result;
}


inline BOOLEAN
NarCreateCleanGPTBootablePartition(int DiskID, int VolumeSizeMB, int EFISizeMB, int RecoverySizeMB, char Letter) {

#if _MSC_VER
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
    // NOTE(Batuhan): safe conversion
    if (NarDumpToFile(InputFN, Buffer, (INT32)strlen(Buffer))) {
        sprintf(Buffer, "diskpart /s %s", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }

#endif
	return FALSE;
}

inline BOOLEAN
NarCreateCleanGPTPartition(int DiskID, int VolumeSizeMB, char Letter) {
#if _MSC_VER

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
    if (NarDumpToFile(InputFN, Buffer, (unsigned int)strlen(Buffer))) {
        sprintf(Buffer, "diskpart /s %s > .txt", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    #endif
    return FALSE;
}


BOOLEAN
NarCreateCleanMBRPartition(int DiskID, char VolumeLetter, int VolumeSize) {

#if _MSC_VER
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
    if (NarDumpToFile(InputFN, Buffer, (unsigned int)strlen(Buffer))) {
        sprintf(Buffer, "diskpart /s %s", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    
    
    printf("NarCreateCleanMBRPartition IS NOT IMPLEMENTED\n");
#endif
    return FALSE;
    
}


inline BOOLEAN
NarCreateCleanMBRBootPartition(int DiskID, char VolumeLetter, int VolumeSizeMB, int SystemPartitionSizeMB, int RecoveryPartitionSizeMB) {
    
#if _MSC_VER
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
    if (NarDumpToFile(InputFN, Buffer, (unsigned int)strlen(Buffer))) {
        sprintf(Buffer, "diskpart /s %s", InputFN);
        printf(Buffer);
        system(Buffer);
        return TRUE;
    }
    
#endif
    
    return FALSE;
    
}

inline BOOLEAN
NarIsOSVolume(char Letter) {
#if _MSC_VER 
    char windir[256];
    GetWindowsDirectoryA(windir, 256);
    return windir[0] == Letter;
#endif
	return FALSE;
}


/*
Returns:
'R'aw
'M'BR
'G'PT
*/
inline int
NarGetVolumeDiskType(char Letter) {
#if _MSC_VER
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
            }
            
        }
        
        
    }
    else {
        printf("Cant open drive %c as file\n", Letter);
    }
    
    VirtualFree(Buf, 0, MEM_RELEASE);
    CloseHandle(Drive);
    CloseHandle(Disk);
    return Result;
#endif
	return 0;
	
}


inline unsigned char
NarGetVolumeDiskID(char Letter) {
#if _MSC_VER
    wchar_t VolPath[512];
    wchar_t Vol[] = L"!:\\";
    Vol[0] = Letter;
    
    unsigned char Result = (unsigned char)NAR_INVALID_DISK_ID;
    DWORD BS = 1024 * 2; //1 KB
    DWORD T = 0;
    
    VOLUME_DISK_EXTENTS* Ext = (VOLUME_DISK_EXTENTS*)VirtualAlloc(0, BS, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    
    if(NarGetVolumeGUIDKernelCompatible((wchar_t)Letter,VolPath)){
        
        HANDLE Drive = CreateFileW(VolPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        
        if (Drive != INVALID_HANDLE_VALUE) {
            
            if (DeviceIoControl(Drive, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0, Ext, BS, &T, 0)) {
                //wchar_t DiskPath[512];
                // L"\\\\?\\PhysicalDrive%i";
                Result = (unsigned char)Ext->Extents[0].DiskNumber;
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
#endif
	return 0;
}

inline BOOLEAN
NarIsFullBackup(int Version) {
    return Version < 0;
}

inline std::wstring
NarBackupIDToWStr(nar_backup_id ID){
    wchar_t B[64];
    memset(B, 0, sizeof(B));
    swprintf(B, 64, L"%I64llu",ID.Q);
    
    std::wstring Result = std::wstring(B);
    return Result;
}

inline std::wstring
GenerateMetadataName(nar_backup_id ID, int Version) {
    
    std::wstring Result = MetadataFileNameDraft;
    
    if (Version == NAR_FULLBACKUP_VERSION) {
        Result += L"FULL";
    }
    else {
        Result += std::to_wstring(Version);
    }
    
    wchar_t t[8];
    swprintf(t, 8, L"%c", ID.Letter);
    
    Result += L"-";
    Result += std::wstring(t);
    Result += NarBackupIDToWStr(ID);
    Result += MetadataExtension;
    
    return Result;
}


inline std::wstring
GenerateBinaryFileName(nar_backup_id ID, int Version) {
    
    std::wstring Result = BackupFileNameDraft;
    
    if (Version == NAR_FULLBACKUP_VERSION) {
        Result += L"FULL";
    }
    else {
        Result += std::to_wstring(Version);
    }
    
    
    wchar_t t[8];
    swprintf(t, 8, L"%c", ID.Letter);
    
    Result += L"-";
    Result += std::wstring(t);
    Result += NarBackupIDToWStr(ID);
    
    Result += BackupExtension;
    //printf("Generated binary file name %S\n", Result.c_str());
    return Result;
    
}


/*
Unlike generatemetadata, binary functions, this one generates absolute path of the log file. Which is 
under windows folder
C:\Windows\Log....
*/
inline std::wstring
GenerateLogFilePath(char Letter) {
    wchar_t NameTemp[]= L"NAR_LOG_FILE__.nlfx";
    NameTemp[13] = (wchar_t)Letter;
    
    std::wstring Result = L"C:\\Windows\\";
    Result += std::wstring(NameTemp);
    
    return Result;
}

// Volume optional, might pass INVALID_HANDLE_VALUE
BOOLEAN
RestoreVersionWithoutLoop(restore_inf R, BOOLEAN RestoreMFT, FILE* Volume) {
    
    BOOLEAN IsVolumeLocal = (NULL == Volume); // Determine if we should close volume handle at end of function
    
    if (R.RootDir.length() > 0) {
        if (R.RootDir[R.RootDir.length() - 1] != L'\\') {
            R.RootDir += L"\\";
        }
    }
    
    if (IsVolumeLocal) {
        printf("Volume handle was invalid for backup ID %i, creating new handle\n", R.Version);
        std::string volname = "/dev/";
		volname += R.TargetPartition;
        Volume = fopen(volname.c_str(), "rb");
    }
    
    backup_metadata_ex* BMEX = InitBackupMetadataEx(R.BackupID, R.Version, R.RootDir);
    if (BMEX == NULL) {
        printf("Couldn't init backupmetadata\n");
        return FALSE;
    }
    printf("Initialized backup metadata for version %i\n", BMEX->M.Version);
    
    int CopyErrorsOccured = 0;
    
    std::wstring BinaryFileName = R.RootDir;
    BinaryFileName += GenerateBinaryFileName(R.BackupID, R.Version);
    
    std::string cBinaryFileName = wstr2str(BinaryFileName);
    FILE* RegionsFile = fopen(cBinaryFileName.c_str(), "rb");
    
    if (Volume != NULL && RegionsFile != NULL) {
        
        for (UINT32 i = 0; i < BMEX->RegionsMetadata.Count; i++) {
            
            if (0==fseek(Volume, (ULONGLONG)BMEX->RegionsMetadata.Data[i].StartPos * (ULONGLONG)BMEX->M.ClusterSize, SEEK_SET)) {
                
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
        if (Volume == 0) {
            printf("Volume handle is invalid: %s\n", R.TargetPartition.c_str());
        }
        if (RegionsFile == 0) {
            printf("Couldn't open regions binary file: %S\n", BinaryFileName.c_str());
        }
        CopyErrorsOccured++;
    }
    
    if (CopyErrorsOccured) {
        printf("%i errors occured during copy operation\n", CopyErrorsOccured);
    }
   
    fclose(RegionsFile);
    
    if (IsVolumeLocal) 
    	fclose(Volume); 
    
    FreeBackupMetadataEx(BMEX);
    
    return CopyErrorsOccured == 0;
}


// Volume optional, might pass INVALID_HANDLE_VALUE
BOOLEAN
RestoreDiffVersion(restore_inf R, FILE* Volume) {
    BOOLEAN Result = TRUE;
    BOOLEAN IsVolumeLocal = (NULL == Volume);
    
    if (IsVolumeLocal) {
        printf("Passed volume argument was invalid, creating new handle for %s\n", R.TargetPartition.c_str());
        
        std::string volname = "/dev/";
        volname += R.TargetPartition;

        FILE *Volume = fopen(volname.c_str(), "rb");
        if (NULL != Volume) {
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
            fclose(Volume);
        }
    }
    
    if(IsVolumeLocal)
    	fclose(Volume);
    
    return Result;
}


// Volume optional, might pass INVALID_HANDLE_VALUE
BOOLEAN
RestoreIncVersion(restore_inf R, FILE* Volume) {
    
    if (R.Version == NAR_FULLBACKUP_VERSION && R.Version <= 0) {
        printf("Invalid version ID (%i) for incremental restore\n", R.Version);
        return FALSE;
    }
    
    BOOLEAN Result = TRUE;
    BOOLEAN IsVolumeLocal = (NULL == Volume);
    
    if (IsVolumeLocal) {
        printf("Passed volume argument was invalid, creating new handle for %s\n", R.TargetPartition.c_str());
        std::string volname = "/dev/";
        volname += R.TargetPartition;
        Volume = fopen(volname.c_str(), "rb");
        if (NULL != Volume) {
            printf("Couldnt create local volume handle, terminating now\n");
            Result = FALSE;
        }
    }
    
    
    // NOTE(Batuhan): restores to full first, then incrementally builds volume to version    
    
    restore_inf SubVersionR;
    SubVersionR.RootDir = R.RootDir;
    SubVersionR.BackupID = R.BackupID;
    SubVersionR.TargetPartition = R.TargetPartition;
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
    
    if(IsVolumeLocal)
    	fclose(Volume);
    
    return Result;
}


backup_metadata
ReadMetadata(nar_backup_id ID, int Version, std::wstring RootDir) {
    
    if (RootDir.length() > 0) {
        if (RootDir[RootDir.length() - 1] != L'\\') {
            RootDir += L"\\";
        }
    }
    
    FILE *F = 0;
    std::wstring wFileName = RootDir + GenerateMetadataName(ID, Version);
	std::string cFileName = wstr2str(wFileName);

    F = fopen((char*)cFileName.c_str(), "rb");
    backup_metadata BM = { 0 };
    
    if (F != NULL) {
        if (1==fread(&BM, sizeof(BM), 1, F)) {
            // success
        }
        else {
			printf("Error at readmetadata, rootdir (%S)\n",RootDir.c_str());  
            BM = {0};
            //memset(&BM, 0, sizeof(BM));
            //memset(&BM.Errors, 1, sizeof(BM.Errors)); // Set all error flags
        }
    }
    else {
        printf("Unable to open metadata file %S\n", wFileName.c_str());
        BM = {0};//memset(&BM, 0, sizeof(BM));
        memset(&BM.Errors, 1, sizeof(BM.Errors)); // Set all error flags
    }

    
    if(F != NULL) fclose(F);
    
    return BM;
}

backup_metadata_ex*
InitBackupMetadataEx(const wchar_t *MetadataPath){
    
    BOOLEAN ErrorOccured = TRUE;
    
    std::string cMetadataP = wstr2str(MetadataPath);
    
    backup_metadata_ex* BMEX = new backup_metadata_ex;
    
    FILE *File = fopen(cMetadataP.c_str(), "rb");
    if (File != NULL) {
    
        if (1==fread(&BMEX->M, sizeof(BMEX->M), 1, File)) {
            
            if (0==fseek(File, BMEX->M.Offset.RegionsMetadata, SEEK_SET)) {
                
                BMEX->RegionsMetadata.Data = (nar_record*)malloc(BMEX->M.Size.RegionsMetadata);
                
                if (BMEX->RegionsMetadata.Data != NULL) {
                    BMEX->RegionsMetadata.Count = 0;
                    // TODO(Batuhan): its not safe to assume regions metadata is lower than 4gb, might exceed in very long period
                    // build safe wrapper around read-write file methods to ensure that it is possible to read data more than 4gb
                    // at once
                    if (1==fread(BMEX->RegionsMetadata.Data, BMEX->M.Size.RegionsMetadata, 1, File)) {
                        ErrorOccured = FALSE;
                        // NOTE(Batuhan): probably safe to assume that we wont have more than 2^32 regions, its really big
                        BMEX->RegionsMetadata.Count = (unsigned int)BMEX->M.Size.RegionsMetadata / sizeof(nar_record);
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
        printf("Couldn't open file %S\n", MetadataPath);
    }
    
    if (ErrorOccured) {
        printf("Error occured while initializing metadata extended structure, freeing regions metadata memory\n");
        free(BMEX);
    }
    
    fclose(File);
    return BMEX;
    
}

backup_metadata_ex*
InitBackupMetadataEx(nar_backup_id ID, int Version, std::wstring RootDir) {
    
    if (RootDir.length() > 0) {
        if (RootDir[RootDir.length() - 1] != L'\\') {
            RootDir += L"\\";
        }
    }
    
    
    BOOLEAN ErrorOccured = TRUE;
    
    backup_metadata_ex* BMEX = new backup_metadata_ex;
    
    std::wstring FileName = RootDir;
    FileName += GenerateMetadataName(ID, Version);
    
    std::string cFn = wstr2str(FileName);
    
	 
    BMEX->FilePath = FileName;
    FILE* File = fopen(cFn.c_str(), "rb");
    
    if (File != NULL) {
        
        if (1==fread(&BMEX->M, sizeof(BMEX->M), 1, File)) {
            
            if (0==fseek(File, BMEX->M.Offset.RegionsMetadata, SEEK_SET)) {
                
                BMEX->RegionsMetadata.Data = (nar_record*)malloc(BMEX->M.Size.RegionsMetadata);
                
                if (BMEX->RegionsMetadata.Data != NULL) {
                    BMEX->RegionsMetadata.Count = 0;
                    // TODO(Batuhan): its not safe to assume regions metadata is lower than 4gb, might exceed in very long period
                    // build safe wrapper around read-write file methods to ensure that it is possible to read data more than 4gb
                    // at once
                    if (1==fread(BMEX->RegionsMetadata.Data, BMEX->M.Size.RegionsMetadata, 1, File)) {
                        ErrorOccured = FALSE;
                        // NOTE(Batuhan): probably safe to assume that we wont have more than 2^32 regions, its really big
                        BMEX->RegionsMetadata.Count = (unsigned int)BMEX->M.Size.RegionsMetadata / sizeof(nar_record);
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
    
    fclose(File);
    return BMEX;
    
}


data_array<nar_record>
ReadMFTLCN(backup_metadata_ex* BMEX) {

    if (!BMEX) {
        return { 0, 0 };
    }
    
    data_array<nar_record> Result = { 0 , 0 };
	std::string fp = wstr2str(BMEX->FilePath.c_str());
	
    FILE *File = fopen(fp.c_str(), "rb");
    if (NULL != File) {
        if (fseek(File, BMEX->M.Offset.MFTMetadata, SEEK_SET)) {
            DWORD BytesRead = 0;
            Result.Count = (UINT32)BMEX->M.Size.MFTMetadata / sizeof(nar_record);
            Result.Data = (nar_record*)malloc(BMEX->M.Size.MFTMetadata);
            if (Result.Data) {
                // TODO(Batuhan): same 4gb problem here, should implement safe method to read more than 4gb at once
                if (1 == fread(Result.Data, BMEX->M.Size.MFTMetadata, 1, File)) {
                    // NOTE(Batuhan): Success!
                }
                else {
                    printf("Can't read MFT metadata, read %lu bytes instead of %I64llu\n", BytesRead, BMEX->M.Size.MFTMetadata);
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
    
    fclose(File);
    return Result;
}



BOOLEAN
RestoreRecoveryFile(restore_inf R) {
    //GUID GREC = { 0 }; // recovery partition guid
    //StrToGUID("{de94bba4-06d1-4d40-a16a-bfd50179d6ac}", &GREC);
    
    BOOLEAN Result = FALSE;
#if _MSC_VER
    HANDLE MetadataFile = INVALID_HANDLE_VALUE;
    
    backup_metadata B = ReadMetadata(R.BackupID, NAR_FULLBACKUP_VERSION, R.RootDir);
    //TODO(Batuhan) check integrity of B
    std::wstring MFN = R.RootDir;
    MFN += GenerateMetadataName(R.BackupID, NAR_FULLBACKUP_VERSION);
    
    HANDLE RecoveryPartitionHandle = NarOpenVolume(NAR_RECOVERY_PARTITION_LETTER);
    if (RecoveryPartitionHandle == INVALID_HANDLE_VALUE) {
        
        MetadataFile = CreateFileW(MFN.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
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
#endif

    return Result;
}



inline BOOLEAN
NarFileNameExtensionCheck(const wchar_t *Path, const wchar_t *Extension){
    size_t pl = wcslen(Path);
    size_t el = wcslen(Extension);
    if(pl <= el) return FALSE;
    return (wcscmp(&Path[pl - el], Extension) == 0);
}

inline BOOLEAN
NarFileNameExtensionCheck(const char *Path, const char *Extension){
    size_t pl = strlen(Path);
    size_t el = strlen(Extension);
    if(pl <= el) return FALSE;
    return (strcmp(&Path[pl - el], Extension) == 0);	
}

std::vector<backup_metadata>
NarGetBackupsInDirectory(const char *arg_dir){
	
	std::string dir = std::string(arg_dir);
	if(dir.back() != '/')
		dir += std::string("/");
	
	
	std::string cmdstr = "ls -p \"" + std::string(dir) + "\" > lsresult.txt";
	std::vector<backup_metadata> Result;
	std::vector<std::string> files;
	files.reserve(1000);
	Result.reserve(1000);
	
	system(cmdstr.c_str());
	file_read fr = NarReadFile("lsresult.txt");
	if(fr.Data != 0){
		std::stringstream ss((char*)fr.Data);
		std::string fname;
		FreeFileRead(fr);
		
		while(ss >> fname)
			if(fname.back() != '/')
				files.push_back(fname);
	}
	
	for(auto &fname: files){
		if(NarFileNameExtensionCheck(fname.c_str(), wstr2str(MetadataExtension).c_str())){
			
			FILE *F = fopen((std::string(dir) + fname).c_str(), "rb");
			backup_metadata M;
			
			if(NULL != F && 1 == fread(&M, sizeof(M), 1, F))
				Result.emplace_back(M);				
			
			if(NULL != F) 
				fclose(F);
			
		}
	}
	
	return Result;
	
}

BOOLEAN
NarGetBackupsInDirectory(const wchar_t* Directory, backup_metadata* B, int BufferSize, int* FoundCount) {

#if _MSC_VER
    
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
            
            //NOTE(Batuhan): Do not search for sub-directories, skip folders.
            if (FDATA.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue;
            }
            
            if (BackupFound == MaxBackupCount) {
                Result = FALSE;
            }
            
            //printf("Found file %S\n", FDATA.cFileName);
            
            //NOTE(Batuhan): Compare file name for metadata draft prefix NAR_ , if prefix found, try to read it
            if (NarFileNameExtensionCheck(FDATA.cFileName, MetadataExtension)) {
                
                wcscpy(wstrbuffer, Directory);
                wcscat(wstrbuffer, L"\\");
                wcscat(wstrbuffer, FDATA.cFileName);
                
                HANDLE F = CreateFileW(wstrbuffer, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
                if (F != INVALID_HANDLE_VALUE) {
                    DWORD BR = 0;
                    if (ReadFile(F, &B[BackupFound], sizeof(*B), &BR, 0) && BR == sizeof(*B)) {
                        //NOTE(Batuhan): Since we just compared metadata file name draft, now, just compare full name, to determine its filename, since it might be corrupted
                        
                        std::wstring T = GenerateMetadataName(B[BackupFound].ID, B[BackupFound].Version);
                        
                        if (wcscmp(FDATA.cFileName, T.c_str()) == 0) {
                            //NOTE(Batuhan): File name indicated from metadata and actual file name matches
                            //Even though metadatas match, actual binary data may not exist at all or even, metadata itself might be corrupted too, or missing. Check it
                            
                            //printf("Backup found %S, ver %i\n", FDATA.cFileName, B[BackupFound].Version);
                            
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
                            
                            BackupFound++;
                            
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
#endif

    return TRUE;
    
}





#if 1

#define REGION(Start, End) nar_record{(Start), (End) - (Start)}

// Path: any path name that contains trailing backslash
// like = C:\\somedir\\thatfile.exe    or    \\relativedirectory\\dir2\\dir3\\ourfile.exe
// OutName: output buffer
// OutMaxLen: in characters
inline void
NarGetFileNameFromPath(const wchar_t* Path, wchar_t *OutName, int32_t OutMaxLen) {
    
    if (!Path || !OutName) return;
    
    memset(OutName, 0, OutMaxLen);
    
    size_t PathLen = wcslen(Path);
    
    if (PathLen > (size_t)OutMaxLen) return;
    
    int TrimPoint = PathLen - 1;
    {
        while (Path[TrimPoint] != L'\\' && --TrimPoint != 0);
    }
    
    if (TrimPoint < 0) TrimPoint = 0;
    
    memcpy(OutName, &Path[TrimPoint], 2 * (PathLen - TrimPoint));
    
}


/*
path = path to the file, may be relative or absolute
Out, user given buffer to write filename with extension
Maxout, max character can be written to Out
*/
inline void
NarGetFileNameFromPath(const wchar_t *path, wchar_t* Out, size_t MaxOut){
    
    size_t pl = wcslen(path);
    
    size_t it = pl - 1;
    for(; path[it] != L'\\' && it > 0; it--);
    
    // NOTE(Batuhan): cant fit given buffer, 
    if(pl - it > MaxOut) Out[0] = 0;
    else{
        wcscpy(Out, &path[it]);
    }
    
    
}


// asserts Buffer is large enough to hold all data needed, since caller has information about metadata this isnt problem at all
inline BOOLEAN
ReadMFTLCNFromMetadata(FILE* FHandle, backup_metadata Metadata, void *Buffer){
    
    BOOLEAN Result = FALSE;
    if(NULL!=FHandle){
        
        // Caller must validate metadata integrity, but it's likely that something invalid may be passed here, check it anyway there isnt a cost
        if(fseek(FHandle, Metadata.Offset.MFTMetadata, SEEK_SET)){
            
            DWORD BR = 0;        
            if(1 == fread(Buffer, Metadata.Size.MFTMetadata, 1, FHandle)){
                Result = TRUE;
            }
            else{
                // readfile failed
                printf("Readfile failed for ReadMFTLCNFromMetadata, tried to read %I64lld bytes, instead read %ld\n", Metadata.Size.MFTMetadata, BR);
            }
            
        }
        else{
            printf("Couldnt set file pointer to %I64lld to read mftlcn from metadata\n", Metadata.Offset.MFTMetadata);
            // setfileptr failed
        }
        
    }
    else{
        printf("Metadata handle was invalid, terminating ReadMFTLCNFromMetadata\n");
        // handle was invalid
    }
    
    return Result;
}



// input MUST be sorted
// Finds point Offset in relative to Records structure, useful when converting absolue volume offsets to our binary backup data offsets.
// returns NAR_POINT_OFFSET_FAILED if fails to find given offset, 
inline INT64 
FindPointOffsetInRecords(nar_record *Records, int32_t Len, int64_t Offset){
    
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


#define NAR_FAILED 0
#define NAR_SUCC   1
#define BREAK_CODE __debugbreak();
#define loop for(;;)

int
alternative_main(int argc, char* argv[]) {
    
    {
        /*
        for(char letter = 'A'; letter <= 'Z'; letter ++){
            nar_backup_id id = NarGenerateBackupID(letter);
            std::wcout<< GenerateMetadataName(id, 0) <<L"\n";
        }
        
        
        for(char letter = 'A'; letter <= 'Z'; letter ++){
            nar_backup_id id = NarGenerateBackupID(letter);
            std::wcout<< GenerateBinaryFileName(id, 0) <<L"\n";
        }
        */
        
        return 0;
    }
    
    
    return 0;
    
    /*
    if(0){
        BOOLEAN Result = FALSE;
        char StringBuffer[1024];
        nar_backup_id ID = {0};
        ID.Year = 2020;
        ID.Month = 1;
        ID.Day = 5;
        ID.Hour  = 21;
        ID.Min = 25;
        ID.Letter = 'C';
        
        unsigned Version = -1;
        std::wstring MetadataFilePath = GenerateMetadataName(ID, Version);
        HANDLE MetadataFile = CreateFileW(MetadataFilePath.c_str(), GENERIC_WRITE | GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0);
        if (MetadataFile == INVALID_HANDLE_VALUE) {
            printf("Couldn't create metadata file : %s\n", MetadataFilePath.c_str());
        }
        if(NarSetFilePointer(MetadataFile, sizeof(backup_metadata))){
            char randombf[512];
            DWORD BytesWritten = 0;
            if(WriteFile(MetadataFile, randombf, 512, &BytesWritten, 0) && BytesWritten == 512){
                
            }
            else{
                printf("Writefile failed\n");
                DisplayError(GetLastError());
            }
        }
        else{
            printf("setfilepointer failed\n");
        }
        
        
    }
    */
    
#if 0    
    printf("skip debug?");
    char c = 0;
    std::cin>>c;
    if(!!(c)){
        //DEBUG_FileRestore();
        printf("restore = 1, file explorer = 2 ");
        
        DEBUG_Restore();
        DEBUG_FileExplorer();
        
    }
#endif
    
    /*
    size_t bsize = 64*1024*1024;
    void *MemBuf = malloc(bsize);
    
    LOG_CONTEXT C = {0};
    C.Port = INVALID_HANDLE_VALUE;
    
    
    if(SetupVSS() && ConnectDriver(&C)){
        DotNetStreamInf inf = {0};
        
#if 0        
        SetupStream(&C, (wchar_t)'E', (BackupType)0, &inf);
        loop{
            char temp;
            std::cin>>temp;
            NarRemoveVolumeFromKernelList('E', C.Port);
            Sleep(16);
        }
#endif

        char Volume = 0;
        int Type = 0;
        loop{
            
            memset(&inf, 0, sizeof(inf));
            
            printf("ENTER LETTER TO DO BACKUP \n");
            scanf("%c", &Volume);
            
            BackupType bt = (BackupType)Type;
            
            if(SetupStream(&C, (wchar_t)Volume, bt, &inf)){
                
                int id = GetVolumeID(&C, (wchar_t)Volume);
                volume_backup_inf *v = &C.Volumes.Data[id];
                size_t TotalRead = 0;
                size_t TotalWritten = 0;
                size_t TargetWrite = (size_t)inf.ClusterSize * (size_t)inf.ClusterCount;
                
                HANDLE file = CreateFileW(inf.FileName.c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
                if(file != INVALID_HANDLE_VALUE){
                    
#if 0                    
                    loop{
                        int Read = ReadStream(v, MemBuf, bsize);
                        TotalRead += Read;
                        if(Read == 0){
                            break;
                        }
                        else{
                            DWORD BytesWritten = 0;
                            if(WriteFile(file, MemBuf, Read, &BytesWritten, 0) && BytesWritten == Read){
                                TotalWritten += BytesWritten;
                                // NOTE(Batuhan): copied successfully
                            }
                            else{
                                BREAK_CODE;
                                printf("Couldnt write to file\n");
                                DisplayError(GetLastError());
                            }
                        }
                    }
#else
                    
                    TotalRead = TargetWrite;
                    TotalWritten = TargetWrite;
#endif
                    
                    if((TotalRead != TargetWrite || TotalWritten != TargetWrite)){
                        BREAK_CODE;
                        TerminateBackup(v, NAR_FAILED);
                    }
                    else{
                        TerminateBackup(v, NAR_SUCC);
                    }
                    
                    PrintDebugRecords();
                    
                }
                else{
                    // NOTE(Batuhan): couldnt create file to save backup
                    BREAK_CODE;
                    int ret = TerminateBackup(v, NAR_FAILED);
                    ret++;// to inspect ret in debugging
                }
                
                CloseHandle(file);
                
            }
            else{
                BREAK_CODE;
                printf("couldnt setup stream\n");
            }
        }
    }
    else{
        BREAK_CODE;
    }
    
    return 0;
    */
    
    return 0;
}

#endif
