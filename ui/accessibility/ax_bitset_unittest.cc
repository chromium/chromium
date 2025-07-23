// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_bitset.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

enum class TestEnum {
  kMinValue = 0,
  VAL_0 = 0,
  VAL_1 = 1,
  VAL_2 = 2,
  VAL_3 = 3,
  VAL_31 = 31,
  kMaxValue = 31,
};

// A macro for testing that a std::optional has both a value and that its value
// is set to a particular expectation.
#define EXPECT_OPTIONAL_EQ(expected, actual) \
  EXPECT_TRUE(actual.has_value());           \
  if (actual) {                              \
    EXPECT_EQ(expected, actual.value());     \
  }

// Tests a small enum (one that fits inside a single byte (aka kMaxValue <= 31).
TEST(AXBitsetTest, TestEnum) {
  AXBitset<TestEnum> map;
  EXPECT_FALSE(map.Get(TestEnum::kMinValue));
  map.Set(TestEnum::kMinValue, true);
  EXPECT_OPTIONAL_EQ(true, map.Get(TestEnum::kMinValue));
  map.Set(TestEnum::kMinValue, false);
  EXPECT_OPTIONAL_EQ(false, map.Get(TestEnum::kMinValue));
  map.Unset(TestEnum::kMinValue);
  EXPECT_FALSE(map.Get(TestEnum::kMinValue));

  EXPECT_FALSE(map.Get(TestEnum::VAL_2));
  map.Set(TestEnum::VAL_2, true);
  EXPECT_OPTIONAL_EQ(true, map.Get(TestEnum::VAL_2));
  map.Set(TestEnum::VAL_2, false);
  EXPECT_OPTIONAL_EQ(false, map.Get(TestEnum::VAL_2));
  map.Unset(TestEnum::VAL_2);
  EXPECT_FALSE(map.Get(TestEnum::VAL_2));

  EXPECT_FALSE(map.Get(TestEnum::VAL_31));
  map.Set(TestEnum::VAL_31, true);
  EXPECT_OPTIONAL_EQ(true, map.Get(TestEnum::VAL_31));
  map.Set(TestEnum::VAL_31, false);
  EXPECT_OPTIONAL_EQ(false, map.Get(TestEnum::VAL_31));
  map.Unset(TestEnum::VAL_31);
  EXPECT_FALSE(map.Get(TestEnum::VAL_31));

  map.Set(TestEnum::kMinValue, true);
  map.Set(TestEnum::VAL_2, true);
  map.Set(TestEnum::VAL_31, true);
  EXPECT_OPTIONAL_EQ(true, map.Get(TestEnum::kMinValue));
  EXPECT_OPTIONAL_EQ(true, map.Get(TestEnum::VAL_2));
  EXPECT_OPTIONAL_EQ(true, map.Get(TestEnum::VAL_31));
  map.Set(TestEnum::VAL_31, false);
  EXPECT_OPTIONAL_EQ(false, map.Get(TestEnum::VAL_31));
  map.Unset(TestEnum::kMinValue);
  EXPECT_FALSE(map.Get(TestEnum::kMinValue));
  EXPECT_OPTIONAL_EQ(true, map.Get(TestEnum::VAL_2));
}

TEST(AXBitsetTest, ForEach) {
  AXBitset<TestEnum> map;
  map.Set(TestEnum::kMinValue, true);
  map.Set(TestEnum::VAL_2, false);
  map.Set(TestEnum::kMaxValue, true);

  std::map<TestEnum, bool> collected_attributes;
  std::map<TestEnum, bool> expected_attributes = {{TestEnum::kMinValue, true},
                                                  {TestEnum::VAL_2, false},
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
  map.Set(TestEnum::VAL_31, false);
  expected_size = 2;
  EXPECT_EQ(expected_size, map.Size());

  // Re-setting an existing attribute.
  map.Set(TestEnum::kMinValue, false);
  EXPECT_EQ(expected_size, map.Size());

  // Unset existing attribute.
  map.Unset(TestEnum::VAL_31);
  expected_size = 1;
  EXPECT_EQ(expected_size, map.Size());
}

TEST(AXBitsetTest, Append) {
  AXBitset<TestEnum> set_a;
  AXBitset<TestEnum> set_b;
  set_a.Set(TestEnum::VAL_0, true);
  set_a.Set(TestEnum::VAL_1, true);
  set_a.Set(TestEnum::VAL_2, false);

  set_b.Set(TestEnum::VAL_1, false);
  set_b.Set(TestEnum::VAL_2, true);
  set_b.Set(TestEnum::VAL_3, true);
  set_b.Set(TestEnum::VAL_31, false);

  set_a.Append(set_b);

  EXPECT_OPTIONAL_EQ(true, set_a.Get(TestEnum::VAL_0));    // No change.
  EXPECT_OPTIONAL_EQ(false, set_a.Get(TestEnum::VAL_1));   // Overridden.
  EXPECT_OPTIONAL_EQ(true, set_a.Get(TestEnum::VAL_2));    // Overridden.
  EXPECT_OPTIONAL_EQ(true, set_a.Get(TestEnum::VAL_3));    // New value.
  EXPECT_OPTIONAL_EQ(false, set_a.Get(TestEnum::VAL_31));  // New value.
}
}  // namespace ui
