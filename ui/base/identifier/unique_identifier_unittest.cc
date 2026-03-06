// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/identifier/unique_identifier.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui::internal {

class UniqueIdentifierTest;

namespace {

DECLARE_UNIQUE_IDENTIFIER_TYPE(TestIdentifier, UniqueIdentifierTest);

DECLARE_UNIQUE_IDENTIFIER_VALUE(TestIdentifier, kTestUniqueIdentifier);
DEFINE_UNIQUE_IDENTIFIER_VALUE(TestIdentifier, kTestUniqueIdentifier);
constexpr char kTestUniqueIdentifierName[] = "kTestUniqueIdentifier";
DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE(TestIdentifier,
                                     kTestLocalUniqueIdentifier);

consteval bool IdEqual(TestIdentifier e1, TestIdentifier e2) {
  return e1 == e2;
}

}  // namespace

class UniqueIdentifierTest : public testing::Test {
 public:
  void SetUp() override { UniqueIdentifier::ClearKnownIdentifiersForTesting(); }

  DECLARE_CLASS_UNIQUE_IDENTIFIER_VALUE(TestIdentifier,
                                        kTestClassUniqueIdentifier);

 protected:
  static intptr_t GetRawValue(TestIdentifier id) {
    return id.GetRawValue(base::PassKey<UniqueIdentifierTest>());
  }
};

DEFINE_CLASS_UNIQUE_IDENTIFIER_VALUE(UniqueIdentifierTest,
                                     TestIdentifier,
                                     kTestClassUniqueIdentifier);

TEST_F(UniqueIdentifierTest, Constexpr) {
  EXPECT_TRUE(IdEqual(kTestUniqueIdentifier, kTestUniqueIdentifier));
  EXPECT_FALSE(IdEqual(kTestClassUniqueIdentifier, kTestLocalUniqueIdentifier));
  // TODO(crbug.com/333028921): Put in compile-time checks for `operator <` once
  // it is constexpr, perhaps using `base::MakeFlatSet<>()`.
}

TEST_F(UniqueIdentifierTest, FromName) {
  EXPECT_FALSE(TestIdentifier::FromName(kTestUniqueIdentifierName));
  EXPECT_EQ(kTestUniqueIdentifierName, kTestUniqueIdentifier.GetName());
  EXPECT_EQ(kTestUniqueIdentifier,
            TestIdentifier::FromName(kTestUniqueIdentifierName));
}

TEST_F(UniqueIdentifierTest, FromRawValue) {
  EXPECT_FALSE(TestIdentifier::FromRawValue(0));
  const intptr_t raw_value = GetRawValue(kTestUniqueIdentifier);
  EXPECT_NE(0, raw_value);
  EXPECT_EQ(kTestUniqueIdentifier, TestIdentifier::FromRawValue(raw_value));
}

}  // namespace ui::internal
