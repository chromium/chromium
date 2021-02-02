/*
  zip_algorithm_xz.c      -- XZ (de)compression routines
  Bazed on zip_algorithm_deflate.c -- deflate (de)compression routines
  Copyright (C) 2017-2019 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <libzip@nih.at>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
  3. The names of the authors may not be used to endorse or promote
     products derived from this software without specific prior
     written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "zipint.h"

#include <limits.h>
#include <lzma.h>
#include <stdlib.h>
#include <zlib.h>

struct ctx {
    zip_error_t *error;
    bool compress;
    zip_uint32_t compression_flags;
    bool end_of_input;
    lzma_stream zstr;
    zip_uint16_t method;
};


static void *
allocate(bool compress, int compression_flags, zip_error_t *error, zip_uint16_t method) {
    struct ctx *ctx;

    if (compression_flags < 0) {
	zip_error_set(error, ZIP_ER_INVAL, 0);
	return NULL;
    }

    if ((ctx = (struct ctx *)malloc(sizeof(*ctx))) == NULL) {
	zip_error_set(error, ZIP_ER_MEMORY, 0);
	return NULL;
    }

    ctx->error = error;
    ctx->compress = compress;
    ctx->compression_flags = (zip_uint32_t)compression_flags;
    ctx->compression_flags |= LZMA_PRESET_EXTREME;
    ctx->end_of_input = false;
    memset(&ctx->zstr, 0, sizeof(ctx->zstr));
    ctx->method = method;
    return ctx;
}


static void *
compress_allocate(zip_uint16_t method, int compression_flags, zip_error_t *error) {
    return allocate(true, compression_flags, error, method);
}


static void *
decompress_allocate(zip_uint16_t method, int compression_flags, zip_error_t *error) {
    return allocate(false, compression_flags, error, method);
}


static void
deallocate(void *ud) {
    struct ctx *ctx = (struct ctx *)ud;
    free(ctx);
}


static zip_uint16_t
general_purpose_bit_flags(void *ud) {
    /* struct ctx *ctx = (struct ctx *)ud; */
    return 0;
}

static int
map_error(lzma_ret ret) {
    switch (ret) {
    case LZMA_UNSUPPORTED_CHECK:
	return ZIP_ER_COMPRESSED_DATA;

    case LZMA_MEM_ERROR:
	return ZIP_ER_MEMORY;

    case LZMA_OPTIONS_ERROR:
	return ZIP_ER_INVAL;

    default:
	return ZIP_ER_INTERNAL;
    }
}


static bool
start(void *ud) {
    struct ctx *ctx = (struct ctx *)ud;
    lzma_ret ret;

    lzma_options_lzma opt_lzma;
    lzma_lzma_preset(&opt_lzma, ctx->compression_flags);
    lzma_filter filters[] = {
	{.id = (ctx->method == ZIP_CM_LZMA ? LZMA_FILTER_LZMA1 : LZMA_FILTER_LZMA2), .options = &opt_lzma},
	{.id = LZMA_VLI_UNKNOWN, .options = NULL},
    };

    ctx->zstr.avail_in = 0;
    ctx->zstr.next_in = NULL;
    ctx->zstr.avail_out = 0;
    ctx->zstr.next_out = NULL;

    if (ctx->compress) {
	if (ctx->method == ZIP_CM_LZMA)
	    ret = lzma_alone_encoder(&ctx->zstr, filters[0].options);
	else
	    ret = lzma_stream_encoder(&ctx->zstr, filters, LZMA_CHECK_CRC64);
    }
    else {
	if (ctx->method == ZIP_CM_LZMA)
	    ret = lzma_alone_decoder(&ctx->zstr, UINT64_MAX);
	else
	    ret = lzma_stream_decoder(&ctx->zstr, UINT64_MAX, LZMA_CONCATENATED);
    }

    if (ret != LZMA_OK) {
	zip_error_set(ctx->error, map_error(ret), 0);
	return false;
    }

    return true;
}


static bool
end(void *ud) {
    struct ctx *ctx = (struct ctx *)ud;

    lzma_end(&ctx->zstr);
    return true;
}


static bool
input(void *ud, zip_uint8_t *data, zip_uint64_t length) {
    struct ctx *ctx = (struct ctx *)ud;

    if (length > UINT_MAX || ctx->zstr.avail_in > 0) {
	zip_error_set(ctx->error, ZIP_ER_INVAL, 0);
	return false;
    }

    ctx->zstr.avail_in = (uInt)length;
    ctx->zstr.next_in = (Bytef *)data;

    return true;
}


static void
end_of_input(void *ud) {
    struct ctx *ctx = (struct ctx *)ud;

    ctx->end_of_input = true;
}


static zip_compression_status_t
process(void *ud, zip_uint8_t *data, zip_uint64_t *length) {
    struct ctx *ctx = (struct ctx *)ud;
    lzma_ret ret;

    ctx->zstr.avail_out = (uInt)ZIP_MIN(UINT_MAX, *length);
    ctx->zstr.next_out = (Bytef *)data;

    ret = lzma_code(&ctx->zstr, ctx->end_of_input ? LZMA_FINISH : LZMA_RUN);
    *length = *length - ctx->zstr.avail_out;

    switch (ret) {
    case LZMA_OK:
	return ZIP_COMPRESSION_OK;

    case LZMA_STREAM_END:
	return ZIP_COMPRESSION_END;

    case LZMA_BUF_ERROR:
	if (ctx->zstr.avail_in == 0) {
	    return ZIP_COMPRESSION_NEED_DATA;
	}

	/* fallthrough */
    default:
	zip_error_set(ctx->error, map_error(ret), 0);
	return ZIP_COMPRESSION_ERROR;
    }
}

/* clang-format off */

zip_compression_algorithm_t zip_algorithm_xz_compress = {
    compress_allocate,
    deallocate,
    general_purpose_bit_flags,
    63,
    start,
    end,
    input,
    end_of_input,
    process
};


zip_compression_algorithm_t zip_algorithm_xz_decompress = {
    decompress_allocate,
    deallocate,
    general_purpose_bit_flags,
    63,
    start,
    end,
    input,
    end_of_input,
    process
};

/* clang-format on */
