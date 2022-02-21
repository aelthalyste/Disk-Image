#include "precompiled.h"
#include "nar.hpp"
#include "platform_io.hpp"



static uint32_t debug_left = 0;
static uint32_t debug_mid = 0;
static uint32_t debug_right = 0;
static uint32_t debug_overrun = 0;

static uint32_t debug_overrun_c0 = 0;
static uint32_t debug_overrun_c1 = 0;

static uint32_t debug_left_c0 = 0;
static uint32_t debug_left_c1 = 0;
static uint32_t debug_left_c2 = 0;


static uint32_t check_for_write_contiunity = 0;


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
            uint64_t DiffToStart =  Offset - LCN[i].StartPos;
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

    // equality
    if (n1->StartPos == n2->StartPos && n2->Len == n1->Len)
        return 0;

    if (n1->StartPos == n2->StartPos && n2->Len < n1->Len) {
        return 1;
    }
    
    if (n1->StartPos > n2->StartPos) {
        return 1;
    }
    
    return -1;
#endif
    
}


UTF8 **GetFilesInDirectoryWithExtension(const UTF8 *DirectoryAsUTF8, uint64_t *OutCount, UTF8 *Extension) {

    wchar_t *Directory = NarUTF8ToWCHAR(DirectoryAsUTF8);
    defer({free(Directory);});

    uint64_t DirLen = wcslen(Directory);
    DirLen++; // null termination
    
    wchar_t *WildcardDir = (wchar_t *)calloc(DirLen, sizeof(WildcardDir[0]));;
    defer({free(WildcardDir);});

    wcscat(WildcardDir, Directory);
    wcscat(WildcardDir, L"\\*");

    wchar_t *ExtensionAsWCHR = NULL;
    defer({if (ExtensionAsWCHR) free(ExtensionAsWCHR);});

    if (Extension) {
        ExtensionAsWCHR = NarUTF8ToWCHAR(Extension);
    }

    WIN32_FIND_DATAW FDATA;
    HANDLE FileIterator = FindFirstFileW(WildcardDir, &FDATA);
    
    uint64_t ResultCap = 1024;
    UTF8 **Result = (UTF8 **)calloc(ResultCap, sizeof(Result[0]));
    uint64_t Count = 0;

    uint32_t TempBufferCap = 1024 * 1024 * 8;
    wchar_t *TempBuffer    = (wchar_t *)calloc(TempBufferCap, 1);

    if (FileIterator != INVALID_HANDLE_VALUE) {

        while (FindNextFileW(FileIterator, &FDATA) != 0) {

            //@NOTE(Batuhan): Do not search for sub-directories, skip folders.
            if (FDATA.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue;
            }

            // extension check
            if (ExtensionAsWCHR != NULL) {
                wchar_t *DS = wcsrchr(FDATA.cFileName, L'.');
                if (DS) {
                    // ++DS;// skip dot
                    if (wcscmp(DS, ExtensionAsWCHR) == 0) {
                        // everything is ok
                    } 
                    else {
                        continue;
                    }
                }
                else {
                    continue;
                }
            }

            // @Stupid +5 just to be sure there is enough room for null termination
            uint64_t FnLen = wcslen(FDATA.cFileName);
            FnLen += DirLen + 5;

            uint32_t fl = (uint32_t)FnLen;
            fl+=1; // for //
            fl+=(uint32_t)DirLen;
            fl+=1; // for null termination

            if (fl * 2 > TempBufferCap) {
                TempBufferCap = fl * 2 * 2;// *2 for wchar_t size, *2 for growing
                TempBuffer = (wchar_t *)realloc(TempBuffer, TempBufferCap);
            }

            TempBuffer[0] = 0;
            wcscat(TempBuffer, Directory);
            wcscat(TempBuffer, L"\\");
            wcscat(TempBuffer, FDATA.cFileName);

            uint32_t NewBufferSize = NarGetWCHARToUTF8ConversionSize(TempBuffer);
            UTF8 *UTF8Path = NarWCHARToUTF8(TempBuffer);

            Result[Count++] = UTF8Path;

            if (Count == ResultCap) {
                ResultCap *= 2;
                Result = (UTF8 **)realloc(Result, ResultCap*sizeof(Result[0]));
            }

        }


    }
    else {
        printf("Cant iterate directory\n");
    }


    FindClose(FileIterator);
    *OutCount = Count;
    
    return Result;

}

