// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/json_schema_compiler/test/test_features.h"

using test::features::TestFeatures;

TEST(FeaturesGeneratorTest, FromString) {
  TestFeatures test_features;
  EXPECT_EQ(TestFeatures::kSimple, test_features.FromString("simple"));
  EXPECT_EQ(TestFeatures::kComplex, test_features.FromString("complex"));
}

TEST(FeaturesGeneratorTest, ToString) {
  TestFeatures test_features;
  EXPECT_STREQ("simple", test_features.ToString(TestFeatures::kSimple));
  EXPECT_STREQ("complex", test_features.ToString(TestFeatures::kComplex));
}
