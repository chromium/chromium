// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libphonenumber/phonenumber_brotli.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "third_party/brotli/include/brotli/decode.h"

namespace i18n::phonenumbers {

MetadataBytes PhonenumberBrotliDecompress(const unsigned char* compressed,
                                          size_t compressed_size,
                                          int uncompressed_size) {
  const size_t expected = static_cast<size_t>(uncompressed_size);
  auto out = std::make_unique<uint8_t[]>(expected);
  size_t decoded_size = expected;
  const BrotliDecoderResult result = BrotliDecoderDecompress(
      compressed_size, compressed, &decoded_size, out.get());
  // The compressed blob is generated from trusted build inputs, so these
  // should always hold in practice.
  CHECK_EQ(result, BROTLI_DECODER_RESULT_SUCCESS);
  CHECK_EQ(decoded_size, expected);
  return MetadataBytes(std::move(out), uncompressed_size);
}

}  // namespace i18n::phonenumbers