UTF8 **GetFilesInDirectory(const UTF8 *Directory, uint64_t *OutCount) {
    return GetFilesInDirectoryWithExtension(Directory, OutCount, NULL);
}

void FreeDirectoryList(UTF8 **List, uint64_t Count) {
    for(uint64_t i=0;i<Count;++i)
        free(List[i]);
    free(List);
}


int BackupPackageComp(const void *v1, const void *v2) {
    backup_package *p1 = (backup_package *)v1;
    backup_package *p2 = (backup_package *)v2;

    ASSERT(p1->BackupInformation.Version != p2->BackupInformation.Version);
    if (p1->BackupInformation.Version < p2->BackupInformation.Version)
        return -1;
    else
        return 1;

}


packages_for_restore LoadPackagesForRestore(const UTF8 *Directory, nar_backup_id BackupID, int32_t Version) {
    

    packages_for_restore Result;

    Result.Count = Version + 1;
    Result.Packages = (backup_package *)calloc(Result.Count, sizeof(Result.Packages[0]));
    
    int FoundCount = NarGetBackupsInDirectoryWithFilter(Directory, Result.Packages, Result.Count, &BackupID, Version);

    if (FoundCount) {
        bool Failed = false;

        // sort packages
        qsort(Result.Packages, FoundCount, sizeof(Result.Packages[0]), BackupPackageComp);

        if (Result.Packages[0].BackupInformation.BT == BackupType::Inc) {
            // @TODO : proper validation
            // validate inc backup, scan all versions
            
            if (FoundCount == Result.Count) {
                for(int64_t i=0;i<FoundCount;++i) {
                    if (Result.Packages[i].BackupInformation.Version != i) {
                        Failed = true;
                        LOG_ERROR("Unable to load packages for restore. Versions are not ordered!"); 
                        break;
                    }
                }
            }
            else {
                LOG_ERROR("Unable to load packages for restore. Incremental backup chain expected %d packages, but got %d", Result.Count, FoundCount);
                Failed = true;
            }

        }
        else {

            // @TODO : proper validation
            // validate diff backup, skip mid versions
            if (FoundCount == 2) {

                if (Result.Packages[0].BackupInformation.Version == 0
                    && Result.Packages[1].BackupInformation.Version == Version) {
                    // everything is ok
                }
                else {
                    LOG_ERROR("Unable to load packages for restore. Version numbers for diff restore is weird. First one is %d, second one is %d, expected 0-%d", Result.Packages[0].BackupInformation.Version, Result.Packages[1].BackupInformation.Version, Version);   
                    Failed = true;
                }

            }
            else {
                LOG_ERROR("Unable to load packages for restore. Diff backups expected exactly 2 packages, but got %d\n", FoundCount); 
                Failed = true;
            }

        }


        // validate chain integrity

        if (Failed) {
            LOG_ERROR("Error : Couldn't verify chain integrity.\n");
            FreePackagesForRestore(&Result);
        }

    }
    else 
        FreePackagesForRestore(&Result);
    


    return Result;
}


void FreePackagesForRestore(packages_for_restore *Packages) {
    if (!Packages) return;
    NarFreeBackupPackages(Packages->Packages, Packages->Count);
    free(Packages->Packages);
    memset(Packages,0,sizeof(*Packages));
}


backup_package *GetLatestPackage(packages_for_restore *Packages) {
    return &Packages->Packages[Packages->Count - 1];
}

backup_package *GetPreviousPackage(packages_for_restore *Packages, backup_package *Current) {

    if (Current->BackupInformation.Version == NAR_FULLBACKUP_VERSION)
        return NULL;

    // inc backup 
    if (Current->BackupInformation.BT == BackupType::Inc) 
        return Packages->Packages + (Current->BackupInformation.Version - 1);
    else if (Current->BackupInformation.BT == BackupType::Diff)
        return &Packages->Packages[0];        


    return reinterpret_cast<backup_package *>(0xdeaddeadull);
}



