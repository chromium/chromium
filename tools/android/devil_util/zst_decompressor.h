// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_ANDROID_DEVIL_UTIL_ZST_DECOMPRESSOR_H_
#define TOOLS_ANDROID_DEVIL_UTIL_ZST_DECOMPRESSOR_H_

#include <istream>

// Forward declarations.
struct ZSTD_inBuffer_s;
typedef ZSTD_inBuffer_s ZSTD_inBuffer;
struct ZSTD_outBuffer_s;
typedef ZSTD_outBuffer_s ZSTD_outBuffer;
struct ZSTD_DCtx_s;
typedef ZSTD_DCtx_s ZSTD_DCtx;

class ZstDecompressor {
 private:
  // The input stream where we are reading the compressed content from.
  std::istream& _input_stream;
  // A buffer containing a portion of the input_stream that we have read.
  char* _input_buffer;
  // The size of the contents stored in the input buffer.
  size_t _input_buffer_size;
  // A struct containing the input buffer, the size of the contents of input
  // buffer, and the position where the zstd library function stopped reading.
  // The position field is updated by the zstd library functions.
  ZSTD_inBuffer* _input_struct;
  // A buffer where the decompressed content is placed into. This is returned to
  // the caller of DecompressStreaming().
  char* _output_buffer;
  // The size of the output buffer. Indicates how large the buffer is.
  size_t _output_buffer_size;
  // A struct containing the output buffer, the size of the output buffer,
  // and the position where the zstd library function stopped writing.
  // The position field is updated by the zstd library functions.
  ZSTD_outBuffer* _output_struct;
  // A context object needed by the zstd library functions.
  ZSTD_DCtx* _ctx;
  // The return value of the last call to the zstd library functions.
  // Equals 0 when there is no more work to be done.
  size_t _last_return_value;

 public:
  // The user is responsible for closing the input_stream if necessary.
  explicit ZstDecompressor(std::istream& input_stream);
  ~ZstDecompressor();

  struct DecompressedContent {
    char* buffer;
    size_t size;
  };

  // Decompress the next portion of the input_stream. Repeatedly call this
  // function to decompress the entirety of input_stream.
  // After execution, this sets output->buffer to a buffer holding decompressed
  // content, and sets output->size to the byte count of decompressed content.
  // The user does not need to free the memory pointed to by the output->buffer.
  // A second call to this function overrides the buffer returned by the first
  // call. In other words, the user should finish using the buffer before making
  // a second call to this function.
  // Return true if the entirety of input_stream has been decompressed, and
  // false otherwise.
  bool DecompressStreaming(DecompressedContent* output);
};

#endif  // TOOLS_ANDROID_DEVIL_UTIL_ZST_DECOMPRESSOR_H_
