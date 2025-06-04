
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_MESSAGE_TEST_TYPES_H_
#define IPCZ_SRC_IPCZ_MESSAGE_TEST_TYPES_H_

#include <cstdint>

namespace ipcz::test {

// E.g. see LinkSide.
struct TestEnum8 {
  enum class Value : uint8_t {
    kA = 0,
    kB = 1,
    kC = 2,
  };

  // For generated message validation code.
  static constexpr Value kMinValue = Value::kA;
  static constexpr Value kMaxValue = Value::kC;

  Value v;
};

enum class TestEnum32 : uint32_t {
  kZero,
  kOne,
  kTwo,
  kThree,
  kFour,
  // For validation.
  kMinValue = kZero,
  kMaxValue = kFour,
};

}  // namespace ipcz::test

#endif  // IPCZ_SRC_IPCZ_MESSAGE_TEST_TYPES_H_
