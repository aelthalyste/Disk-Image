/*++

Copyright (c) 1989-2002  Microsoft Corporation

Module Name:

    mspyKern.h

Abstract:
    Header file which contains the structures, type definitions,
    constants, global variables and function prototypes that are
    only visible within the kernel.

Environment:

    Kernel mode

--*/
#ifndef __MSPYKERN_H__
#define __MSPYKERN_H__

#pragma warning(push)
#pragma warning(disable:4820) 


#include <fltKernel.h>
#include <suppress.h>
#include "minispy.h"

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

#define NAR_LOOKASIDE_SIZE 1024LL*2LL
#define NAR_TAG 'RAN'


#ifndef _DEBUG
//#define DbgPrint(v)
#endif


//
//  Win8 define for support of NPFS/MSFS
//  Win7 define for support of new ECPs.
//  Vista define for including transaction support,
//  older ECPs
//

#define MINISPY_WIN8     (NTDDI_VERSION >= NTDDI_WIN8)
#define MINISPY_WIN7     (NTDDI_VERSION >= NTDDI_WIN7)
#define MINISPY_VISTA    (NTDDI_VERSION >= NTDDI_VISTA)
#define MINISPY_NOT_W2K  (OSVER(NTDDI_VERSION) > NTDDI_WIN2K)
//
//  Define callback types for Vista
//

#if MINISPY_VISTA

//
//  Dynamically imported Filter Mgr APIs
//

typedef NTSTATUS
(*PFLT_SET_TRANSACTION_CONTEXT)(
                                _In_ PFLT_INSTANCE Instance,
                                _In_ PKTRANSACTION Transaction,
                                _In_ FLT_SET_CONTEXT_OPERATION Operation,
                                _In_ PFLT_CONTEXT NewContext,
                                _Outptr_opt_ PFLT_CONTEXT* OldContext
                                );

typedef NTSTATUS
(*PFLT_GET_TRANSACTION_CONTEXT)(
                                _In_ PFLT_INSTANCE Instance,
                                _In_ PKTRANSACTION Transaction,
                                _Outptr_ PFLT_CONTEXT* Context
                                );

typedef NTSTATUS
(*PFLT_ENLIST_IN_TRANSACTION)(
                              _In_ PFLT_INSTANCE Instance,
                              _In_ PKTRANSACTION Transaction,
                              _In_ PFLT_CONTEXT TransactionContext,
                              _In_ NOTIFICATION_MASK NotificationMask
                              );

//
// Flags for the known ECPs
//

#define ECP_TYPE_FLAG_PREFETCH                   0x00000001

#if MINISPY_WIN7

#define ECP_TYPE_FLAG_OPLOCK_KEY                 0x00000002
#define ECP_TYPE_FLAG_NFS                        0x00000004
#define ECP_TYPE_FLAG_SRV                        0x00000008

#endif

#define ADDRESS_STRING_BUFFER_SIZE          64

//
//  Enumerate the ECPs MiniSpy supports
//

typedef enum _ECP_TYPE {
    
    EcpPrefetchOpen,
    EcpOplockKey,
    EcpNfsOpen,
    EcpSrvOpen,
    
    NumKnownEcps
        
} ECP_TYPE;

#endif

//---------------------------------------------------------------------------
//      Global variables
//---------------------------------------------------------------------------

