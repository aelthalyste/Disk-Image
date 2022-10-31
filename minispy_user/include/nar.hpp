#pragma once


#ifdef  __linux__
#define NAR_LINUX   1
#elif   _WIN32
#define NAR_WINDOWS 1
#endif

#if NAR_WINDOWS
// #include <windows.h>
// #include "../inc/minispy.h"
#else


struct nar_backup_id{
    union{
        unsigned long long Q;
        struct{
            unsigned short Year;
            unsigned char Month;
            unsigned char Day;
            unsigned char Hour;
            unsigned char Min;
            char Letter;
        };
    };
};
#endif


#define ASSERT(exp) BG_ASSERT(exp)
#define NAR_BREAK   BG_DEBUGBREAK




#define NAR_METADATA_EXTENSION  ".nbfsm"
#define NAR_BINARY_EXTENSION    ".nbfsf"

#define Kilobyte(val) ((val)*1024ll)
#define Megabyte(val) (Kilobyte(val)*1024ll)
#define Gigabyte(val) (Megabyte(val)*1024ll)


#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define MAX(a,b) ((a)>(b) ? (a) : (b))

#define NAR_COMPRESSION_FRAME_SIZE (1024ull*1024ull*16ull)

#define NAR_INVALID_VOLUME_TRACK_ID (-1)
#define NAR_INVALID_DISK_ID (-1)
#define NAR_INVALID_ENTRY (-1)

#define NAR_DISKTYPE_GPT 'G'
#define NAR_DISKTYPE_MBR 'M'
#define NAR_DISKTYPE_RAW 'R'

#define NAR_FULLBACKUP_VERSION     (0)
#define NAR_INVALID_BACKUP_VERSION (-2)

#define NAR_EFI_PARTITION_LETTER 'S'
#define NAR_RECOVERY_PARTITION_LETTER 'R'


#include "precompiled.h"
#include "memory.hpp"
#include "narstring.hpp"
#include "memory.hpp"
#include "package.hpp"
#include "bg.hpp"



#define printf(fmt, ...) do{BG_INTERNAL_LOG("INFO"   , fmt, ## __VA_ARGS__);} while (0);



enum BackupType {
    Diff,
    Inc
};

typedef int32_t compression_type;
const compression_type NAR_NO_VERSION_FILTER = (1024 * 1024 * 1); // 1million version is sure big enough.
const compression_type NAR_COMPRESSION_NONE = 0;
const compression_type NAR_COMPRESSION_LZ4  = 1;
const compression_type NAR_COMPRESSION_ZSTD = 2;

static inline bool NarIsCompressionTypeSupported(compression_type v) {
    ASSERT(v != NAR_COMPRESSION_LZ4); // @NOTE : i am not sure whether we shoul support lz4 or not. its very experimental at this stage and i dont want anything to backed up with this assumption
    if (v == NAR_COMPRESSION_NONE ||  
        v == NAR_COMPRESSION_ZSTD)
        return true;
    return false;
}

#define NAR_BINARY_MAGIC_NUMBER    'NARB'
#define NAR_BINARY_IDENTIFIER_SIZE (1024ll)
#define NAR_COMPRESSION_HEADER_RESERVED_SIZE (1024ll)

#pragma pack(push ,1) // force 1 byte alignment
struct backup_binary_identifier {
    uint32_t               Magic;
    nar_backup_id          BackupID;
    int32_t                Version;
    compression_type       CompressionType;
    char                   Letter; 
};
#pragma pack(pop)


#pragma pack(push ,1) // force 1 byte alignment
struct backup_information {
    uint64_t SizeOfBinaryData;
    uint64_t VersionMaxWriteOffset;

    uint64_t OriginalSizeOfVolume;

    uint64_t EFIPartitionSize;       // gpt + os volume 
    uint64_t SystemPartitionSize;    // mbr + os volume
    uint64_t RecoveryPartitionSize;  // gpt + os volume? (probably, gotta look spec)    

    uint32_t ClusterSize;

    int32_t Version;
    BackupType BT;

    int8_t Letter;
    int8_t DiskType;

    uint8_t  IsOSVolume;

    uint8_t          IsBinaryDataCompressed;
    compression_type CompressionType;

    nar_backup_id BackupID;
    Bg_Date MetadataTimeStamp;
};
#pragma pack(pop)


struct backup_information_ex : backup_information {
    UTF8 *Path;
};




struct backup_package {
    backup_information BackupInformation;
    Package_Reader     Package;
    UTF8 *Path;
};

struct packages_for_restore {
    backup_package *Packages;
    int32_t Count;
};


struct nar_fs_table_header {
    uint64_t data_size;
    int32_t  DataCompressionType;
};

// 2 bytes for the size of the actual name, then the name itself, 
// WITHOUT null termination
struct nar_name_table : nar_fs_table_header {
    uint8_t *names;
};

