#include "precompiled.h"
#include "nar.h"

/*
setups iter for finding intersections of two regions. 
Assumes both regions are sorted.
*/
RegionCoupleIter
NarStartIntersectionIter(const nar_record *R1, const nar_record *R2, size_t R1Len, size_t R2Len){
    RegionCoupleIter Result = NarInitRegionCoupleIter(R1, R2, R1Len, R2Len);
    NarNextIntersectionIter(&Result);
    return Result;
}

void
NarNextIntersectionIter(RegionCoupleIter *Iter){
    
    Iter->It = {};
    if(NarIterateRegionCoupleUntilCollision(Iter)){
        uint32_t R1REnd = Iter->R1Iter->StartPos + Iter->R1Iter->Len;
        uint32_t R2REnd = Iter->R2Iter->StartPos + Iter->R2Iter->Len;
        
        // intersection
        uint32_t IntersectionEnd   = MIN(R1REnd, R2REnd);
        uint32_t IntersectionStart = MAX(Iter->R1Iter->StartPos, Iter->R2Iter->StartPos);
        
        Iter->It = {IntersectionStart, IntersectionEnd - IntersectionStart};
        
        if(R1REnd < R2REnd) Iter->R1Iter++;
        else                Iter->R2Iter++;
    }
    
}


/*
Setups Iter for finding exclusion of Ex from Base.
*/
RegionCoupleIter
NarStartExcludeIter(const nar_record *Base, const nar_record *Ex, size_t BaseN, size_t ExN){
    RegionCoupleIter Result = NarInitRegionCoupleIter(Base, Ex, BaseN, ExN);
    Result.__CompRegion = *Result.R1Iter;
    NarNextExcludeIter(&Result);
    return Result;
}



void
NarNextExcludeIter(RegionCoupleIter *Iter){
    
    // R1 == base regions
    // R2 == regions to exclude from base
    
    Iter->It = {};
    
    for(;;){
        
        if(Iter->R1Iter == Iter->R1End){
            return;
        }
        if(Iter->R2Iter == Iter->R2End){
            return;
        }
        
        uint32_t R1REnd = Iter->__CompRegion.StartPos + Iter->__CompRegion.Len;
        uint32_t R2REnd = Iter->R2Iter->StartPos + Iter->R2Iter->Len;
        
        if(R1REnd < Iter->R2Iter->StartPos){
            Iter->It = Iter->__CompRegion;
            Iter->R1Iter++;
            Iter->__CompRegion = (Iter->R1Iter != Iter->R1End) ? *Iter->R1Iter : nar_record{};
            return;
        }
        if(R2REnd < Iter->__CompRegion.StartPos){
            Iter->R2Iter++;
            continue;
        }
        
        break;
    }
    
    
    
    ASSERT(Iter->R1Iter < Iter->R1End);
    ASSERT(Iter->R2Iter < Iter->R2End);
    
    // NOTE(Batuhan): this diagram can't be seen properly in 4coder..
    // open file in notepad or something simple.
    
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
    
    /*
for from left   : loop till end of collision
for from right  : terminate
for from middle : return left one, make __compregion right one
for overshadow  : loop till end of collision
*/
    
    for(;;){
        
        uint32_t BEnd = Iter->__CompRegion.StartPos + Iter->__CompRegion.Len;
        uint32_t EEnd = Iter->R2Iter->StartPos + Iter->R2Iter->Len;
        
        if(Iter->R1Iter == Iter->R1End){
            break;
        }
        if(Iter->R2Iter == Iter->R2End){
            break;
        }
        
        if((Iter->__CompRegion.StartPos < Iter->R2Iter->StartPos && BEnd < Iter->R2Iter->StartPos)
           || (Iter->__CompRegion.StartPos > EEnd))
        {
            Iter->R1Iter++;
            Iter->__CompRegion = Iter->R1Iter != Iter->R1End ? *Iter->R1Iter : nar_record{};
            // no collision
            return;
        }
        
        // collision from left
        if(Iter->__CompRegion.StartPos >= Iter->R2Iter->StartPos &&
           EEnd >= Iter->R2Iter->StartPos && EEnd < BEnd){
            Iter->It.StartPos = EEnd;
            Iter->It.Len      = BEnd -EEnd;
            ASSERT(BEnd > EEnd);
            Iter->__CompRegion = {EEnd, BEnd - EEnd};
            Iter->R2Iter++;
        }
        // collision from right
        else if(Iter->R2Iter->StartPos > Iter->__CompRegion.StartPos && 
                Iter->R2Iter->StartPos <= BEnd && EEnd >= BEnd){
            Iter->It.StartPos = Iter->__CompRegion.StartPos;
            Iter->It.Len      = Iter->R2Iter->StartPos - Iter->__CompRegion.StartPos;
            ASSERT(BEnd > Iter->R2Iter->StartPos);
            Iter->R1Iter++;
            Iter->__CompRegion = (Iter->R1Iter != Iter->R1End) ? *Iter->R1Iter : nar_record{};
            break;
        }
        // collision from middle
        else if(Iter->R2Iter->StartPos > Iter->It.StartPos &&
                EEnd < BEnd){
            Iter->It.StartPos = Iter->__CompRegion.StartPos;
            Iter->It.Len      = Iter->R2Iter->StartPos - Iter->__CompRegion.StartPos;
            ASSERT(Iter->R2Iter->StartPos > Iter->__CompRegion.StartPos);
            Iter->__CompRegion = {EEnd, BEnd - EEnd};
            Iter->R2Iter++;
            break;
        }
        // r2 overshadows r1 completely
        else if(Iter->R2Iter->StartPos <= Iter->__CompRegion.StartPos &&
                EEnd >= BEnd){
            Iter->It.StartPos = Iter->__CompRegion.StartPos;
            Iter->R1Iter++;
            Iter->__CompRegion = Iter->R1Iter != Iter->R1End ? *Iter->R1Iter : nar_record{};
            return NarNextExcludeIter(Iter);
        }
        else{
            ASSERT(false);
            break;
        }
        
    }
    
    
}

