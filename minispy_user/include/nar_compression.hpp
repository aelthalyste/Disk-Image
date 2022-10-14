#pragma once

#include <stdint.h>
#include "zstd.h"

#if MANAGED
public
#endif
enum class NAR_COMPRESSION_TYPE : uint8_t {
    none,
    lz4,
    zstd,
    INVALID,
};


/*
    @NOTE : 13.08.2022 allright, here is the deal.
    for some reason compiler raises an INTERNAL error if we use NAR_COMPRESSION_TYPE enum as a
    managed type (like compiling in clr mode) in a struct and initialize this struct with default constuctor {}
    ex:

    struct foo {
        NAR_COMPRESSION_TYPE _t;
        int a;
    };

    foo bar = {};// raises an internal error.

    So this is why we have constants below, because we cant compile this unit in clr mode, so i have to retype all of this for
    nar_compression_ctx.
*/
#if 0
constexpr uint8_t NAR_COMPRESSION_TYPE_NONE = 0;
constexpr uint8_t NAR_COMPRESSION_TYPE_ZSTD = 1;
constexpr uint8_t NAR_COMPRESSION_TYPE_LZ4  = 2;

typedef uint8_t Compression_Type;
#endif

struct Nar_Compression_Ctx {

    
    uint64_t    bytes_compressed  ;
    uint64_t    bytes_feed        ;
    double      compression_ratio ;
    
    NAR_COMPRESSION_TYPE _type;

    union {
        // zstd stuff
        struct {
            ZSTD_CCtx *zstd_ctx;
            int32_t  thread_count;
            int32_t  job_size;
            int32_t  compression_level;
            size_t ret;
        };

        // lz4
        struct {
            // @TODO : implement
            int32_t __internal;
        };
    };
};


[[nodiscard]] uint64_t nar_compress_ctx(Nar_Compression_Ctx *context, const void *src, void *dst, uint64_t src_size, uint64_t dst_size);

[[nodiscard]] uint64_t nar_compress_zstd(const void* src, void* dst, uint64_t src_size, uint64_t dst_size);
[[nodiscard]] uint64_t nar_decompress_zstd(const void* src, void* dst, uint64_t src_size, uint64_t dst_size);

[[nodiscard]] uint64_t nar_compress_lz4(const void* src, void* dst, uint64_t src_size, uint64_t dst_size);
[[nodiscard]] uint64_t nar_decompress_lz4(const void* src, void* dst, uint64_t src_size, uint64_t dst_size);

[[nodiscard]] uint64_t compress(NAR_COMPRESSION_TYPE type, const void* input, void* output, uint64_t input_size, uint64_t output_cap);
[[nodiscard]] uint64_t decompress(NAR_COMPRESSION_TYPE type, const void* input, void* output, uint64_t input_size, uint64_t output_cap);


[[nodiscard]] Nar_Compression_Ctx create_compression_context(NAR_COMPRESSION_TYPE type);
[[nodiscard]] Nar_Compression_Ctx create_zstd_compression_context();
[[nodiscard]] Nar_Compression_Ctx create_lz4_compression_context();
              void                free_compression_context(Nar_Compression_Ctx* ctx);