struct nar_layout_table : nar_fs_table_header {
    uint8_t *layouts;
};

struct nar_file_table : nar_fs_table_header {
    uint32_t *ids;
};


struct nar_kernel_record {
    uint32_t StartPos;
    uint32_t Len;
};

union nar_region {
    struct {
        int64_t off;
        int64_t len;
    };
    struct {
        int64_t CompressedSize;
        int64_t DecompressedSize;
    };  
};

typedef nar_region USN_Extent;

#if 0
union nar_record {
    struct{
        uint32_t StartPos;
        uint32_t Len;
    };
    struct{
        uint32_t CompressedSize;
        uint32_t DecompressedSize;
    };
    struct {
        uint32_t off;
        uint32_t len;
    };
};

typedef nar_record USN_Extent;
#endif


struct RegionCoupleIter{
    const nar_region *R1;
    const nar_region *R2;
    
    size_t     R1Len;
    size_t     R2Len;
    
    const nar_region *R1End;
    const nar_region *R2End;
    
    const nar_region *R1Iter;
    const nar_region *R2Iter;
    
    nar_region __CompRegion;
    nar_region It;
};


struct point_offset{
    int64_t  Offset;
    int64_t  Readable; // remaining region length
    int64_t  Indice;        // region indice we just found
};






static inline void
NarBackupIDToStr(nar_backup_id ID, char *Res, int ResMax);


#if 0
static inline uint32_t
GenerateBinaryFileNameUTF8(nar_backup_id ID, int32_t Version, NarUTF8 *Out){
    
    if(Out == NULL){
        return 27;
    }
    uint32_t Result = 0;
    
    char Bf[1024];
    char VersionBf[32];
    if(Version == NAR_FULLBACKUP_VERSION){
        snprintf(VersionBf, sizeof(VersionBf), "%s", "FULL");
    }
    else{
        snprintf(VersionBf, sizeof(VersionBf), "%d", Version);
    }
    
    // 27
    int ChWritten = snprintf(Bf, sizeof(Bf), "NB_%s-%c%02d%02d%02d%02d.nbfsf", VersionBf, ID.Letter, ID.Month, ID.Day, ID.Hour, ID.Min);
    if(ChWritten <= (int)Out->Cap){
        memset(Out->Str, 0, Out->Cap);
        memcpy(Out->Str, Bf, ChWritten);
        Out->Len = ChWritten;
        Result = Out->Len;
    }
    
    return Result;
}
#endif




static inline void 
NarBackupIDToStr(nar_backup_id ID, char *Res, int MaxRes){
    snprintf(Res, MaxRes, "%c%02d%02d%02d%02d", ID.Letter, ID.Month, ID.Day, ID.Hour, ID.Min);
}




bool
NarIsRegionIterValid(RegionCoupleIter Iter);


bool
__NarIsRegionIterExpired(RegionCoupleIter Iter);



inline RegionCoupleIter
NarInitRegionCoupleIter(const nar_region *Base, const nar_region *Ex, size_t BaseN, size_t ExN);

inline bool
NarIterateRegionCoupleUntilCollision(RegionCoupleIter *Iter);


inline void
NarNextExcludeIter(RegionCoupleIter *Iter);

RegionCoupleIter
NarStartExcludeIter(const nar_region *Base, const nar_region *Ex, size_t BaseN, size_t ExN);


RegionCoupleIter
NarStartIntersectionIter(const  nar_region *R1, const nar_region *R2, size_t R1Len, size_t R2Len);

void
NarNextIntersectionIter(RegionCoupleIter *Iter);

void
NarGetPreviousBackupInfo(int32_t Version, BackupType Type, int32_t *OutVersion);

int64_t
NarLCNToVCN(nar_region *LCN, int64_t LCNCount, int64_t Offset);


void NarConvertTo8ByteRecords(nar_kernel_record *R1, nar_region *Out, int64_t Count, int64_t ClusterSize);

/*
returns new size
*/
int64_t MergeRegions(nar_region* R, int64_t Count);

bool IsRegionsCollide(nar_region R1, nar_region R2);


// input MUST be sorted
// Finds point Offset in relative to Records structure, useful when converting absolue volume offsets to our binary backup data offsets.
// returns NAR_POINT_OFFSET_FAILED if fails to find given offset, 
point_offset 
FindPointOffsetInRecords(nar_region *Records, int64_t Len, int64_t Offset);


int
CompareNarRecords(const void* v1, const void* v2);


static inline int8_t
IsNumeric(char val) {
    return val >= '0' && val <= '9';
}



static inline bool
NarIsFullOnlyBackup(nar_backup_id ID){
    return (ID.Q & (1<<15));
}


static inline nar_backup_id
NarSetAsFullOnlyBackup(nar_backup_id ID){
    nar_backup_id Result = ID;
    Result.Q |= (1<<15);
    return Result;
}


