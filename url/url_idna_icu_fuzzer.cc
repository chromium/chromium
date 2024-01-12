// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "third_party/icu/fuzzers/fuzzer_utils.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"

struct Environment {
  IcuEnvironment icu_environment;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  if (size < 1) {
    return 0;
  }

  std::u16string_view input(reinterpret_cast<const char16_t*>(data),
                            size / sizeof(const char16_t));
  url::RawCanonOutputW<1024> output;

  url::IDNToASCII(input, &output);
  return 0;
}
