// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "zst_decompressor.h"

#include <iostream>

#include "third_party/zstd/src/lib/zstd.h"

ZstDecompressor::ZstDecompressor(std::istream& input_stream)
    : input_stream_(input_stream) {
  input_buffer_size_ = ZSTD_DStreamInSize();
  input_buffer_ = new char[input_buffer_size_];
  input_struct_ = new ZSTD_inBuffer{/* src = */ input_buffer_,
                                    /* size = */ 0,
                                    /* pos = */ 0};
  output_buffer_size_ = ZSTD_DStreamOutSize();
  output_buffer_ = new char[output_buffer_size_];
  output_struct_ = new ZSTD_outBuffer{/* dst = */ output_buffer_,
                                      /* size = */ output_buffer_size_,
                                      /* pos = */ 0};
  ctx_ = ZSTD_createDCtx();
  if (ctx_ == nullptr) {
    std::cerr << "ZSTD_createDCtx failed!" << std::endl;
    exit(1);
  }
  last_return_value_ = 0;
}

ZstDecompressor::~ZstDecompressor() {
  delete[] input_buffer_;
  delete input_struct_;
  delete[] output_buffer_;
  delete output_struct_;
  ZSTD_freeDCtx(ctx_);
}

bool ZstDecompressor::DecompressStreaming(DecompressedContent* output) {
  // If we have decompressed everything that we have read last time, we should
  // read a new chunk of the input_stream.
  if (input_struct_->pos >= input_struct_->size) {
    input_stream_.read(input_buffer_, input_buffer_size_);
    if (input_stream_.fail() && !input_stream_.eof()) {
      std::cerr << "ZST decompressor failed to read from input_stream."
                << std::endl;
      exit(1);
    }
    size_t num_bytes_read = input_stream_.gcount();
    if (num_bytes_read == 0) {
      if (last_return_value_ != 0) {
        std::cerr << "ZST decompressor has processed the entirety of "
                     "input_stream but ZSTD says the decoding process is not "
                     "done. This likely means the input_stream is malformed."
                  << std::endl;
        exit(1);
      }
      return true;
    }
    input_struct_->size = num_bytes_read;
    input_struct_->pos = 0;
  }

  // Now that there is some content in the input buffer that we have not
  // decompressed, we pass the input buffer to the zstd library function and ask
  // it to do the decompression work.
  output_struct_->pos = 0;
  last_return_value_ =
      ZSTD_decompressStream(ctx_, output_struct_, input_struct_);
  if (ZSTD_isError(last_return_value_)) {
    std::cerr << "ZSTD_decompressStream returned an error: "
              << ZSTD_getErrorName(last_return_value_) << std::endl;
    exit(1);
  }
  output->buffer = output_buffer_;
  output->size = output_struct_->pos;
  return false;
}
