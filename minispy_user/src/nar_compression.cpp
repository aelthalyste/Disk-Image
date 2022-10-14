#include "nar_compression.hpp"

#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"
#include "lz4.h"
#include <assert.h>

static constexpr int32_t NAR_ZSTD_JOB_SIZE          = 512 * 1024;
static constexpr int32_t NAR_ZSTD_COMPRESSION_LEVEL = 6;
static constexpr int32_t NAR_ZSTD_THREAD_COUNT      = 6; // @TODO : make this dynamic depending on # of cores
static ZSTD_threadPool *g_zstd_thread_pool = NULL;


Nar_Compression_Ctx create_compression_context(NAR_COMPRESSION_TYPE type) {
    switch (type) {
        case NAR_COMPRESSION_TYPE::zstd:    return create_zstd_compression_context();
        case NAR_COMPRESSION_TYPE::lz4 :    return create_lz4_compression_context();
        case NAR_COMPRESSION_TYPE::none:    return {};
        case NAR_COMPRESSION_TYPE::INVALID: return {};
    }
    //Nar_Compression_Ctx result{};
    return {};
}

Nar_Compression_Ctx create_lz4_compression_context() {
   Nar_Compression_Ctx result = {};
   result._type = NAR_COMPRESSION_TYPE::lz4;
   return result;
}

Nar_Compression_Ctx create_zstd_compression_context() {
    Nar_Compression_Ctx result = {};
    result._type = NAR_COMPRESSION_TYPE::zstd;
    result.thread_count      = NAR_ZSTD_THREAD_COUNT;
    result.job_size          = NAR_ZSTD_JOB_SIZE;
    result.compression_level = NAR_ZSTD_COMPRESSION_LEVEL;

    if (g_zstd_thread_pool == NULL)
        g_zstd_thread_pool = ZSTD_createThreadPool(NAR_ZSTD_THREAD_COUNT);

    result.zstd_ctx = ZSTD_createCCtx();
    assert(result.zstd_ctx);
    
    size_t ret = 0;

    // we dont really care if any of those calls fails.
    // ret = ZSTD_CCtx_refThreadPool(result.zstd_ctx, g_zstd_thread_pool);
    // assert(!ZSTD_isError(ret));

    ret = ZSTD_CCtx_setParameter(result.zstd_ctx, ZSTD_c_nbWorkers       , result.thread_count);
    assert(!ZSTD_isError(ret));
    
    ret = ZSTD_CCtx_setParameter(result.zstd_ctx, ZSTD_c_jobSize         , result.job_size);
    assert(!ZSTD_isError(ret));

    ret = ZSTD_CCtx_setParameter(result.zstd_ctx, ZSTD_c_compressionLevel, result.compression_level);
    assert(!ZSTD_isError(ret));

    return result;
}

void free_compression_context(Nar_Compression_Ctx *ctx) {
    if (ctx->_type == NAR_COMPRESSION_TYPE::zstd) {
        ZSTD_freeCCtx(ctx->zstd_ctx);
        ctx->zstd_ctx = NULL;
    }
    else if (ctx->_type == NAR_COMPRESSION_TYPE::lz4) {
        // do nothing. not implemented
    }
}


uint64_t nar_compress_ctx(Nar_Compression_Ctx *ctx, const void *src, void *dst, uint64_t src_size, uint64_t dst_size) {
    uint64_t result = 0;

    if (ctx->_type == NAR_COMPRESSION_TYPE::zstd) {  
        // init zstd buffers

        ZSTD_inBuffer z_in = {};
        z_in.size = src_size;
        z_in.src  = src;

        ZSTD_outBuffer z_out = {};
        z_out.size = dst_size;
        z_out.dst  = dst;
        ctx->ret = ZSTD_compressStream2(ctx->zstd_ctx, &z_out, &z_in, ZSTD_e_end);        
        if(!ZSTD_isError(ctx->ret) && z_in.pos == src_size)
            result = z_out.pos;
        
    }
    else if (ctx->_type == NAR_COMPRESSION_TYPE::lz4) {
        // @TODO implement;
        // ignore ctx and redirect to contextless compression for now
        result = nar_compress_lz4(src, dst, src_size, dst_size);
    }

    if (!result) {
        ctx->bytes_compressed += result;
        ctx->bytes_feed        = src_size;
        ctx->compression_ratio = (double)ctx->bytes_feed / (double)ctx->bytes_compressed;
    }

    return result;

}




uint64_t nar_compress_lz4(const void* src, void* dst, uint64_t src_size, uint64_t dst_size) {
    int rv = LZ4_compress_default((const char*)src, (char*)dst, (int)src_size, (int)dst_size);
    return rv > 0 ? rv : 0;
}

uint64_t nar_decompress_lz4(const void* src, void* dst, uint64_t src_size, uint64_t dst_size) {
    int rv = LZ4_decompress_safe((const char*)src, (char*)dst, (int)src_size, (int)dst_size);
    return rv > 0 ? rv : 0;
}

uint64_t compress(NAR_COMPRESSION_TYPE type, const void* input, void* output, uint64_t input_size, uint64_t output_cap) {
    uint64_t new_size = 0;
    if       (type == NAR_COMPRESSION_TYPE::lz4) new_size = nar_compress_lz4(input, output, input_size, output_cap);
    else if (type == NAR_COMPRESSION_TYPE::zstd) new_size = nar_compress_zstd(input, output, input_size, output_cap);
    return new_size;
};

uint64_t decompress(NAR_COMPRESSION_TYPE type, const void* input, void* output, uint64_t input_size, uint64_t output_cap) {
    uint64_t new_size = 0;
    if      (type == NAR_COMPRESSION_TYPE::lz4)  new_size = nar_decompress_lz4(input, output, input_size, output_cap);
    else if (type == NAR_COMPRESSION_TYPE::zstd) new_size = nar_decompress_zstd(input, output, input_size, output_cap);
    return new_size;
};



uint64_t nar_compress_zstd(const void* src, void* dst, uint64_t src_size, uint64_t dst_size) {
    size_t zstd_ret = ZSTD_compress(dst, dst_size, src, src_size, 4);
    return ZSTD_isError(zstd_ret) ? 0 : zstd_ret;
}

uint64_t nar_decompress_zstd(const void* src, void* dst, uint64_t src_size, uint64_t dst_size) {

    uint64_t result = 0;
    size_t zstd_ret = ZSTD_decompress(dst, dst_size, src, src_size);
    if (!ZSTD_isError(zstd_ret))
        result = zstd_ret;

    return result;
}