bool InitRestore(Restore_Ctx *ctx, const UTF8 *DirectoryToLook, nar_backup_id BackupID, int32_t Version) {

    ASSERT(ctx);
    ASSERT(DirectoryToLook);
    ASSERT(Version >= -1);

    uint64_t PackageCount = 0;
    packages_for_restore Packages = LoadPackagesForRestore(DirectoryToLook, BackupID, Version);
    defer({FreePackagesForRestore(&Packages);});
    
    if (!Packages.Count) {
        printf("Unable to load packages for restore! InitRestore failed.\n");
        return false;
    }

    Array<USN_Extent> written_extents;
    defer({arrfree(&written_extents);});

    backup_package *P = GetLatestPackage(&Packages);


    uint64_t RegionCount = 0;
    nar_record *Records = (nar_record *)P->Package.get_entry("regions", &RegionCount);
    RegionCount /= sizeof(nar_record);

    Extent_Exclusion_Iterator exclusion_iter;
    defer({free_exclusion_iterator(&exclusion_iter);});

    Array<USN_Extent> Extents;
    // init exc iterator for the first time.
    {
        Extents.data = Records;
        Extents.len  = RegionCount;
        Extents.cap  = RegionCount;

        exclusion_iter = init_exclusion_iterator(Extents, written_extents);
        USN_Extent fsext;
        fsext.off = P->BackupInformation.OriginalSizeOfVolume/P->BackupInformation.ClusterSize;
        fsext.len = Gigabyte(3);
        arrput(&written_extents, fsext);
    }

    arrreserve(&ctx->instructions, 1024);

    Array<nar_record> backup_regions;

    // @NOTE : pre-determine all reads-write locations and sort according to write extents to make better api for caller
    // so they don't have to call file seek at target destination. 
    for (;;) {
        // addition loop

        Restore_Instruction ri;
        USN_Extent write_extent = {};

        if (!iterate_next_extent(&write_extent, &exclusion_iter)) {
            
            P = GetPreviousPackage(&Packages, P);

            if (P != NULL) {
                // there are still versions to apply!
                USN_Extent file_size_extent;
                file_size_extent.off = P->BackupInformation.OriginalSizeOfVolume;
                file_size_extent.len = Gigabyte(3);
                arrput(&written_extents, file_size_extent);

                sort_and_merge_extents(written_extents);
                free_exclusion_iterator(&exclusion_iter);

                
                backup_regions.data = (nar_record *)P->Package.get_entry("regions", &backup_regions.len);
                backup_regions.len /= sizeof(nar_record);
                backup_regions.cap = backup_regions.len;

                exclusion_iter = init_exclusion_iterator(backup_regions, written_extents);
                
                // retry with new backup block
                continue;
            }
            else {
                sort_restore_instructions(ctx->instructions);

                // @NOTE(batuhan) : windows doesn't report file resizing as overwrite operation, it doesn't even report what file size has become. This may sound like 
                // non-brained API behaviour, yes it is. In past, old days, it wasn't even problem for us, it's wasn't even condition we would have to handle carefully. 
                // We were executing our instructions out of order, meaning our write operations weren't sequential, we were seeking file there and there. This  
                // kind of restore was silently handling all extend-truncate operations(there is a big hidden assumption I haven't checked yet). So, extending file by 500mb  
                // and writing nothing to it would be handled by the system, WITHOUT EVEN COPYING SINGLE BYTE. That's the kind of behaviour we want. 
                // But sadly, exposing this knowledge to API users is hard. For every endpoint, this system has to be introduced to API user and hope they got it in
                // first try and don't come up with weird bug reports that's completely due to their misunderstanding. It's just too many friction to what this simple system trying to achieve.
                // So I just reordered instructions according to their write offsets, made them all sequential writes, so user can just read and write directly. 
                // But if file change history has extend record, there would be gaps between write offsets, so I have to manually find those gaps and patch them with
                // special instruction that costs 0 file IO(user still has to read that much bytes, 0 cost is for my backend), just zeroed memory and send it.
                // 1/31/2022   
                {
                    s64 current_write_offset = 0;
                    for_array(i, ctx->instructions){
                        BG_ASSERT(ctx->instructions[i].where_to_write.len==ctx->instructions[i].where_to_read.len);

                        // check for discontiunity.
                        if(ctx->instructions[i].where_to_write.off != current_write_offset) {
                            BG_ASSERT(ctx->instructions[i].where_to_write.off > current_write_offset);
                            s64 patch_length = ctx->instructions[i].where_to_write.off - current_write_offset;

                            Restore_Instruction patch;
                            zero_memory(&patch, sizeof(patch));
                            patch.instruction_type   = ZERO;
                            patch.version            = -1;
                            patch.where_to_read.len  = patch_length;
                            patch.where_to_write.len = patch_length;
                            
                            arrins(&ctx->instructions, i, patch);
                        }

                        current_write_offset += ctx->instructions[i].where_to_write.len;
                    }
                }



                if (check_for_write_contiunity) {
                    
                    s64 current_write_offset = 0;

                    bool err_occured = false;
                    for_array(i, ctx->instructions) {
                        
                        if (ctx->instructions[i].instruction_type == NORMAL) {
                            if (ctx->instructions[i].where_to_write.len != ctx->instructions[i].where_to_read.len) {
                                LOG_DEBUG("FATAL ERROR : %4d'th instruction's read and write lengths doesnt match!. read extent was : [%10lld][%10lld], write extent was [%10lld][%10lld]", i, ctx->instructions[i].where_to_read.off, ctx->instructions[i].where_to_read.len, ctx->instructions[i].where_to_write.off, ctx->instructions[i].where_to_write.len); 
                                err_occured=true;
                            }
                            if (ctx->instructions[i].where_to_write.off != current_write_offset) {
                                LOG_DEBUG("FATAL ERROR : %4d'th instruction's write offset doesn't match what we add up so far. Read sum was %lld, but write extent says to write %lld. We probably messed up in backup and missed up some extents, which happens a lot if the file is extended, USN_REASON_EXTEND", i, current_write_offset, ctx->instructions[i].where_to_write.off);
                                LOG_DEBUG("Breaking this loop, since rest of the instructions will be failing with exact same reason. Once you enter this route, you can't roll back! So if there are additional errors or some weirdness, they wont be reported back");
                                err_occured=true;
                                break; 
                            }
                        }

                        current_write_offset += ctx->instructions[i].where_to_write.len;
                    }
                    
                    if (current_write_offset!=(s64)ctx->target_file_size) {
                        LOG_DEBUG("FATAL ERROR : Extents summed up doesn't match target file size. This may indicate there is discontiunity in the instruction stream or something is really messed up at the backup stage. (version : %d)", P->BackupInformation.Version);
                        err_occured=true;
                    }
                    if(err_occured) {
                        LOG_DEBUG("Error detected when scanning for discontiunity, emitting instructions..[instruction_index][version][read_instruction][what_we_add_up_so_far_from_reading][write_instruction][if discontiunity detected!]");
                        s64 what_we_add_up_so_far_from_reading = 0;
                        for_array(i, ctx->instructions) {
                            bool dc=false;
                            if (ctx->instructions[i].where_to_write.len != ctx->instructions[i].where_to_read.len) {
                                dc=true;
                            }
                            if (ctx->instructions[i].where_to_write.off != current_write_offset) {
                                dc=true;
                            }  
                            const char *dc_string = dc?" ":"DC!";
                            LOG_DEBUG("[%4d] - [%3d] -- [%10lld][%10lld] -- [%10lld] -- [%10lld][%10lld] %s", i, ctx->instructions[i].version, ctx->instructions[i].where_to_read.off, ctx->instructions[i].where_to_read.len, what_we_add_up_so_far_from_reading, ctx->instructions[i].where_to_write.off, ctx->instructions[i].where_to_write.len, dc_string);  
                            what_we_add_up_so_far_from_reading += ctx->instructions[i].where_to_read.len;
                        }
                    }

                }

                return true;                    
            }


        }

        // @TODO : if compression is enabled, some additional boring stuff needed has to be done!


        ri.instruction_type   = NORMAL;
        ri.where_to_write     = write_extent;
        ri.where_to_read.off  = extent_offset_to_file_offset(backup_regions, write_extent.off);
        ri.where_to_read.len  = ri.where_to_write.len;
        ri.version            = P->BackupInformation.Version;

        BG_ASSERT(ri.where_to_read.off >= 0);

        arrput(&ctx->instructions, ri);
        arrput(&written_extents, ri.where_to_write);
    }





    return false;
}

