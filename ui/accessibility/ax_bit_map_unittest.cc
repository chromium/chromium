// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_bit_map.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

enum class SmallTestEnum {
  kMinValue = 0,
  kFirstValue = 0,
  kMiddleValue = 30,
  kLastValue = 63,
  kMaxValue = 63,
};

enum class LargeTestEnum {
  kMinValue = 0,
  kFirstValue = 0,
  kMiddleValue = 64,
  kLastValue = 127,
  kMaxValue = 127,
};

// A macro for testing that a std::optional has both a value and that its value
// is set to a particular expectation.
#define EXPECT_OPTIONAL_EQ(expected, actual) \
  EXPECT_TRUE(actual.has_value());           \
  if (actual) {                              \
    EXPECT_EQ(expected, actual.value());     \
  }

// Tests a small enum (one that fits inside a single byte (aka kMaxValue <= 63).
TEST(AXBitMapTest, SmallEnum) {
  AXBitMap<SmallTestEnum> map;
  EXPECT_FALSE(map.Has(SmallTestEnum::kMinValue));
  map.Set(SmallTestEnum::kMinValue, true);
  EXPECT_OPTIONAL_EQ(true, map.Has(SmallTestEnum::kMinValue));
  map.Set(SmallTestEnum::kMinValue, false);
  EXPECT_OPTIONAL_EQ(false, map.Has(SmallTestEnum::kMinValue));
  map.Unset(SmallTestEnum::kMinValue);
  EXPECT_FALSE(map.Has(SmallTestEnum::kMinValue));

  EXPECT_FALSE(map.Has(SmallTestEnum::kMiddleValue));
  map.Set(SmallTestEnum::kMiddleValue, true);
  EXPECT_OPTIONAL_EQ(true, map.Has(SmallTestEnum::kMiddleValue));
  map.Set(SmallTestEnum::kMiddleValue, false);
  EXPECT_OPTIONAL_EQ(false, map.Has(SmallTestEnum::kMiddleValue));
  map.Unset(SmallTestEnum::kMiddleValue);
  EXPECT_FALSE(map.Has(SmallTestEnum::kMiddleValue));

  EXPECT_FALSE(map.Has(SmallTestEnum::kLastValue));
  map.Set(SmallTestEnum::kLastValue, true);
  EXPECT_OPTIONAL_EQ(true, map.Has(SmallTestEnum::kLastValue));
  map.Set(SmallTestEnum::kLastValue, false);
  EXPECT_OPTIONAL_EQ(false, map.Has(SmallTestEnum::kLastValue));
  map.Unset(SmallTestEnum::kLastValue);
  EXPECT_FALSE(map.Has(SmallTestEnum::kLastValue));

  map.Set(SmallTestEnum::kMinValue, true);
  map.Set(SmallTestEnum::kMiddleValue, true);
  map.Set(SmallTestEnum::kLastValue, true);
  EXPECT_OPTIONAL_EQ(true, map.Has(SmallTestEnum::kMinValue));
  EXPECT_OPTIONAL_EQ(true, map.Has(SmallTestEnum::kMiddleValue));
  EXPECT_OPTIONAL_EQ(true, map.Has(SmallTestEnum::kLastValue));
  map.Set(SmallTestEnum::kLastValue, false);
  EXPECT_OPTIONAL_EQ(false, map.Has(SmallTestEnum::kLastValue));
  map.Unset(SmallTestEnum::kMinValue);
  EXPECT_FALSE(map.Has(SmallTestEnum::kMinValue));
  EXPECT_OPTIONAL_EQ(true, map.Has(SmallTestEnum::kMiddleValue));
}

// Tests a large enum (one that requires two bytes (aka kMaxValue <= 127).
TEST(AXBitMapTest, LargeEnum) {
  AXBitMap<LargeTestEnum> map;
  EXPECT_FALSE(map.Has(LargeTestEnum::kMinValue));
  map.Set(LargeTestEnum::kMinValue, true);
  EXPECT_OPTIONAL_EQ(true, map.Has(LargeTestEnum::kMinValue));
  map.Set(LargeTestEnum::kMinValue, false);
  EXPECT_OPTIONAL_EQ(false, map.Has(LargeTestEnum::kMinValue));
  map.Unset(LargeTestEnum::kMinValue);
  EXPECT_FALSE(map.Has(LargeTestEnum::kMinValue));

  EXPECT_FALSE(map.Has(LargeTestEnum::kMiddleValue));
  map.Set(LargeTestEnum::kMiddleValue, true);
  EXPECT_OPTIONAL_EQ(true, map.Has(LargeTestEnum::kMiddleValue));
  map.Set(LargeTestEnum::kMiddleValue, false);
  EXPECT_OPTIONAL_EQ(false, map.Has(LargeTestEnum::kMiddleValue));
  map.Unset(LargeTestEnum::kMiddleValue);
  EXPECT_FALSE(map.Has(LargeTestEnum::kMiddleValue));

  EXPECT_FALSE(map.Has(LargeTestEnum::kLastValue));
  map.Set(LargeTestEnum::kLastValue, true);
  EXPECT_OPTIONAL_EQ(true, map.Has(LargeTestEnum::kLastValue));
  map.Set(LargeTestEnum::kLastValue, false);
  EXPECT_OPTIONAL_EQ(false, map.Has(LargeTestEnum::kLastValue));
  map.Unset(LargeTestEnum::kLastValue);
  EXPECT_FALSE(map.Has(LargeTestEnum::kLastValue));

  map.Set(LargeTestEnum::kMinValue, true);
  map.Set(LargeTestEnum::kMiddleValue, true);
  map.Set(LargeTestEnum::kLastValue, true);
  EXPECT_OPTIONAL_EQ(true, map.Has(LargeTestEnum::kMinValue));
  EXPECT_OPTIONAL_EQ(true, map.Has(LargeTestEnum::kMiddleValue));
  EXPECT_OPTIONAL_EQ(true, map.Has(LargeTestEnum::kLastValue));
  map.Set(LargeTestEnum::kLastValue, false);
  EXPECT_OPTIONAL_EQ(false, map.Has(LargeTestEnum::kLastValue));
  map.Unset(LargeTestEnum::kMinValue);
  EXPECT_FALSE(map.Has(LargeTestEnum::kMinValue));
  EXPECT_OPTIONAL_EQ(true, map.Has(LargeTestEnum::kMiddleValue));
}

}  // namespace ui
