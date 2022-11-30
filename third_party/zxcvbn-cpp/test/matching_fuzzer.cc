// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/zxcvbn-cpp/native-src/zxcvbn/matching.hpp"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/strings/string_util.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string password(reinterpret_cast<const char*>(data), size);
  if (!base::IsStringUTF8(password))
    return 0;

  zxcvbn::dictionary_match(password, {});
  zxcvbn::reverse_dictionary_match(password, {});
  zxcvbn::l33t_match(password, {}, {});
  zxcvbn::spatial_match(password, {});
  zxcvbn::repeat_match(password);
  zxcvbn::sequence_match(password);
  zxcvbn::regex_match(password, {});
  zxcvbn::date_match(password);
  zxcvbn::omnimatch(password);
  return 0;
}