void BG_API FreeRestoreCtx(Restore_Ctx *ctx) {
    arrfree(&ctx->instructions);
}

bool NarCompareBackupID(nar_backup_id id1, nar_backup_id id2) {
    return 0 == memcmp(&id1, &id2, sizeof(id1));
}



int32_t NarGetBackupsInDirectoryWithFilter(const UTF8 *Directory, backup_package *output, int MaxCount, nar_backup_id *FilteredID, int32_t MaxVersion) {

    int32_t Count = 0;
    uint64_t FileCount = 0;
    UTF8 **Files = GetFilesInDirectoryWithExtension(Directory, &FileCount, NAR_METADATA_EXTENSION);
    ASSERT(Files);
    if (Files) {
        
        for(int i=0;i<FileCount;++i) {

            if (Count == MaxCount)
                break;
            
            file_read fr = NarReadFile(Files[i]);
            if (fr.Data) {
                bool Added = false;

                if (init_package_reader_from_memory(&output[Count].Package, fr.Data, fr.Len)) {
                    backup_package *p = &output[Count];
                    p->Package.do_i_own_data = true;
                    uint64_t s;
                    void *BInfPtr = p->Package.get_entry("backup_information", &s);
                    if (BInfPtr) {
                        
                        backup_information *BInf = (backup_information *)BInfPtr;
                        bool skip = false;

                        if (FilteredID != NULL) {
                        
                            if (!NarCompareBackupID(*FilteredID, BInf->BackupID))
                                skip = true;    

                            if (MaxVersion != NAR_NO_VERSION_FILTER) {
                                if (BInf->BT == Diff) {
                                    // diff backup only needs full + itself
                                    if (MaxVersion != BInf->Version && MaxVersion != 0)
                                        skip = true;
                                }
                                else if (BInf->BT == Inc) {
                                    // do nothing
                                }
                                else {
                                    ASSERT(false);
                                }
                            }

                        }
                        

                        if (BInf->Version > MaxVersion) 
                            skip = true;
                        

                        if (!skip) {
                            memcpy(&p->BackupInformation, BInfPtr, sizeof(p->BackupInformation));
                            p->Path = (UTF8 *)utf8dup(Files[i]);
                            ++Count;
                            Added = true;
                        }   

                    }
                    else {
                        // package doesn't contain backup_information, while this is unlikely, we might encounter a file with extension that didn't build up by us, while magic number and other stuff matching.
                    } 

                }

                if (!Added) {
                    FreeFileRead(fr);
                }
            }


        }
    }

    FreeDirectoryList(Files, FileCount);

    return Count;    
}


