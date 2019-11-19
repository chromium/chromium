// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_enum_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"

namespace ui {

// Templatized function that tests that for a mojom enum
// such as ax::mojom::Role, ax::mojom::Event, etc. we can
// call ToString() on the enum to get a string, and then
// ParseEnumName() on the string to get back the original
// value. Also tests what happens when we call ToString
// or ParseEnumName on a bogus value.
template <typename T>
void TestEnumStringConversion(T(ParseFunction)(const char*)) {
  // Check every valid enum value.
  for (int i = static_cast<int>(T::kMinValue);
       i <= static_cast<int>(T::kMaxValue); ++i) {
    T src = static_cast<T>(i);
    std::string str = ToString(src);
    auto dst = ParseFunction(str.c_str());
    EXPECT_EQ(src, dst);
  }

  // Parse a bogus string.
  EXPECT_EQ(T::kNone, ParseFunction("bogus"));

  // Convert a bogus value to a string.
  EXPECT_STREQ("", ToString(static_cast<T>(999999)));
}

// Templatized function that tries calling a setter on AXNodeData
// such as AddIntAttribute, AddFloatAttribute - with each possible
// enum value.
//
// This variant is for cases where the value type is an object.
template <typename T, typename U>
void TestAXNodeDataSetter(void (AXNodeData::*Setter)(T, const U&),
                          const U& value) {
  AXNodeData node_data;
  for (int i = static_cast<int>(T::kMinValue) + 1;
       i <= static_cast<int>(T::kMaxValue); ++i) {
    T attr = static_cast<T>(i);
    ((node_data).*(Setter))(attr, value);
  }
  EXPECT_TRUE(!node_data.ToString().empty());
}

// Same as TextAXNodeData, above, but This variant is for
// cases where the value type is POD.
template <typename T, typename U>
void TestAXNodeDataSetter(void (AXNodeData::*Setter)(T, U), U value) {
  AXNodeData node_data;
  for (int i = static_cast<int>(T::kMinValue) + 1;
       i <= static_cast<int>(T::kMaxValue); ++i) {
    T attr = static_cast<T>(i);
    ((node_data).*(Setter))(attr, value);
  }
  EXPECT_TRUE(!node_data.ToString().empty());
}

TEST(AXEnumUtilTest, Event) {
  TestEnumStringConversion<ax::mojom::Event>(ParseEvent);
}

TEST(AXEnumUtilTest, Role) {
  TestEnumStringConversion<ax::mojom::Role>(ParseRole);
}

TEST(AXEnumUtilTest, State) {
  TestEnumStringConversion<ax::mojom::State>(ParseState);
}

TEST(AXEnumUtilTest, Action) {
  TestEnumStringConversion<ax::mojom::Action>(ParseAction);
}

TEST(AXEnumUtilTest, ActionFlags) {
  TestEnumStringConversion<ax::mojom::ActionFlags>(ParseActionFlags);
}

TEST(AXEnumUtilTest, DefaultActionVerb) {
  TestEnumStringConversion<ax::mojom::DefaultActionVerb>(
      ParseDefaultActionVerb);
}

TEST(AXEnumUtilTest, Mutation) {
  TestEnumStringConversion<ax::mojom::Mutation>(ParseMutation);
}

TEST(AXEnumUtilTest, StringAttribute) {
  TestEnumStringConversion<ax::mojom::StringAttribute>(ParseStringAttribute);
  TestAXNodeDataSetter<ax::mojom::StringAttribute>(
      &AXNodeData::AddStringAttribute, std::string());
}

TEST(AXEnumUtilTest, IntAttribute) {
  TestEnumStringConversion<ax::mojom::IntAttribute>(ParseIntAttribute);
  TestAXNodeDataSetter<ax::mojom::IntAttribute>(&AXNodeData::AddIntAttribute,
                                                0);
}

TEST(AXEnumUtilTest, FloatAttribute) {
  TestEnumStringConversion<ax::mojom::FloatAttribute>(ParseFloatAttribute);
  TestAXNodeDataSetter<ax::mojom::FloatAttribute>(
      &AXNodeData::AddFloatAttribute, 0.0f);
}

TEST(AXEnumUtilTest, BoolAttribute) {
  TestEnumStringConversion<ax::mojom::BoolAttribute>(ParseBoolAttribute);
  TestAXNodeDataSetter<ax::mojom::BoolAttribute>(&AXNodeData::AddBoolAttribute,
                                                 false);
}

TEST(AXEnumUtilTest, IntListAttribute) {
  TestEnumStringConversion<ax::mojom::IntListAttribute>(ParseIntListAttribute);
  TestAXNodeDataSetter<ax::mojom::IntListAttribute>(
      &AXNodeData::AddIntListAttribute, std::vector<int32_t>());
}

TEST(AXEnumUtilTest, StringListAttribute) {
  TestEnumStringConversion<ax::mojom::StringListAttribute>(
      ParseStringListAttribute);
  TestAXNodeDataSetter<ax::mojom::StringListAttribute>(
      &AXNodeData::AddStringListAttribute, std::vector<std::string>());
}

// TODO(dmazzoni) this should be an enum of flags, not an
// enum of every possible bitfield value.
TEST(AXEnumUtilTest, DISABLED_MarkerType) {
  TestEnumStringConversion<ax::mojom::MarkerType>(ParseMarkerType);
}

TEST(AXEnumUtilTest, Text_Decoration_Style) {
  TestEnumStringConversion<ax::mojom::TextDecorationStyle>(
      ParseTextDecorationStyle);
}

TEST(AXEnumUtilTest, ListStyle) {
  TestEnumStringConversion<ax::mojom::ListStyle>(ParseListStyle);
}

TEST(AXEnumUtilTest, TextDirection) {
  TestEnumStringConversion<ax::mojom::TextDirection>(ParseTextDirection);
}

TEST(AXEnumUtilTest, TextPosition) {
  TestEnumStringConversion<ax::mojom::TextPosition>(ParseTextPosition);
}

TEST(AXEnumUtilTest, TextStyle) {
  TestEnumStringConversion<ax::mojom::TextStyle>(ParseTextStyle);
}

TEST(AXEnumUtilTest, AriaCurrentState) {
  TestEnumStringConversion<ax::mojom::AriaCurrentState>(ParseAriaCurrentState);
}

TEST(AXEnumUtilTest, HasPopup) {
  TestEnumStringConversion<ax::mojom::HasPopup>(ParseHasPopup);
}

TEST(AXEnumUtilTest, InvalidState) {
  TestEnumStringConversion<ax::mojom::InvalidState>(ParseInvalidState);
}

TEST(AXEnumUtilTest, Restriction) {
  TestEnumStringConversion<ax::mojom::Restriction>(ParseRestriction);
}

TEST(AXEnumUtilTest, CheckedState) {
  TestEnumStringConversion<ax::mojom::CheckedState>(ParseCheckedState);
}

TEST(AXEnumUtilTest, SortDirection) {
  TestEnumStringConversion<ax::mojom::SortDirection>(ParseSortDirection);
}

TEST(AXEnumUtilTest, NameFrom) {
  TestEnumStringConversion<ax::mojom::NameFrom>(ParseNameFrom);
}

TEST(AXEnumUtilTest, DescriptionFrom) {
  TestEnumStringConversion<ax::mojom::DescriptionFrom>(ParseDescriptionFrom);
}

TEST(AXEnumUtilTest, EventFrom) {
  TestEnumStringConversion<ax::mojom::EventFrom>(ParseEventFrom);
}

TEST(AXEnumUtilTest, Gesture) {
  TestEnumStringConversion<ax::mojom::Gesture>(ParseGesture);
}

TEST(AXEnumUtilTest, TextAffinity) {
  TestEnumStringConversion<ax::mojom::TextAffinity>(ParseTextAffinity);
}

TEST(AXEnumUtilTest, TreeOrder) {
  TestEnumStringConversion<ax::mojom::TreeOrder>(ParseTreeOrder);
}

TEST(AXEnumUtilTest, ImageAnnotationStatus) {
  TestEnumStringConversion<ax::mojom::ImageAnnotationStatus>(
      ParseImageAnnotationStatus);
}

TEST(AXEnumUtilTest, Dropeffect) {
  TestEnumStringConversion<ax::mojom::Dropeffect>(ParseDropeffect);
}

}  // namespace ui
