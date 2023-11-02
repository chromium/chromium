// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "third_party/libaddressinput/src/cpp/src/address_field_util.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);
  std::vector<i18n::addressinput::AddressField> address_field;
  i18n::addressinput::ParseAddressFieldsRequired(input, &address_field);
  return 0;
}
