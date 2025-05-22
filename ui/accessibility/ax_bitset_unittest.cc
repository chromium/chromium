// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_bitset.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

enum class TestEnum {
  kMinValue = 0,
  kFirstValue = 0,
  kMiddleValue = 30,
  kLastValue = 63,
  kMaxValue = 63,
};

// A macro for testing that a std::optional has both a value and that its value
// is set to a particular expectation.
#define EXPECT_OPTIONAL_EQ(expected, actual) \
  EXPECT_TRUE(actual.has_value());           \
  if (actual) {                              \
    EXPECT_EQ(expected, actual.value());     \
  }

// Tests a small enum (one that fits inside a single byte (aka kMaxValue <= 63).
TEST(AXBitsetTest, TestEnum) {
  AXBitset<TestEnum> map;
  EXPECT_FALSE(map.Has(TestEnum::kMinValue));
  map.Set(TestEnum::kMinValue, true);
  EXPECT_OPTIONAL_EQ(true, map.Has(TestEnum::kMinValue));
  map.Set(TestEnum::kMinValue, false);
  EXPECT_OPTIONAL_EQ(false, map.Has(TestEnum::kMinValue));
  map.Unset(TestEnum::kMinValue);
  EXPECT_FALSE(map.Has(TestEnum::kMinValue));

  EXPECT_FALSE(map.Has(TestEnum::kMiddleValue));
  map.Set(TestEnum::kMiddleValue, true);
  EXPECT_OPTIONAL_EQ(true, map.Has(TestEnum::kMiddleValue));
  map.Set(TestEnum::kMiddleValue, false);
  EXPECT_OPTIONAL_EQ(false, map.Has(TestEnum::kMiddleValue));
  map.Unset(TestEnum::kMiddleValue);
  EXPECT_FALSE(map.Has(TestEnum::kMiddleValue));

  EXPECT_FALSE(map.Has(TestEnum::kLastValue));
  map.Set(TestEnum::kLastValue, true);
  EXPECT_OPTIONAL_EQ(true, map.Has(TestEnum::kLastValue));
  map.Set(TestEnum::kLastValue, false);
  EXPECT_OPTIONAL_EQ(false, map.Has(TestEnum::kLastValue));
  map.Unset(TestEnum::kLastValue);
  EXPECT_FALSE(map.Has(TestEnum::kLastValue));

  map.Set(TestEnum::kMinValue, true);
  map.Set(TestEnum::kMiddleValue, true);
  map.Set(TestEnum::kLastValue, true);
  EXPECT_OPTIONAL_EQ(true, map.Has(TestEnum::kMinValue));
  EXPECT_OPTIONAL_EQ(true, map.Has(TestEnum::kMiddleValue));
  EXPECT_OPTIONAL_EQ(true, map.Has(TestEnum::kLastValue));
  map.Set(TestEnum::kLastValue, false);
  EXPECT_OPTIONAL_EQ(false, map.Has(TestEnum::kLastValue));
  map.Unset(TestEnum::kMinValue);
  EXPECT_FALSE(map.Has(TestEnum::kMinValue));
  EXPECT_OPTIONAL_EQ(true, map.Has(TestEnum::kMiddleValue));
}

TEST(AXBitsetTest, ForEach) {
  AXBitset<TestEnum> map;
  map.Set(TestEnum::kMinValue, true);
  map.Set(TestEnum::kMiddleValue, false);
  map.Set(TestEnum::kMaxValue, true);

  std::map<TestEnum, bool> collected_attributes;
  std::map<TestEnum, bool> expected_attributes = {
      {TestEnum::kMinValue, true},
      {TestEnum::kMiddleValue, false},
      {TestEnum::kMaxValue, true}};

  map.ForEach([&collected_attributes](TestEnum attr, bool value) {
    collected_attributes[attr] = value;
  });

  EXPECT_EQ(expected_attributes, collected_attributes);
}

TEST(AXBitsetTest, Size) {
  AXBitset<TestEnum> map;
  size_t expected_size = 0;
  EXPECT_EQ(expected_size, map.Size());

  map.Set(TestEnum::kMinValue, true);
  map.Set(TestEnum::kLastValue, false);
  expected_size = 2;
  EXPECT_EQ(expected_size, map.Size());

  // Re-setting an existing attribute.
  map.Set(TestEnum::kMinValue, false);
  EXPECT_EQ(expected_size, map.Size());

  // Unset existing attribute.
  map.Unset(TestEnum::kLastValue);
  expected_size = 1;
  EXPECT_EQ(expected_size, map.Size());
}
}  // namespace ui
