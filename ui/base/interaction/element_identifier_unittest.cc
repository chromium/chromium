// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_identifier.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

// Use DECLARE/DEFINE_ELEMENT instead of DEFINE_LOCAL_ELEMENT so that this
// ElementIdentifier's name is predictable.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kTestElementIdentifier);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kTestElementIdentifier);
const char* const kTestElementIdentifierName = "kTestElementIdentifier";

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestLocalElementIdentifier);

consteval bool IdEqual(ElementIdentifier e1, ElementIdentifier e2) {
  return e1 == e2;
}

}  // namespace

class ElementIdentifierTest : public testing::Test {
 public:
  void SetUp() override { ElementIdentifier::GetKnownIdentifiers().clear(); }

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTestClassElementIdentifier);

 protected:
  static intptr_t GetRawValue(ElementIdentifier id) { return id.GetRawValue(); }
};

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ElementIdentifierTest,
                                      kTestClassElementIdentifier);

TEST_F(ElementIdentifierTest, Constexpr) {
  EXPECT_TRUE(IdEqual(kTestElementIdentifier, kTestElementIdentifier));
  EXPECT_FALSE(
      IdEqual(kTestClassElementIdentifier, kTestLocalElementIdentifier));
  // TODO(crbug.com/333028921): Put in compile-time checks for `operator <` once
  // it is constexpr, perhaps using `base::MakeFlatSet<>()`.
}

TEST_F(ElementIdentifierTest, FromName) {
  EXPECT_FALSE(ElementIdentifier::FromName(kTestElementIdentifierName));
  EXPECT_EQ(kTestElementIdentifierName, kTestElementIdentifier.GetName());
  EXPECT_EQ(kTestElementIdentifier,
            ElementIdentifier::FromName(kTestElementIdentifierName));
}

TEST_F(ElementIdentifierTest, FromRawValue) {
  EXPECT_FALSE(ElementIdentifier::FromRawValue(0));
  const intptr_t raw_value = GetRawValue(kTestElementIdentifier);
  EXPECT_NE(0, raw_value);
  EXPECT_EQ(kTestElementIdentifier, ElementIdentifier::FromRawValue(raw_value));
}

}  // namespace ui
