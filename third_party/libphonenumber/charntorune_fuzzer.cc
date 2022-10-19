// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include "third_party/libphonenumber/dist/cpp/src/phonenumbers/utf/utf.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  Rune rune;
  charntorune(&rune, reinterpret_cast<const char*>(data), size);
  return 0;
}