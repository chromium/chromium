// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_property_node.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

using ui::AXPropertyFilter;
using ui::AXPropertyNode;

namespace ui {

class AXPropertyNodeTest : public testing::Test {
 public:
  AXPropertyNodeTest() = default;
  ~AXPropertyNodeTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AXPropertyNodeTest);
};

AXPropertyNode Parse(const char* input) {
  AXPropertyFilter filter(input, AXPropertyFilter::ALLOW);
  return AXPropertyNode::From(filter);
}

AXPropertyNode GetArgumentNode(const char* input) {
  auto got = Parse(input);
  if (got.parameters.size() == 0) {
    return AXPropertyNode();
  }
  return std::move(got.parameters[0]);
}

void ParseAndCheck(const char* input, const char* expected) {
  auto got = Parse(input).ToString();
  EXPECT_EQ(got, expected);
}

struct ProperyNodeCheck {
  std::string target;
  std::string name_or_value;
  std::vector<ProperyNodeCheck> parameters;
};

void Check(const AXPropertyNode& got, const ProperyNodeCheck& expected) {
  EXPECT_EQ(got.target, expected.target);
  EXPECT_EQ(got.name_or_value, expected.name_or_value);
  EXPECT_EQ(got.parameters.size(), expected.parameters.size());
  for (auto i = 0U;
       i < std::min(expected.parameters.size(), got.parameters.size()); i++) {
    Check(got.parameters[i], expected.parameters[i]);
  }
}

void ParseAndCheck(const char* input, const ProperyNodeCheck& expected) {
  Check(Parse(input), expected);
}

TEST_F(AXPropertyNodeTest, ParseProperty) {
  // Properties and methods.
  ParseAndCheck("Role", "Role");
  ParseAndCheck("ChildAt(3)", "ChildAt(3)");
  ParseAndCheck("Cell(3, 4)", "Cell(3, 4)");
  ParseAndCheck("Volume(3, 4, 5)", "Volume(3, 4, 5)");
  ParseAndCheck("TableFor(CellBy(id))", "TableFor(CellBy(id))");
  ParseAndCheck("A(B(1), 2)", "A(B(1), 2)");
  ParseAndCheck("A(B(1), 2, C(3, 4))", "A(B(1), 2, C(3, 4))");
  ParseAndCheck("[3, 4]", "[](3, 4)");
  ParseAndCheck("Cell([3, 4])", "Cell([](3, 4))");

  // Arguments
  ParseAndCheck("Text({val: 1})", "Text({}(val: 1))");
  ParseAndCheck("Text({lat: 1, len: 1})", "Text({}(lat: 1, len: 1))");
  ParseAndCheck("Text({dict: {val: 1}})", "Text({}(dict: {}(val: 1)))");
  ParseAndCheck("Text({dict: {val: 1}, 3})", "Text({}(dict: {}(val: 1), 3))");
  ParseAndCheck("Text({dict: [1, 2]})", "Text({}(dict: [](1, 2)))");
  ParseAndCheck("Text({dict: ValueFor(1)})", "Text({}(dict: ValueFor(1)))");

  // Nested arguments
  ParseAndCheck("AXIndexForTextMarker(AXTextMarkerForIndex(0))",
                "AXIndexForTextMarker(AXTextMarkerForIndex(0))");

  // Line indexes filter.
  ParseAndCheck(":3,:5;AXDOMClassList", ":3,:5;AXDOMClassList");

  // Context object.
  ParseAndCheck(":1.AXDOMClassList", ":1.AXDOMClassList");
  ParseAndCheck(":1.AXDOMClassList", {":1", "AXDOMClassList"});

  ParseAndCheck(":1.AXIndexForTextMarker(:1.AXTextMarkerForIndex(0))",
                ":1.AXIndexForTextMarker(:1.AXTextMarkerForIndex(0))");
  ParseAndCheck(":1.AXIndexForTextMarker(:1.AXTextMarkerForIndex(0))",
                {":1",
                 "AXIndexForTextMarker",
                 {{":1", "AXTextMarkerForIndex", {{"", "0"}}}}});

  // Wrong format.
  ParseAndCheck("Role(3", "Role(3)");
  ParseAndCheck("TableFor(CellBy(id", "TableFor(CellBy(id))");
  ParseAndCheck("[3, 4", "[](3, 4)");

  // Arguments conversion
  EXPECT_EQ(GetArgumentNode("ChildAt([3])").IsArray(), true);
  EXPECT_EQ(GetArgumentNode("Text({loc: 3, len: 2})").IsDict(), true);
  EXPECT_EQ(GetArgumentNode("ChildAt(3)").IsDict(), false);
  EXPECT_EQ(GetArgumentNode("ChildAt(3)").IsArray(), false);
  EXPECT_EQ(GetArgumentNode("ChildAt(3)").AsInt(), 3);

  // Dict: FindStringKey
  EXPECT_EQ(
      GetArgumentNode("Text({start: :1, dir: forward})").FindStringKey("start"),
      ":1");
  EXPECT_EQ(
      GetArgumentNode("Text({start: :1, dir: forward})").FindStringKey("dir"),
      "forward");
  EXPECT_EQ(GetArgumentNode("Text({start: :1, dir: forward})")
                .FindStringKey("notexists"),
            base::nullopt);

  // Dict: FindIntKey
  EXPECT_EQ(GetArgumentNode("Text({loc: 3, len: 2})").FindIntKey("loc"), 3);
  EXPECT_EQ(GetArgumentNode("Text({loc: 3, len: 2})").FindIntKey("len"), 2);
  EXPECT_EQ(GetArgumentNode("Text({loc: 3, len: 2})").FindIntKey("notexists"),
            base::nullopt);

  // Dict: FindKey
  EXPECT_EQ(GetArgumentNode("Text({anchor: {:1, 0, up}})")
                .FindKey("anchor")
                ->ToString(),
            "anchor: {}(:1, 0, up)");

  EXPECT_EQ(GetArgumentNode("Text({anchor: {:1, 0, up}})").FindKey("focus"),
            nullptr);

  EXPECT_EQ(GetArgumentNode("AXStringForTextMarkerRange({anchor: {:2, 1, "
                            "down}, focus: {:2, 2, down}})")
                .FindKey("anchor")
                ->ToString(),
            "anchor: {}(:2, 1, down)");
}

}  // namespace ui