inline RegionCoupleIter
NarInitRegionCoupleIter(const nar_record *Base, const nar_record *Ex, size_t BaseN, size_t ExN){
    RegionCoupleIter Result = {};
    
    Result.R1     = Base;
    Result.R1Len  = BaseN;
    Result.R1Iter = Result.R1;
    Result.R1End  = Result.R1 + Result.R1Len;
    
    Result.R2     = Ex;
    Result.R2Len  = ExN;
    Result.R2Iter = Result.R2;
    Result.R2End  = Result.R2 + Result.R2Len;
    
    Result.It     = {0, 0}; 
    return Result;
}


// returns true if it finds valid collision block. false is it depletes 
// Iter
bool
NarIterateRegionCoupleUntilCollision(RegionCoupleIter *Iter){
    
    for(;;){
        if(Iter->R1Iter == Iter->R1End){
            break;
        }
        if(Iter->R2Iter == Iter->R2End){
            break;
        }
        
        uint32_t R1REnd = Iter->R1Iter->StartPos + Iter->R1Iter->Len;
        uint32_t R2REnd = Iter->R2Iter->StartPos + Iter->R2Iter->Len;
        if(R1REnd < Iter->R2Iter->StartPos){
            Iter->R1Iter++;
            continue;
        }
        if(R2REnd < Iter->R1Iter->StartPos){
            Iter->R2Iter++;
            continue;
        }
        
        break;
    }
    
    return !__NarIsRegionIterExpired(*Iter);
}


bool
NarIsRegionIterValid(RegionCoupleIter Iter){
    return (Iter.It.Len != 0);
}


bool
__NarIsRegionIterExpired(RegionCoupleIter Iter){
    return (Iter.R1Iter == Iter.R1End || Iter.R2Iter == Iter.R2End);
}


void
NarGetPreviousBackupInfo(int32_t Version, BackupType Type, int32_t *OutVersion){
    
    ASSERT(OutVersion);
    if(Type == BackupType::Diff){
        if(Version != NAR_FULLBACKUP_VERSION){
            *OutVersion = NAR_FULLBACKUP_VERSION;
        }
        else{
            *OutVersion = NAR_INVALID_BACKUP_VERSION;
        }
    }
    else if(Type == BackupType::Inc){
        if(Version == NAR_FULLBACKUP_VERSION){
            *OutVersion = NAR_INVALID_BACKUP_VERSION;
        }
        else{
            *OutVersion = --Version;
        }
    }
    else{
        ASSERT(false);
    }
    
}


size_t
NarLCNToVCN(nar_record *LCN, size_t LCNCount, size_t Offset){
    bool Found = false;
    size_t Acc = 0;
    for(size_t i = 0; i<LCNCount; i++){
        
        if(Offset>= LCN[i].StartPos && Offset < LCN[i].StartPos + LCN[i].Len){
            ASSERT(Offset >= LCN[i].StartPos);
            uint32_t DiffToStart =  Offset - LCN[i].StartPos;
            Acc += DiffToStart;
            Found = true;
            break;
        }
        
        ASSERT(i != LCNCount - 1);
        Acc += LCN[i].Len;
    }
    
    ASSERT(Found == true);
    
    return Acc;
}



