// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_property_node.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

using ui::AXPropertyFilter;
using ui::AXPropertyNode;

namespace ui {

class AXPropertyNodeTest : public testing::Test {
 public:
  AXPropertyNodeTest() = default;

  AXPropertyNodeTest(const AXPropertyNodeTest&) = delete;
  AXPropertyNodeTest& operator=(const AXPropertyNodeTest&) = delete;

  ~AXPropertyNodeTest() override = default;
};

AXPropertyNode Parse(const char* input) {
  AXPropertyFilter filter(input, AXPropertyFilter::ALLOW);
  return AXPropertyNode::From(filter);
}

AXPropertyNode GetArgumentNode(const char* input) {
  auto got = Parse(input);
  if (got.arguments.size() == 0) {
    return AXPropertyNode();
  }
  return std::move(got.arguments[0]);
}

void ParseAndCheck(const char* input, const char* expected) {
  auto got = Parse(input).ToFlatString();
  EXPECT_EQ(got, expected);
}

void ParseAndCheckTree(const char* input, const char* expected) {
  auto got = AXPropertyNode::From(input).ToTreeString();
  EXPECT_EQ(got, expected);
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
  EXPECT_EQ(GetArgumentNode("AXPerformAction(AXPress)").AsString(), "AXPress");
  EXPECT_EQ(GetArgumentNode("AXPerformAction('AXPress')").AsString(),
            "AXPress");

  // Dict: FindStringKey
  EXPECT_EQ(
      GetArgumentNode("Text({start: :1, dir: forward})").FindStringKey("start"),
      ":1");
  EXPECT_EQ(
      GetArgumentNode("Text({start: :1, dir: forward})").FindStringKey("dir"),
      "forward");
  EXPECT_EQ(GetArgumentNode("Text({start: :1, dir: forward})")
                .FindStringKey("notexists"),
            std::nullopt);

  // Dict: FindIntKey
  EXPECT_EQ(GetArgumentNode("Text({loc: 3, len: 2})").FindIntKey("loc"), 3);
  EXPECT_EQ(GetArgumentNode("Text({loc: 3, len: 2})").FindIntKey("len"), 2);
  EXPECT_EQ(GetArgumentNode("Text({loc: 3, len: 2})").FindIntKey("notexists"),
            std::nullopt);

  // `AXPropertyNode::FindKey()`
  EXPECT_EQ(GetArgumentNode("Text({anchor: {:1, 0, up}})")
                .FindKey("anchor")
                ->ToFlatString(),
            "anchor: {}(:1, 0, up)");

  EXPECT_EQ(GetArgumentNode("Text({anchor: {:1, 0, up}})").FindKey("focus"),
            nullptr);

  EXPECT_EQ(GetArgumentNode("AXStringForTextMarkerRange({anchor: {:2, 1, "
                            "down}, focus: {:2, 2, down}})")
                .FindKey("anchor")
                ->ToFlatString(),
            "anchor: {}(:2, 1, down)");
}

TEST_F(AXPropertyNodeTest, CallChains) {
  ParseAndCheckTree("textbox.name", R"~~(textbox.
name)~~");

  ParseAndCheckTree("textbox.parent.name",
                    R"~~(textbox.
parent.
name)~~");

  ParseAndCheckTree("table.rowAt(row.childIndex)",
                    R"~~(table.
rowAt(
  row.
  childIndex
))~~");

  ParseAndCheckTree(":1.AXDOMClassList", R"~~(:1.
AXDOMClassList)~~");

  ParseAndCheckTree(":1.AXIndexForTextMarker(:1.AXTextMarkerForIndex(0))",
                    R"~~(:1.
AXIndexForTextMarker(
  :1.
  AXTextMarkerForIndex(
    0
  )
))~~");

  ParseAndCheckTree("table.cellAt(cell.rowIndex, cell.columnIndex)",
                    R"~~(table.
cellAt(
  cell.
  rowIndex,
  cell.
  columnIndex
))~~");

  ParseAndCheckTree(
      "table.cellAt(table.rowIndexFor(cell), table.columnIndexFor(cell))",
      R"~~(table.
cellAt(
  table.
  rowIndexFor(
    cell
  ),
  table.
  columnIndexFor(
    cell
  )
))~~");
}

TEST_F(AXPropertyNodeTest, CallChains_Array) {
  ParseAndCheckTree("children[3]", R"~~(children.
[](
  3
))~~");

  ParseAndCheckTree("textbox.AXChildren[0]", R"~~(textbox.
AXChildren.
[](
  0
))~~");

  ParseAndCheckTree("textbox.AXChildrenFor(textbox_child)[0]", R"~~(textbox.
AXChildrenFor(
  textbox_child
).
[](
  0
))~~");

  ParseAndCheckTree("get(AXChildren[0])", R"~~(get(
  AXChildren.
  [](
    0
  )
))~~");

  ParseAndCheckTree("textbox.AXChildren[0].AXRole", R"~~(textbox.
AXChildren.
[](
  0
).
AXRole)~~");

  ParseAndCheckTree(
      "textarea.AXTextMarkerRangeForUIElement(textarea.AXChildren[0])",
      R"~~(textarea.
AXTextMarkerRangeForUIElement(
  textarea.
  AXChildren.
  [](
    0
  )
))~~");
}

TEST_F(AXPropertyNodeTest, Variables) {
  // Statement
  ParseAndCheckTree(
      "textmarker_range:= textarea.AXTextMarkerRangeForUIElement(textarea)",
      R"~~(textmarker_range:textarea.
AXTextMarkerRangeForUIElement(
  textarea
))~~");

  // Integer array
  ParseAndCheckTree("var:= [3, 4]",
                    R"~~(var:[](
  3,
  4
))~~");

  // Range dictionary
  ParseAndCheckTree("var:= {loc: 3, len: 2}",
                    R"~~(var:{}(
  loc:3,
  len:2
))~~");

  // TextMarker dictionary
  ParseAndCheckTree("var:= {:2, 2, down}",
                    R"~~(var:{}(
  :2,
  2,
  down
))~~");

  // TextMarker array
  ParseAndCheckTree("var:= [{:2, 2, down}, {:1, 1, up}]",
                    R"~~(var:[](
  {}(
    :2,
    2,
    down
  ),
  {}(
    :1,
    1,
    up
  )
))~~");

  // TextMarkerRange dictionary
  ParseAndCheckTree("var:= {anchor: {:2, 1, down}, focus: {:2, 2, down} }",
                    R"~~(var:{}(
  anchor:{}(
    :2,
    1,
    down
  ),
  focus:{}(
    :2,
    2,
    down
  )
))~~");
}

TEST_F(AXPropertyNodeTest, RValue) {
  ParseAndCheckTree("textarea.AXSelectedTextMarkerRange = {loc: 3, len: 2}",
                    R"~~(textarea.
AXSelectedTextMarkerRange=
{}(
  loc:3,
  len:2
))~~");
}

}  // namespace ui