int32_t NarGetBackupsInDirectory(const UTF8 *Directory, backup_package *output, int MaxCount) {
    return NarGetBackupsInDirectoryWithFilter(Directory, output, MaxCount, NULL, 4 * 1024 * 1024);
}

void NarFreeBackupPackages(backup_package *Packages, int32_t Count) {
    for(int i=0;i<Count;++i) {
        free_package_reader(&Packages[i].Package);
        free(Packages[i].Path);
    }
}


Array<Array<backup_information_ex>> NarGetChainsInDirectory(const UTF8 *Directory) {

    Array<Array<backup_information_ex>> Result;

    int32_t PackageCap = 1024 * 64 * 1;
    backup_package *Packages = (backup_package *)calloc(sizeof(*Packages), PackageCap);
    int32_t FoundCount = NarGetBackupsInDirectory(Directory, Packages, PackageCap);
    
    for (int pi=0;pi<FoundCount;++pi) {

        int AddedToResult = 0;
        for (int j=0;j<Result.len;++j) {
            if (NarCompareBackupID(Result[j][0].BackupID, Packages[pi].BackupInformation.BackupID)) {
                
                // add to array
                {
                    backup_information_ex *P = arrputptr(&Result[j]);
                    memcpy(P, &Packages[pi].BackupInformation, sizeof(Packages[pi].BackupInformation));
                    P->Path =  utf8dup(Packages[pi].Path);
                }

                AddedToResult=1;
                break;
            }
        }


        // create new entry for chain if we didn't encounter it in result array.
        if (!AddedToResult) {
            Array<backup_information_ex> t;
            arrinit(&t, 16);
            arrput(&Result, t);
            backup_information_ex *P = arrputptr(&Result[Result.len-1]);
            memcpy(P, &Packages[pi].BackupInformation, sizeof(Packages[pi].BackupInformation));
            P->Path = utf8dup(Packages[pi].Path);
        }

    }
    
    NarFreeBackupPackages(Packages, FoundCount);

    return Result;
}









