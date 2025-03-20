// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "zst_decompressor.h"

#include <iostream>

#include "third_party/zstd/src/lib/zstd.h"

ZstDecompressor::ZstDecompressor(std::istream& input_stream)
    : _input_stream(input_stream) {
  _input_buffer_size = ZSTD_DStreamInSize();
  _input_buffer = new char[_input_buffer_size];
  _input_struct = new ZSTD_inBuffer{/* src = */ _input_buffer,
                                    /* size = */ 0,
                                    /* pos = */ 0};
  _output_buffer_size = ZSTD_DStreamOutSize();
  _output_buffer = new char[_output_buffer_size];
  _output_struct = new ZSTD_outBuffer{/* dst = */ _output_buffer,
                                      /* size = */ _output_buffer_size,
                                      /* pos = */ 0};
  _ctx = ZSTD_createDCtx();
  if (_ctx == nullptr) {
    std::cerr << "ZSTD_createDCtx failed!" << std::endl;
    exit(1);
  }
  _last_return_value = 0;
}

ZstDecompressor::~ZstDecompressor() {
  delete[] _input_buffer;
  delete _input_struct;
  delete[] _output_buffer;
  delete _output_struct;
  ZSTD_freeDCtx(_ctx);
}

bool ZstDecompressor::DecompressStreaming(DecompressedContent* output) {
  // If we have decompressed everything that we have read last time, we should
  // read a new chunk of the input_stream.
  if (_input_struct->pos >= _input_struct->size) {
    _input_stream.read(_input_buffer, _input_buffer_size);
    if (_input_stream.fail() && !_input_stream.eof()) {
      std::cerr << "ZST decompressor failed to read from input_stream."
                << std::endl;
      exit(1);
    }
    size_t num_bytes_read = _input_stream.gcount();
    if (num_bytes_read == 0) {
      if (_last_return_value != 0) {
        std::cerr << "ZST decompressor has processed the entirety of "
                     "input_stream but ZSTD says the decoding process is not "
                     "done. This likely means the input_stream is malformed."
                  << std::endl;
        exit(1);
      }
      return true;
    }
    _input_struct->size = num_bytes_read;
    _input_struct->pos = 0;
  }

  // Now that there is some content in the input buffer that we have not
  // decompressed, we pass the input buffer to the zstd library function and ask
  // it to do the decompression work.
  _output_struct->pos = 0;
  _last_return_value =
      ZSTD_decompressStream(_ctx, _output_struct, _input_struct);
  if (ZSTD_isError(_last_return_value)) {
    std::cerr << "ZSTD_decompressStream returned an error: "
              << ZSTD_getErrorName(_last_return_value) << std::endl;
    exit(1);
  }
  output->buffer = _output_buffer;
  output->size = _output_struct->pos;
  return false;
}
