// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "url/url_canon_icu.h"
#include "url/url_canon_icu_test_helpers.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1) {
    return 0;
  }

  url::test::UConvScoper conv("utf-8");
  url::ICUCharsetConverter converter(conv.converter());
  url::RawCanonOutput<1024> output;

  converter.ConvertFromUTF16(reinterpret_cast<const char16_t*>(data),
                             size / sizeof(const char16_t), &output);
  return 0;
}
