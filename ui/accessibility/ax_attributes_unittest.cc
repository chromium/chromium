// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_attributes.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ui {

TEST(AXIntAttributesTest, AbsenceIsZero) {
  ASSERT_EQ(AXIntAttributes().Get(ax::mojom::IntAttribute::kDefaultActionVerb),
            0);
}

TEST(AXFloatAttributesTest, AbsenceIsZero) {
  ASSERT_EQ(AXFloatAttributes().Get(ax::mojom::FloatAttribute::kValueForRange),
            0.0f);
}

TEST(AXBoolAttributesTest, AbsenceIsFalse) {
  ASSERT_EQ(AXBoolAttributes().Get(ax::mojom::BoolAttribute::kBusy), false);
}

TEST(AXStringAttributesTest, AbsenceIsEmptyString) {
  ASSERT_EQ(AXStringAttributes().Get(ax::mojom::StringAttribute::kAccessKey),
            std::string());
}

TEST(AXStringListAttributesTest, AbsenceIsEmptyVecetor) {
  ASSERT_EQ(AXStringListAttributes().Get(
                ax::mojom::StringListAttribute::kAriaNotificationAnnouncements),
            std::vector<std::string>());
}

TEST(AXIntListAttributesTest, AbsenceIsEmptyVector) {
  ASSERT_EQ(
      AXIntListAttributes().Get(ax::mojom::IntListAttribute::kIndirectChildIds),
      std::vector<int32_t>());
}

// Test general operations with a fundamental type.
TEST(AXAttributesFundamentalTest, Do) {
  AXIntAttributes a;

  // Initially empty.
  ASSERT_EQ(a.size(), 0U);
  ASSERT_FALSE(a.Has(ax::mojom::IntAttribute::kDefaultActionVerb));

  // Set adds a value.
  a.Set(ax::mojom::IntAttribute::kDefaultActionVerb, 5);
  ASSERT_EQ(a.size(), 1U);
  ASSERT_TRUE(a.Has(ax::mojom::IntAttribute::kDefaultActionVerb));
  ASSERT_EQ(a.Get(ax::mojom::IntAttribute::kDefaultActionVerb), 5);

  // Set overwrites a previous value.
  a.Set(ax::mojom::IntAttribute::kDefaultActionVerb, 6);
  ASSERT_EQ(a.size(), 1U);
  ASSERT_EQ(a.Get(ax::mojom::IntAttribute::kDefaultActionVerb), 6);

  // Remove, well, removes a value.
  a.Remove(ax::mojom::IntAttribute::kDefaultActionVerb);
  ASSERT_EQ(a.size(), 0U);
  ASSERT_FALSE(a.Has(ax::mojom::IntAttribute::kDefaultActionVerb));
}

// Test general operations with an object type.
TEST(AXAttributesObjectTest, Do) {
  AXStringAttributes a;

  // Initially empty.
  ASSERT_EQ(a.size(), 0U);
  ASSERT_FALSE(a.Has(ax::mojom::StringAttribute::kAccessKey));

  // Set adds a value.
  a.Set(ax::mojom::StringAttribute::kAccessKey, "hi");
  ASSERT_EQ(a.size(), 1U);
  ASSERT_TRUE(a.Has(ax::mojom::StringAttribute::kAccessKey));
  ASSERT_EQ(a.Get(ax::mojom::StringAttribute::kAccessKey), "hi");

  // Get returns a reference rather than an instance.
  {
    const auto& r1 = a.Get(ax::mojom::StringAttribute::kAccessKey);
    const auto& r2 = a.Get(ax::mojom::StringAttribute::kAccessKey);
    ASSERT_EQ(&r1, &r2);
  }

  // Set overwrites a previous value.
  a.Set(ax::mojom::StringAttribute::kAccessKey, "there");
  ASSERT_EQ(a.size(), 1U);
  ASSERT_EQ(a.Get(ax::mojom::StringAttribute::kAccessKey), "there");

  // Remove, well, removes a value.
  a.Remove(ax::mojom::StringAttribute::kAccessKey);
  ASSERT_EQ(a.size(), 0U);
  ASSERT_FALSE(a.Has(ax::mojom::StringAttribute::kAccessKey));
}

}  // namespace ui
