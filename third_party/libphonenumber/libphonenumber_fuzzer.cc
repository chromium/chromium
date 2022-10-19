// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include "third_party/libphonenumber/phonenumber_api.h"

using ::i18n::phonenumbers::PhoneNumber;
using ::i18n::phonenumbers::PhoneNumberUtil;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);

  PhoneNumber parsed_number;
  PhoneNumberUtil* phone_number_util = PhoneNumberUtil::GetInstance();
  phone_number_util->Parse(input, "US", &parsed_number);

  return 0;
}