typedef struct _nar_kernel_data {
    
    //
    //  The object that identifies this driver.
    //
    
    PDRIVER_OBJECT DriverObject;
    
    //
    //  The filter that results from a call to
    //  FltRegisterFilter.
    //
    
    PFLT_FILTER Filter;
    
    //
    //  Server port: user mode connects to this port
    //
    
    PFLT_PORT ServerPort;
    

    ULONG NameQueryMethod;

    //
    //  Client connection port: only one connection is allowed at a time.,
    //
    
    PFLT_PORT ClientPort;
    
    //NPAGED_LOOKASIDE_LIST LogAllocator;

    // If VolFileID == 0, means that entry is invalid and we can skip flushing it's log at filterunload routine
    struct volume_region_buffer {
        FAST_MUTEX FastMutex; // used to provide exclusive access to MemoryBuffer
        UNICODE_STRING GUIDStrVol; //24 byte
        
        // GUIDStrVol.Buffer is equal to this struct, do not directly call this.
        char Reserved[NAR_GUID_STR_SIZE];
        
        
        // First 4 byte used to indicate used size, first 4 bytes included as used, so memorybuffers max usable size is NAR_MEMORYBUFFER_SIZE - sizeof*(INT32)
        // Do not directly call this to push data, instead use NAR_PUSH_MB macro
        void *MemoryBuffer;

        char Letter;
        int VolFileID; // usually (Letter - 'A' + 1), 0 means invalid

    } VolumeRegionBuffer[NAR_MAX_VOLUME_COUNT];

    HANDLE* FileHandles;

    volatile int IsShutdownInitiated;

    HANDLE MetadataHandle;
    ULONG UserModePID;
    int OsDeviceID;
} nar_data;


//
//  Defines the minispy context structure
//

typedef struct _MINISPY_TRANSACTION_CONTEXT {
    ULONG Flags;
    ULONG Count;
    
}MINISPY_TRANSACTION_CONTEXT, * PMINISPY_TRANSACTION_CONTEXT;

//
//  This macro below is used to set the flags field in minispy's
//  MINISPY_TRANSACTION_CONTEXT structure once it has been
//  successfully enlisted in the transaction.
//

#define MINISPY_ENLISTED_IN_TRANSACTION 0x01

//
//  Minispy's global variables
//

extern nar_data NarData;


#define DEFAULT_NAME_QUERY_METHOD           FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP
#define NAME_QUERY_METHOD                   L"NameQueryMethod"

//  Registration structure
//---------------------------------------------------------------------------

extern const FLT_REGISTRATION FilterRegistration;

//---------------------------------------------------------------------------
//  Function prototypes
//---------------------------------------------------------------------------

FLT_PREOP_CALLBACK_STATUS
SpyPreOperationCallback(
                        _Inout_ PFLT_CALLBACK_DATA Data,
                        _In_ PCFLT_RELATED_OBJECTS FltObjects,
                        _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
                        );

FLT_POSTOP_CALLBACK_STATUS
SpyPostOperationCallback(
                         _Inout_ PFLT_CALLBACK_DATA Data,
                         _In_ PCFLT_RELATED_OBJECTS FltObjects,
                         _In_ PVOID CompletionContext,
                         _In_ FLT_POST_OPERATION_FLAGS Flags
                         );


NTSTATUS
SpyFilterUnload(
                _In_ FLT_FILTER_UNLOAD_FLAGS Flags
                );

NTSTATUS
SpyQueryTeardown(
                 _In_ PCFLT_RELATED_OBJECTS FltObjects,
                 _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
                 );

NTSTATUS
SpyTeardownStart(
                 _In_ PCFLT_RELATED_OBJECTS FltObjects,
                 _In_ FLT_INSTANCE_TEARDOWN_FLAGS Reason
                 );

void
SpyTeardownComplete(
                    _In_ PCFLT_RELATED_OBJECTS FltObjects,
                    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Reason
                    );



NTSTATUS
SpyVolumeInstanceSetup(
                       _In_ PCFLT_RELATED_OBJECTS FltObjects,
                       _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
                       _In_ DEVICE_TYPE VolumeDeviceType,
                       _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType);


LONG
SpyExceptionFilter(
                   _In_ PEXCEPTION_POINTERS ExceptionPointer,
                   _In_ BOOLEAN AccessingUserBuffer
                   );

//---------------------------------------------------------------------------
//  Memory allocation routines
//---------------------------------------------------------------------------



#endif  //__MSPYKERN_H__

