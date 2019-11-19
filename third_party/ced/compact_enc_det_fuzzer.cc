// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <vector>

#include "third_party/ced/src/compact_enc_det/compact_enc_det.h"

namespace {
constexpr size_t kMaxInputSize = 64 * 1024;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Early out if there isn't enough data to extract options and pass data to
  // the library.
  if (size < 3 * sizeof(int32_t) + 1)
    return 0;

  // Limit the input size to avoid timing out on clusterfuzz.
  if (size > kMaxInputSize)
    return 0;

  FuzzedDataProvider data_provider(data, size);

  CompactEncDet::TextCorpusType corpus =
      static_cast<CompactEncDet::TextCorpusType>(
          data_provider.ConsumeIntegralInRange<int32_t>(
              0, CompactEncDet::NUM_CORPA));
  Encoding encoding_hint = static_cast<Encoding>(
      data_provider.ConsumeIntegralInRange<int32_t>(0, NUM_ENCODINGS));
  Language langauge_hint = static_cast<Language>(
      data_provider.ConsumeIntegralInRange<int32_t>(0, NUM_LANGUAGES));
  bool ignore_7bit_mail_encodings = data_provider.ConsumeBool();

  std::vector<char> text = data_provider.ConsumeRemainingBytes<char>();
  int bytes_consumed = 0;
  bool is_reliable = false;
  CompactEncDet::DetectEncoding(
      text.data(), text.size(), nullptr /* url_hint */,
      nullptr /* http_charset_hint */, nullptr /* meta_charset_hint */,
      encoding_hint, langauge_hint, corpus, ignore_7bit_mail_encodings,
      &bytes_consumed, &is_reliable);

  return 0;
}
