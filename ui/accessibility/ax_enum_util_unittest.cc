// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_enum_util.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"

namespace ui {

// Templatized function that tests that for a mojom enum
// such as ax::mojom::Role, ax::mojom::Event, etc. we can
// call ToString() on the enum to get a string, and then
// ParseAXEnum() on the string to get back the original
// value. Also tests what happens when we call ToString
// or ParseAXEnum on a bogus value.
template <typename T>
void TestEnumStringConversion(
    int32_t(step)(int32_t) = [](int32_t val) { return val + 1; }) {
  // Check every valid enum value.
  for (int i = static_cast<int>(T::kMinValue);
       i <= static_cast<int>(T::kMaxValue); i = step(i)) {
    T src = static_cast<T>(i);
    std::string str = ToString(src);
    auto dst = ParseAXEnum<T>(str.c_str());
    EXPECT_EQ(src, dst);
  }

  // Trying to parse a bogus string should crash.
  // TODO(http://crbug.com/1129919) - try to re-enable this death test.
  // EXPECT_DEATH_IF_SUPPORTED(ParseAXEnum<T>("bogus"), "Check failed");

  // Parse the empty string.
  EXPECT_EQ(T::kNone, ParseAXEnum<T>(""));

  // Convert a bogus value to a string.
  int out_of_range_value = static_cast<int>(T::kMaxValue) + 1;
  EXPECT_STREQ("", ToString(static_cast<T>(out_of_range_value)));
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
  TestEnumStringConversion<ax::mojom::Event>();
}

TEST(AXEnumUtilTest, Role) {
  TestEnumStringConversion<ax::mojom::Role>();
}

TEST(AXEnumUtilTest, State) {
  TestEnumStringConversion<ax::mojom::State>();
}

TEST(AXEnumUtilTest, Action) {
  TestEnumStringConversion<ax::mojom::Action>();
}

TEST(AXEnumUtilTest, ActionFlags) {
  TestEnumStringConversion<ax::mojom::ActionFlags>();
}

TEST(AXEnumUtilTest, DefaultActionVerb) {
  TestEnumStringConversion<ax::mojom::DefaultActionVerb>();
}

TEST(AXEnumUtilTest, Mutation) {
  TestEnumStringConversion<ax::mojom::Mutation>();
}

TEST(AXEnumUtilTest, StringAttribute) {
  TestEnumStringConversion<ax::mojom::StringAttribute>();

  AXNodeData node_data;
  for (int i = static_cast<int>(ax::mojom::StringAttribute::kMinValue) + 1;
       i <= static_cast<int>(ax::mojom::StringAttribute::kMaxValue); ++i) {
    ax::mojom::StringAttribute attr =
        static_cast<ax::mojom::StringAttribute>(i);
    if (attr == ax::mojom::StringAttribute::kChildTreeId)
      continue;
    node_data.AddStringAttribute(attr, std::string());
  }
  EXPECT_TRUE(!node_data.ToString().empty());
}

TEST(AXEnumUtilTest, IntAttribute) {
  TestEnumStringConversion<ax::mojom::IntAttribute>();
  TestAXNodeDataSetter<ax::mojom::IntAttribute>(&AXNodeData::AddIntAttribute,
                                                0);
}

TEST(AXEnumUtilTest, FloatAttribute) {
  TestEnumStringConversion<ax::mojom::FloatAttribute>();
  TestAXNodeDataSetter<ax::mojom::FloatAttribute>(
      &AXNodeData::AddFloatAttribute, 0.0f);
}

TEST(AXEnumUtilTest, BoolAttribute) {
  TestEnumStringConversion<ax::mojom::BoolAttribute>();
  TestAXNodeDataSetter<ax::mojom::BoolAttribute>(&AXNodeData::AddBoolAttribute,
                                                 false);
}

TEST(AXEnumUtilTest, IntListAttribute) {
  TestEnumStringConversion<ax::mojom::IntListAttribute>();
  TestAXNodeDataSetter<ax::mojom::IntListAttribute>(
      &AXNodeData::AddIntListAttribute, std::vector<int32_t>());
}

TEST(AXEnumUtilTest, StringListAttribute) {
  TestEnumStringConversion<ax::mojom::StringListAttribute>();
  TestAXNodeDataSetter<ax::mojom::StringListAttribute>(
      &AXNodeData::AddStringListAttribute, std::vector<std::string>());
}

TEST(AXEnumUtilTest, MarkerType) {
  TestEnumStringConversion<ax::mojom::MarkerType>(
      [](int32_t val) {
        return val == 0 ? 1 :
                        // 8 (Composition) is
                        // explicitly skipped in
                        // ax_enums.mojom.
                   val == 4 ? 16 : val * 2;
      });
}

TEST(AXEnumUtilTest, Text_Decoration_Style) {
  TestEnumStringConversion<ax::mojom::TextDecorationStyle>();
}

TEST(AXEnumUtilTest, ListStyle) {
  TestEnumStringConversion<ax::mojom::ListStyle>();
}

TEST(AXEnumUtilTest, MoveDirection) {
  TestEnumStringConversion<ax::mojom::MoveDirection>();
}

TEST(AXEnumUtilTest, Command) {
  TestEnumStringConversion<ax::mojom::Command>();
}

TEST(AXEnumUtilTest, InputEventType) {
  TestEnumStringConversion<ax::mojom::InputEventType>();
}

TEST(AXEnumUtilTest, TextBoundary) {
  TestEnumStringConversion<ax::mojom::TextBoundary>();
}

TEST(AXEnumUtilTest, TextAlign) {
  TestEnumStringConversion<ax::mojom::TextAlign>();
}

TEST(AXEnumUtilTest, WritingDirection) {
  TestEnumStringConversion<ax::mojom::WritingDirection>();
}

TEST(AXEnumUtilTest, TextPosition) {
  TestEnumStringConversion<ax::mojom::TextPosition>();
}

TEST(AXEnumUtilTest, TextStyle) {
  TestEnumStringConversion<ax::mojom::TextStyle>();
}

TEST(AXEnumUtilTest, AriaCurrentState) {
  TestEnumStringConversion<ax::mojom::AriaCurrentState>();
}

TEST(AXEnumUtilTest, HasPopup) {
  TestEnumStringConversion<ax::mojom::HasPopup>();
}

TEST(AXEnumUtilTest, InvalidState) {
  TestEnumStringConversion<ax::mojom::InvalidState>();
}

TEST(AXEnumUtilTest, Restriction) {
  TestEnumStringConversion<ax::mojom::Restriction>();
}

TEST(AXEnumUtilTest, CheckedState) {
  TestEnumStringConversion<ax::mojom::CheckedState>();
}

TEST(AXEnumUtilTest, SortDirection) {
  TestEnumStringConversion<ax::mojom::SortDirection>();
}

TEST(AXEnumUtilTest, NameFrom) {
  TestEnumStringConversion<ax::mojom::NameFrom>();
}

TEST(AXEnumUtilTest, DescriptionFrom) {
  TestEnumStringConversion<ax::mojom::DescriptionFrom>();
}

TEST(AXEnumUtilTest, EventFrom) {
  TestEnumStringConversion<ax::mojom::EventFrom>();
}

TEST(AXEnumUtilTest, Gesture) {
  TestEnumStringConversion<ax::mojom::Gesture>();
}

TEST(AXEnumUtilTest, TextAffinity) {
  TestEnumStringConversion<ax::mojom::TextAffinity>();
}

TEST(AXEnumUtilTest, TreeOrder) {
  TestEnumStringConversion<ax::mojom::TreeOrder>();
}

TEST(AXEnumUtilTest, ImageAnnotationStatus) {
  TestEnumStringConversion<ax::mojom::ImageAnnotationStatus>();
}

}  // namespace ui
