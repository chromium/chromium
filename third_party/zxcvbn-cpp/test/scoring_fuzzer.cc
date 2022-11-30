// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/zxcvbn-cpp/native-src/zxcvbn/scoring.hpp"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/strings/string_util.h"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/matching.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string password(reinterpret_cast<const char*>(data), size);
  if (!base::IsStringUTF8(password))
    return 0;

  auto matches = zxcvbn::omnimatch(password);
  zxcvbn::most_guessable_match_sequence(password, matches);

  if (!matches.empty())
    zxcvbn::estimate_guesses(matches[0], password);

  return 0;
}
