// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "zst_compressor.h"

#include <iostream>

#include "third_party/zstd/src/lib/zstd.h"

ZstCompressor::ZstCompressor(std::ostream& output_stream, int compression_level)
    : output_stream_(output_stream) {
  input_buffer_recommended_size_ = ZSTD_CStreamInSize();
  input_struct_ = new ZSTD_inBuffer{/* src = */ nullptr,
                                    /* size = */ 0,
                                    /* pos = */ 0};
  output_buffer_size_ = ZSTD_CStreamOutSize();
  output_buffer_ = new char[output_buffer_size_];
  output_struct_ = new ZSTD_outBuffer{/* dst = */ output_buffer_,
                                      /* size = */ output_buffer_size_,
                                      /* pos = */ 0};
  ctx_ = ZSTD_createCCtx();
  if (ctx_ == nullptr) {
    std::cerr << "ZSTD_createCCtx failed!" << std::endl;
    exit(1);
  }
  size_t ret =
      ZSTD_CCtx_setParameter(ctx_, ZSTD_c_compressionLevel, compression_level);
  if (ZSTD_isError(ret)) {
    std::cerr << "ZSTD_CCtx_setParameter returned an error: "
              << ZSTD_getErrorName(ret) << std::endl;
    exit(1);
  }
}

ZstCompressor::~ZstCompressor() {
  delete input_struct_;
  delete[] output_buffer_;
  delete output_struct_;
  ZSTD_freeCCtx(ctx_);
}

size_t ZstCompressor::GetRecommendedInputBufferSize() {
  return input_buffer_recommended_size_;
}

void ZstCompressor::CompressStreaming(UncompressedContent input,
                                      bool last_chunk) {
  input_struct_->src = input.buffer;
  input_struct_->size = input.size;
  input_struct_->pos = 0;
  ZSTD_EndDirective mode = last_chunk ? ZSTD_e_end : ZSTD_e_continue;

  // Repeatedly call ZSTD_compressStream2() until it has consumed all the input.
  bool finished = false;
  while (!finished) {
    output_struct_->pos = 0;
    size_t ret =
        ZSTD_compressStream2(ctx_, output_struct_, input_struct_, mode);
    if (ZSTD_isError(ret)) {
      std::cerr << "ZSTD_compressStream returned an error: "
                << ZSTD_getErrorName(ret) << std::endl;
      exit(1);
    }
    output_stream_.write(output_buffer_, output_struct_->pos);
    if (output_stream_.fail()) {
      std::cerr << "ZST compressor failed to write to output_stream."
                << std::endl;
      exit(1);
    }
    // If we are processing the last chunk, we are finished when zstd returns 0,
    // which means it has consumed all the input AND flushed all the output.
    // Otherwise, we are finished when it has consumed all the input.
    if (last_chunk) {
      finished = (ret == 0);
    } else {
      finished = (input_struct_->pos == input.size);
    }
  }
}