// REGION USN_BACKUP CODES!!!


Extent_Exclusion_Iterator
init_exclusion_iterator(Array<USN_Extent> base, Array<USN_Extent> to_be_excluded) {
    Extent_Exclusion_Iterator result = {};

    Array<USN_Extent> base_copy;
    base_copy.data = (USN_Extent *)calloc(base.len, sizeof(base.data[0]));
    base_copy.len  = base.len;
    memcpy(base_copy.data, base.data, base.len * sizeof(base.data[0]));

    slice_from_array(&result.base, base_copy);    
    slice_from_array(&result.to_be_excluded, to_be_excluded);
    correctly_align_exclusion_extents(&result);
    return result;
}

void
free_exclusion_iterator(Extent_Exclusion_Iterator *iter) {
    free(iter->base.data);
    *iter = {};
}


int32_t
qs_comp_restore_inst_fn(const void *p1, const void *p2) {
    Restore_Instruction inst1 = *(Restore_Instruction *)p1;
    Restore_Instruction inst2 = *(Restore_Instruction *)p2;
    return qs_comp_extent_fn(&inst1.where_to_write, &inst2.where_to_write);
}

void
sort_restore_instructions(Array<Restore_Instruction> & arr) {
    qsort(arr.data, arr.len, sizeof(arr[0]), qs_comp_restore_inst_fn);
}



void
correctly_align_exclusion_extents(Extent_Exclusion_Iterator *iter) {
    if (iter->to_be_excluded.len && iter->excl_indc < iter->to_be_excluded.len) {
        USN_Extent be = iter->base[iter->base_indc];
        USN_Extent ee = iter->to_be_excluded[iter->excl_indc];
        while (false == is_extents_collide(ee, be)) {
            if (ee.off > be.off) {
                break;
            }
            if (iter->excl_indc + 1 != iter->to_be_excluded.len)
                iter->excl_indc++;
            else
                break;

            ee = iter->to_be_excluded[iter->excl_indc];
        }
    }
}


bool nar_debug_check_if_we_touched_all_extent_scenarios(void) {
    #define CE(x) LOG_DEBUG("%-20s -> %s", #x, ((x) ? "PASS" : "ERROR"));

    CE(debug_left);
    CE(debug_mid);
    CE(debug_right);
    CE(debug_overrun);

    CE(debug_overrun_c0);
    CE(debug_overrun_c1);

    CE(debug_left_c0);
    CE(debug_left_c1);
    CE(debug_left_c2);

    #undef CE
    return (debug_left && debug_mid && debug_right && debug_overrun && debug_overrun_c0 && debug_overrun_c1 && debug_left_c0 && debug_left_c1 && debug_left_c2);
}

void nar_debug_reset_internal_branch_states(void) {
    debug_left_c0 = 0;
    debug_left_c1 = 0;
    debug_left_c2 = 0;

    debug_overrun = 0;
    debug_overrun_c0 = 0;
    debug_overrun_c1 = 0;        
}

