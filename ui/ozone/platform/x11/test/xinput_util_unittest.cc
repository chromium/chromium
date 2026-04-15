// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/x11/xinput_util.h"

#include <vector>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/x/xinput.h"

namespace ui {

// Regression test for crbug.com/501862016.
TEST(XInputUtilTest, IsXinputMaskSetBoundsCheck) {
  std::vector<uint32_t> mask = {1};  // 4 bytes, covering opcodes 0-31
  auto mask_bytes = base::as_byte_span(mask);

  // Opcode 0 is set (first bit of 1)
  EXPECT_TRUE(IsXinputMaskSet(mask_bytes, 0));

  // Opcode 1 is not set
  EXPECT_FALSE(IsXinputMaskSet(mask_bytes, 1));

  // Opcode 31 is not set
  EXPECT_FALSE(IsXinputMaskSet(mask_bytes, 31));

  // Opcode 32 is out of bounds for a 4-byte mask
  EXPECT_FALSE(IsXinputMaskSet(mask_bytes, 32));

  // Opcode 1000 is way out of bounds
  EXPECT_FALSE(IsXinputMaskSet(mask_bytes, 1000));
}

// Regression test for crbug.com/501862016.
TEST(XInputUtilTest, SetXinputMask) {
  std::vector<uint32_t> mask = {0};  // 4 bytes, covering opcodes 0-31

  // Setting opcode 0 should work
  SetXinputMask(base::as_writable_byte_span(mask), 0);
  EXPECT_EQ(mask[0], 1u);
  EXPECT_TRUE(IsXinputMaskSet(base::as_byte_span(mask), 0));
}

// Regression test for crbug.com/501862016.
TEST(XInputUtilTest, SetXinputMaskDeathTest) {
  std::vector<uint32_t> mask = {0};  // 4 bytes, covering opcodes 0-31

  // Setting opcode 32 (out of bounds) should CHECK/crash
  EXPECT_DEATH_IF_SUPPORTED(
      SetXinputMask(base::as_writable_byte_span(mask), 32), "");
}

// Regression test for crbug.com/501862016.
TEST(XInputUtilTest, XIEventMaskSpan) {
  x11::Input::XIEventMask xi_mask{};

  // Initially nothing set
  EXPECT_FALSE(IsXinputMaskSet(base::byte_span_from_ref(xi_mask), 1));

  // Set some bit
  SetXinputMask(base::byte_span_from_ref(xi_mask), 1);
  EXPECT_TRUE(IsXinputMaskSet(base::byte_span_from_ref(xi_mask), 1));

  // Out of bounds for int-sized mask (usually 32 bits)
  EXPECT_FALSE(IsXinputMaskSet(base::byte_span_from_ref(xi_mask), 100));
}

}  // namespace ui
