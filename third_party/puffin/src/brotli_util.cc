// Copyright 2021 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/src/include/puffin/brotli_util.h"

#include "brotli/decode.h"
#include "brotli/encode.h"
#include "puffin/memory_stream.h"
#include "puffin/src/logging.h"

namespace puffin {

namespace {

constexpr auto kBufferSize = 32768;
constexpr auto kDefaultParamQuality = 9;
constexpr auto kDefaultParamLgwin = 20;
}  // namespace

bool BrotliEncode(const uint8_t* input,
                  size_t input_size,
                  UniqueStreamPtr output_stream,
                  int quality) {
  std::unique_ptr<BrotliEncoderState, decltype(&BrotliEncoderDestroyInstance)>
      encoder(BrotliEncoderCreateInstance(nullptr, nullptr, nullptr),
              BrotliEncoderDestroyInstance);
  TEST_AND_RETURN_FALSE(encoder != nullptr);

  BrotliEncoderSetParameter(encoder.get(), BROTLI_PARAM_QUALITY, quality);
  BrotliEncoderSetParameter(encoder.get(), BROTLI_PARAM_LGWIN,
                            kDefaultParamLgwin);

  size_t available_in = input_size;
  while (available_in != 0 || !BrotliEncoderIsFinished(encoder.get())) {
    const uint8_t* next_in = input + input_size - available_in;
    // Set up the output buffer
    uint8_t buffer[kBufferSize];
    uint8_t* next_out = buffer;
    size_t available_out = kBufferSize;

    BrotliEncoderOperation op =
        available_in == 0 ? BROTLI_OPERATION_FINISH : BROTLI_OPERATION_PROCESS;

    if (!BrotliEncoderCompressStream(encoder.get(), op, &available_in, &next_in,
                                     &available_out, &next_out, nullptr)) {
      return false;
    }

    size_t bytes_consumed = kBufferSize - available_out;
    output_stream->Write(buffer, bytes_consumed);
  }

  return true;
}

bool BrotliEncode(const uint8_t* input,
                  size_t input_size,
                  UniqueStreamPtr output_stream) {
  return BrotliEncode(input, input_size, std::move(output_stream),
                      kDefaultParamQuality);
}

bool BrotliEncode(const uint8_t* input,
                  size_t input_size,
                  std::vector<uint8_t>* output) {
  TEST_AND_RETURN_FALSE(output != nullptr);
  return BrotliEncode(input, input_size, MemoryStream::CreateForWrite(output));
}

bool BrotliDecode(const uint8_t* input,
                  size_t input_size,
                  UniqueStreamPtr output_stream) {
  std::unique_ptr<BrotliDecoderState, decltype(&BrotliDecoderDestroyInstance)>
      decoder(BrotliDecoderCreateInstance(nullptr, nullptr, nullptr),
              BrotliDecoderDestroyInstance);
  TEST_AND_RETURN_FALSE(decoder != nullptr);

  size_t available_in = input_size;
  while (available_in != 0 || !BrotliDecoderIsFinished(decoder.get())) {
    const uint8_t* next_in = input + input_size - available_in;
    // Set up the output buffer
    uint8_t buffer[kBufferSize] = {0};
    uint8_t* next_out = buffer;
    size_t available_out = kBufferSize;

    BrotliDecoderResult result =
        BrotliDecoderDecompressStream(decoder.get(), &available_in, &next_in,
                                      &available_out, &next_out, nullptr);
    if (result == BROTLI_DECODER_RESULT_ERROR ||
        result == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
      return false;
    }

    size_t bytes_consumed = kBufferSize - available_out;
    output_stream->Write(buffer, bytes_consumed);
  }
  return true;
}

bool BrotliDecode(const uint8_t* input,
                  size_t input_size,
                  std::vector<uint8_t>* output) {
  TEST_AND_RETURN_FALSE(output != nullptr);
  return BrotliDecode(input, input_size, MemoryStream::CreateForWrite(output));
}

}  // namespace puffin