void nar_debug_reset_touch_states(void) {
    debug_left = 0;
    debug_mid = 0;
    debug_right = 0;

    debug_overrun = 0;
}


// returns false if unable to iterate next extent.
bool
iterate_next_extent(USN_Extent *result, Extent_Exclusion_Iterator *iter) {
    
    if (iter->base_indc == iter->base.len)
        return false;

    correctly_align_exclusion_extents(iter);

    result->off = 0;
    result->len = 0;

    // to catch stupid bugs
    defer({ASSERT(result->len != 0);});
    
    // no more extents to excluded, return base extents without filtering.
    if (iter->excl_indc == iter->to_be_excluded.len) {
        *result = iter->base[iter->base_indc];
        iter->base_indc++;
        return true;
    }


    if (false == is_extents_collide(iter->base[iter->base_indc], iter->to_be_excluded[iter->excl_indc])) {
        *result = iter->base[iter->base_indc];

        // iterate exclusion region if it falls behind the base slice.
        if (iter->base[iter->base_indc].off > iter->to_be_excluded[iter->excl_indc].off) {
            if (iter->excl_indc < iter->to_be_excluded.len) {
                iter->excl_indc++;
            }
        }
        
        iter->base_indc++;

        return true;
    }



    // else, do some region removal stuff.
    // first, determine which region we are colligin from, and iterate until we collide from absolute right or end of collision

    USN_Extent b = iter->base[iter->base_indc];
    USN_Extent e = iter->to_be_excluded[iter->excl_indc];

    uint32_t eoe = e.off + e.len;
    uint32_t eob = b.off + b.len;

    for (;;) {

        // @NOTE : all of the collision-style checks are assuming that there is collision!. otherwise, this loop would be infinite loop.
        // and all checks would be filled with very long conditions, more branches etc.

        ASSERT(is_extents_collide(b, e));

        // iterate next base region, this one is shadowed out by excl extent.
        if (e.off <= b.off && eoe >= eob) {
            debug_overrun = 1;
            iter->base_indc++;

            if (iter->base_indc == iter->base.len) {
                result->len = -1;// to make defer above happy;
                return false;
            }

            b   = iter->base[iter->base_indc];
            eob = b.off + b.len;

            correctly_align_exclusion_extents(iter);
            // if new exclusion extent doesnt touch anything, just return it.
            if (iter->excl_indc < iter->to_be_excluded.len && is_extents_collide(b, iter->to_be_excluded[iter->excl_indc])) {
                // restart from start.
                e   = iter->to_be_excluded[iter->excl_indc];
                eoe = e.off + e.len;
                continue;
            }
            else {
                
                if (iter->excl_indc < iter->to_be_excluded.len) {
                    // @NOTE(batuhan): base and extent does not collide. do simple iterator

                    if (iter->to_be_excluded[iter->excl_indc].off > b.off) {
                        debug_overrun_c0 = 1;
                        iter->base_indc++;
                    }
                    else {
                        debug_overrun_c1 = 1;
                        iter->excl_indc++;
                    }

                }

                *result = b;
                return true;
            }

        }


        // remove collision region, iterate exclusion
        // from left
        else if (e.off <= b.off && eoe < eob) {
            debug_left = 1;
            // remove
            ASSERT(eob >= eoe);
            b.len = eob - eoe;
            b.off = eoe;
            // iterate excl
            
            iter->excl_indc++;

            // try to iterate next excl ext, if it is depleted, freely iterate base extents.
            if (iter->excl_indc < iter->to_be_excluded.len) {
                if (is_extents_collide(b, iter->to_be_excluded[iter->excl_indc])) {
                    debug_left_c0  = 1;
                    e = iter->to_be_excluded[iter->excl_indc];
                    eoe = e.off + e.len;
                }
                else {
                    debug_left_c1 = 1;
                    iter->base_indc++;
                    *result = b;
                    ASSERT(result->len);
                    return true;
                }
            }
            else {
                debug_left_c2 = 1;
                iter->base_indc++;
                *result = b;
                ASSERT(result->len);
                return true;
            }

        }


        // most annoying one, return left part of the collision, and edit base extent slice.
        // middle
        else if (e.off > b.off && eoe < eob) {
            debug_mid = 1;
            b.off = b.off;
            b.len = e.off - b.off;
            *result = b;

            // address sanitizer + custom range checker is going to catch this anyway, but let just be sure.
            ASSERT(iter->base_indc < iter->base.len);
            ASSERT(eob >= eoe);
            USN_Extent rr;
            rr.off = eoe;
            rr.len = eob - eoe;
            iter->base[iter->base_indc] = rr;
            iter->excl_indc++;
            ASSERT(result->len);

            return true;
        }


        // safe to return b, iterate base.
        // right
        else if (e.off > b.off && eoe >= eob) {
            debug_right = 1;
            b.off = b.off;
            b.len = e.off - b.off;
            *result = b;

            iter->base_indc++;

            ASSERT(result->len);
            return true;
        }


    }

    

}


