// Copyright 2019 The Chromium project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is Chromium specific, to make crdtp_test.cc work.  It will work
// in the standalone (upstream) build, which uses mini_chromium as a dependency,
// as well as in Chromium. In other code bases (e.g. v8), a custom file with
// these two functions and appropriate
// includes may need to be provided, so it isn't necessarily part of a roll.

#ifndef CRDTP_ENCODING_TEST_PLATFORM_H_
#define CRDTP_ENCODING_TEST_PLATFORM_H_

#include <cstdint>
#include <string>
#include <vector>
#include "base/logging.h"
#include "span.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crdtp {
std::string UTF16ToUTF8(span<uint16_t> in);
std::vector<uint16_t> UTF8ToUTF16(span<uint8_t> in);
}  // namespace crdtp

#endif  // CRDTP_ENCODING_TEST_PLATFORM_H_