int32_t NarGetBackupsInDirectoryWithFilter(const UTF8 *Directory, backup_package *output, int MaxCount, nar_backup_id *FilteredID, int32_t MaxVersion);
int32_t NarGetBackupsInDirectory(const UTF8 *Directory, backup_package *output, int MaxCount);
void    NarFreeBackupPackages(backup_package *packages, int32_t Count);

UTF8 **GetFilesInDirectoryWithExtension(const UTF8 *DirectoryAsUTF8, uint64_t *OutCount, UTF8 *Extension);
UTF8 **GetFilesInDirectory(const UTF8 *Directory, uint64_t *OutCount);
void FreeDirectoryList(UTF8 **List, uint64_t Count);




packages_for_restore LoadPackagesForRestore(const UTF8 *Directory, nar_backup_id BackupID, int32_t Version);
void FreePackagesForRestore(packages_for_restore *Packages);
bool BG_API AdvanceRestore(struct Restore_Ctx *ctx, struct Restore_Instruction *instruction);
bool BG_API InitRestore(struct Restore_Ctx *output, const UTF8 *DirectoryToLook, nar_backup_id BackupID, int32_t Version);
void BG_API FreeRestoreCtx(struct Restore_Ctx *ctx);
bool NarCompareBackupID(nar_backup_id id1, nar_backup_id id2);



backup_package *GetLatestPackage(packages_for_restore *Packages);
backup_package *GetPreviousPackage(packages_for_restore *Packages, backup_package *Current);



BG_API Array<Array<backup_information_ex>> NarGetChainsInDirectory(const UTF8 *Directory); 
BG_API void                                NarFreeChains(Array<Array<backup_information_ex>> & Chains);



struct nar_binary_files;
BG_API nar_binary_files*    NarGetBinaryFilesInDirectory(const UTF8 *Directory, nar_backup_id BackupID, int32_t Version);
BG_API void                 NarFreeBinaryFilesInDirectory(nar_binary_files *Files);
BG_API bool                 NarReadVersion(nar_binary_files *Files, int32_t Version, void *Data, uint64_t Offset, uint64_t Size);
bool NarValidateBinaryIdentifier(backup_binary_identifier *Output, void *Bf); 


#if 0

nar_time {
    uint8_t year   : 5;
    uint8_t month  : 4;
    uint8_t day    : 5;
    uint8_t hour   : 5;
    uint8_t minute : 6;
    uint8_t second : 6;
};

typedef uint32_t layout_id;
typedef uint32_t file_id;

struct file_record {
    uint64_t FileSize;
    uint32_t CreationTime;
    uint32_t ModifiedTime;
    uint32_t FileNameSlot;
    file_id ID;
    file_id ParentID;
    layout_id Layout;
};

struct file_table {

};

300
n_of_files * ( bytes) + 6mb per million file;
#endif



// USN_EXTENT CODEBASE START


struct Extent_Exclusion_Iterator {
    Slice<USN_Extent> base;
    Slice<USN_Extent> to_be_excluded;
    uint64_t base_indc;
    uint64_t excl_indc;
};

enum Instruction_Type {
    INVALID,
    NORMAL,
    ZERO
};

// Restore structs 
struct Restore_Instruction {
    uint8_t instruction_type;

    USN_Extent where_to_read;

    // @TODO : replace this with uint64_t write_size. All writes must be sequential.
    USN_Extent where_to_write;

    int32_t     version;
};


struct Restore_Ctx {
    Array<Restore_Instruction>  instructions;
    u64 target_file_size;
    u64 cii = 0;
};


// functions!


Extent_Exclusion_Iterator init_exclusion_iterator(Array<USN_Extent> base, Array<USN_Extent> to_be_excluded);

void free_exclusion_iterator(Extent_Exclusion_Iterator *iter);

int32_t qs_comp_restore_inst_fn(const void *p1, const void *p2);
void    sort_restore_instructions(Array<Restore_Instruction> & arr);

void    correctly_align_exclusion_extents(Extent_Exclusion_Iterator *iter);
bool    iterate_next_extent(USN_Extent *result, Extent_Exclusion_Iterator *iter);

bool nar_debug_check_if_we_touched_all_extent_scenarios(void);
void nar_debug_reset_internal_branch_states(void);
void nar_debug_reset_touch_states(void);


int64_t  extent_offset_to_file_offset(Array<USN_Extent> & extents, int64_t extent_offset);
void sort_and_merge_extents(Array<USN_Extent> & arr);

void sort_extents(Array<USN_Extent> & arr);
void merge_extents(Array<USN_Extent> & arr);

bool     is_extents_collide(USN_Extent lhs, USN_Extent rhs);
int32_t  qs_comp_extent_fn(const void *p1, const void *p2);