int64_t 
extent_offset_to_file_offset(Array<USN_Extent> & extents, int64_t extent_offset) {
    int64_t result = 0;
    for_array (i, extents) {
        int64_t eoe = extents[i].off + extents[i].len;
        if (eoe >= extent_offset) {
            int64_t im = extent_offset - extents[i].off;
            result += im;
            break;
        }
        else {
            result += extents[i].len;
        }
    }
    return result;
}

void
sort_and_merge_extents(Array<USN_Extent> & arr) {
    sort_extents(arr);
    merge_extents(arr);
}


void
sort_extents(Array<USN_Extent> & arr) {
    qsort(arr.data, arr.len, sizeof(arr[0]), qs_comp_extent_fn);
}


void
merge_extents(Array<USN_Extent> & arr) {
    
    if (arr.len == 0) return;

    int64_t merged_record_id = 0;
    int64_t c_iter = 0;

    for (;;) {

        if ((uint64_t)c_iter >= arr.len) {
            break;
        }
        
        
        if (is_extents_collide(arr[merged_record_id], arr[c_iter])) {            
            int64_t ep1 = arr[c_iter].off           + arr[c_iter].len;
            int64_t ep2 = arr[merged_record_id].off + arr[merged_record_id].len;
            
            int64_t end_point = BG_MAX(ep1, ep2);
            ASSERT(end_point > arr[merged_record_id].off);
            ASSERT(end_point - arr[merged_record_id].off <= 0xffffffff);

            arr[merged_record_id].len = end_point - arr[merged_record_id].off;
            
            c_iter++;
        }
        else {
            merged_record_id++;
            arr[merged_record_id] = arr[c_iter];
        }
        
        
    }
    
    
    ASSERT(merged_record_id >= 0);
    ASSERT(merged_record_id <= 0xffffffffffff);

    ASSERT((uint64_t)merged_record_id + 1 <= arr.len);
    ASSERT((uint64_t)c_iter <= arr.len);


    arr.len = merged_record_id + 1;

}


bool
is_extents_collide(USN_Extent lhs, USN_Extent rhs) {
    
    auto lhsend = lhs.off + lhs.len;
    auto rhsend = rhs.off + rhs.len;
    
    if (lhs.off > rhsend || rhs.off > lhsend) {
        return false;
    }

    return true;
}


int32_t 
qs_comp_extent_fn(const void *p1, const void *p2) {
    
    USN_Extent lhs = *(USN_Extent*)p1;
    USN_Extent rhs = *(USN_Extent*)p2;

    auto lhsend = lhs.off + lhs.len;
    auto rhsend = rhs.off + rhs.len;
    if (lhs.off > rhs.off) {
        return +1;
    }
    if (rhs.off > lhs.off) {
        return -1;
    }
    if (rhs.off == lhs.off) {
        if (rhsend < lhsend) {
            return -1;
        }
        return +1;
    }

    if (rhs.off == lhs.off && lhs.len == rhs.len) {
        return 0;
    }

    ASSERT(false);
    return 0;
}
