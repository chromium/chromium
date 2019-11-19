// Copyright 2019 The Chromium project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is Chromium specific, to make crdtp_test.cc work.  It will work
// in the standalone (upstream) build, which uses mini_chromium as a dependency,
// as well as in Chromium. In other code bases (e.g. v8), a custom file with
// these two functions and appropriate
// includes may need to be provided, so it isn't necessarily part of a roll.

#include "test_platform.h"

#include <cstdint>
#include <string>
#include <vector>
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"

namespace crdtp {
std::string UTF16ToUTF8(span<uint16_t> in) {
  std::string out;
  bool success = base::UTF16ToUTF8(
      reinterpret_cast<const base::char16*>(in.data()), in.size(), &out);
  CHECK(success);
  return out;
}

std::vector<uint16_t> UTF8ToUTF16(span<uint8_t> in) {
  base::string16 tmp;
  bool success = base::UTF8ToUTF16(reinterpret_cast<const char*>(in.data()),
                                   in.size(), &tmp);
  CHECK(success);
  return std::vector<uint16_t>(tmp.begin(), tmp.end());
}
}  // namespace crdtp