/*
    ASSUMES RECORDS ARE SORTED
THIS FUNCTION REALLOCATES MEMORY VIA realloc(), DO NOT PUT MEMORY OTHER THAN ALLOCATED BY MALLOC, OTHERWISE IT WILL CRASH THE PROGRAM
*/
void
MergeRegions(data_array<nar_record>* R) {
    
    
    uint32_t MergedRecordsIndex = 0;
    uint32_t CurrentIter = 0;
    
    for (;;) {
        if (CurrentIter >= R->Count) {
            break;
        }
        
        uint32_t EndPointTemp = R->Data[CurrentIter].StartPos + R->Data[CurrentIter].Len;
        
        if (IsRegionsCollide(R->Data[MergedRecordsIndex], R->Data[CurrentIter])) {
            uint32_t EP1 = R->Data[CurrentIter].StartPos + R->Data[CurrentIter].Len;
            uint32_t EP2 = R->Data[MergedRecordsIndex].StartPos + R->Data[MergedRecordsIndex].Len;
            
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


/*
*/
void
MergeRegionsWithoutRealloc(data_array<nar_record>* R) {
    
    
    uint32_t MergedRecordsIndex = 0;
    uint32_t CurrentIter = 0;
    
    for (;;) {
        if (CurrentIter >= R->Count) {
            break;
        }
        
        uint32_t EndPointTemp = R->Data[CurrentIter].StartPos + R->Data[CurrentIter].Len;
        
        if (IsRegionsCollide(R->Data[MergedRecordsIndex], R->Data[CurrentIter])) {
            uint32_t EP1 = R->Data[CurrentIter].StartPos + R->Data[CurrentIter].Len;
            uint32_t EP2 = R->Data[MergedRecordsIndex].StartPos + R->Data[MergedRecordsIndex].Len;
            
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
    //R->Data = (nar_record*)realloc(R->Data, sizeof(nar_record) * R->Count);
}


inline bool
IsRegionsCollide(nar_record R1, nar_record R2) {
    
    uint32_t R1EndPoint = R1.StartPos + R1.Len;
    uint32_t R2EndPoint = R2.StartPos + R2.Len;
    
    if (R1.StartPos == R2.StartPos && R1.Len == R2.Len) {
        return true;
    }
    
    
    if ((R1EndPoint <= R2EndPoint
         && R1EndPoint >= R2.StartPos)
        || (R2EndPoint <= R1EndPoint
            && R2EndPoint >= R1.StartPos)
        ) {
        return true;
    }
    
    return false;
}




// input MUST be sorted
// Finds point Offset in relative to Records structure, useful when converting absolue volume offsets to our binary backup data offsets.
// returns NAR_POINT_OFFSET_FAILED if fails to find given offset, 
point_offset 
FindPointOffsetInRecords(nar_record *Records, uint64_t Len, int64_t Offset){
    
    if(!Records) return {0};
    
    point_offset Result = {0};
    
    bool Found = false;
    
    for(uint64_t i = 0; i < Len; i++){
        
        if(Offset <= (int64_t)Records[i].StartPos + (int64_t)Records[i].Len){
            
            int64_t Diff = (Offset - (int64_t)Records[i].StartPos);
            if (Diff < 0) {
                // Exceeded offset, this means we cant relate our Offset and Records data, return failcode
                Found = false;
            }
            else {
                Found = true;
                Result.Offset        += Diff;
                Result.Indice         = i;
                Result.Readable       = (int64_t)Records[i].Len - Diff;
            }
            
            break;
            
        }
        
        
        Result.Offset += Records[i].Len;
        
    }
    
    
    return (Found ? Result : point_offset{0});
}



int
CompareNarRecords(const void* v1, const void* v2) {
    
    nar_record* n1 = (nar_record*)v1;
    nar_record* n2 = (nar_record*)v2;
    
#if 0    
    if(n1->StartPos == n2->StartPos){
        return (int)((int64_t)n1->Len - (int64_t)n2->Len);
    }
    else{
        return (int)((int64_t)n1->StartPos - (int64_t)n2->StartPos);
    }
#endif
    
#if 1    
    // old version
    if (n1->StartPos == n2->StartPos && n2->Len < n1->Len) {
        return 1;
    }
    
    if (n1->StartPos > n2->StartPos) {
        return 1;
    }
    
    return -1;
#endif
    
}


