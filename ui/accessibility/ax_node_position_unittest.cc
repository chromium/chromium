// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_text_boundary.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

using TestPositionType = std::unique_ptr<AXPosition<AXNodePosition, AXNode>>;
using TestPositionRange = AXRange<AXPosition<AXNodePosition, AXNode>>;

namespace {

constexpr AXNode::AXID ROOT_ID = 1;
constexpr AXNode::AXID BUTTON_ID = 2;
constexpr AXNode::AXID CHECK_BOX_ID = 3;
constexpr AXNode::AXID TEXT_FIELD_ID = 4;
constexpr AXNode::AXID STATIC_TEXT1_ID = 5;
constexpr AXNode::AXID INLINE_BOX1_ID = 6;
constexpr AXNode::AXID LINE_BREAK_ID = 7;
constexpr AXNode::AXID STATIC_TEXT2_ID = 8;
constexpr AXNode::AXID INLINE_BOX2_ID = 9;

// A group of basic and extended characters.
constexpr const wchar_t* kGraphemeClusters[] = {
    // The English word "hey" consisting of four ASCII characters.
    L"h",
    L"e",
    L"y",
    // A Hindi word (which means "Hindi") consisting of two Devanagari
    // grapheme clusters.
    L"\x0939\x093F",
    L"\x0928\x094D\x0926\x0940",
    // A Thai word (which means "feel") consisting of three Thai grapheme
    // clusters.
    L"\x0E23\x0E39\x0E49",
    L"\x0E2A\x0E36",
    L"\x0E01",
};

class AXPositionTest : public testing::Test {
 public:
  static const char* TEXT_VALUE;

  AXPositionTest() = default;
  ~AXPositionTest() override = default;

 protected:
  void SetUp() override;
  void TearDown() override;

  AXTree* CreateMultipageDocument(AXNodeData& root_data,
                                  AXNodeData& page_1_data,
                                  AXNodeData& page_1_text_data,
                                  AXNodeData& page_2_data,
                                  AXNodeData& page_2_text_data,
                                  AXNodeData& page_3_data,
                                  AXNodeData& page_3_text_data) {
    AXNodePosition::SetTree(nullptr);

    root_data.id = 1;
    root_data.role = ax::mojom::Role::kDocument;

    page_1_data.id = 2;
    page_1_data.role = ax::mojom::Role::kRegion;
    page_1_data.AddBoolAttribute(
        ax::mojom::BoolAttribute::kIsPageBreakingObject, true);

    page_1_text_data.id = 3;
    page_1_text_data.role = ax::mojom::Role::kStaticText;
    page_1_text_data.SetName("some text on page 1");
    page_1_text_data.AddBoolAttribute(
        ax::mojom::BoolAttribute::kIsLineBreakingObject, true);
    page_1_data.child_ids = {3};

    page_2_data.id = 4;
    page_2_data.role = ax::mojom::Role::kRegion;
    page_2_data.AddBoolAttribute(
        ax::mojom::BoolAttribute::kIsPageBreakingObject, true);

    page_2_text_data.id = 5;
    page_2_text_data.role = ax::mojom::Role::kStaticText;
    page_2_text_data.SetName("some text on page 2");
    page_2_text_data.AddIntAttribute(
        ax::mojom::IntAttribute::kTextStyle,
        static_cast<int32_t>(ax::mojom::TextStyle::kBold));
    page_2_data.child_ids = {5};

    page_3_data.id = 6;
    page_3_data.role = ax::mojom::Role::kRegion;
    page_3_data.AddBoolAttribute(
        ax::mojom::BoolAttribute::kIsPageBreakingObject, true);

    page_3_text_data.id = 7;
    page_3_text_data.role = ax::mojom::Role::kStaticText;
    page_3_text_data.SetName("some more text on page 3");
    page_3_data.child_ids = {7};

    root_data.child_ids = {2, 4, 6};

    AXTreeUpdate update;
    AXTreeData tree_data;
    AXTreeID new_id = AXTreeID::CreateNewAXTreeID();
    tree_data.tree_id = new_id;
    update.tree_data = tree_data;
    update.has_tree_data = true;
    update.root_id = root_data.id;
    update.nodes = {root_data,       page_1_data,      page_1_text_data,
                    page_2_data,     page_2_text_data, page_3_data,
                    page_3_text_data};

    return new AXTree(update);
  }

  AXTree* CreateMultipageDocument(AXNodeData& root_data,
                                  AXNodeData& page_1_data,
                                  AXNodeData& page_1_text_data,
                                  AXNodeData& page_2_data,
                                  AXNodeData& page_2_text_data,
                                  AXNodeData& page_3_data,
                                  AXNodeData& page_3_text_data) const {
    root_data.id = 1;
    root_data.role = ax::mojom::Role::kDocument;

    page_1_data.id = 2;
    page_1_data.role = ax::mojom::Role::kRegion;
    page_1_data.AddBoolAttribute(
        ax::mojom::BoolAttribute::kIsPageBreakingObject, true);

    page_1_text_data.id = 3;
    page_1_text_data.role = ax::mojom::Role::kStaticText;
    page_1_text_data.SetName("some text on page 1");
    page_1_text_data.AddBoolAttribute(
        ax::mojom::BoolAttribute::kIsLineBreakingObject, true);
    page_1_data.child_ids = {3};

    page_2_data.id = 4;
    page_2_data.role = ax::mojom::Role::kRegion;
    page_2_data.AddBoolAttribute(
        ax::mojom::BoolAttribute::kIsPageBreakingObject, true);

    page_2_text_data.id = 5;
    page_2_text_data.role = ax::mojom::Role::kStaticText;
    page_2_text_data.SetName("some text on page 2");
    page_2_text_data.AddIntAttribute(
        ax::mojom::IntAttribute::kTextStyle,
        static_cast<int32_t>(ax::mojom::TextStyle::kBold));
    page_2_data.child_ids = {5};

    page_3_data.id = 6;
    page_3_data.role = ax::mojom::Role::kRegion;
    page_3_data.AddBoolAttribute(
        ax::mojom::BoolAttribute::kIsPageBreakingObject, true);

    page_3_text_data.id = 7;
    page_3_text_data.role = ax::mojom::Role::kStaticText;
    page_3_text_data.SetName("some more text on page 3");
    page_3_data.child_ids = {7};

    root_data.child_ids = {2, 4, 6};

    AXTreeUpdate update;
    AXTreeData tree_data;
    AXTreeID new_id = AXTreeID::CreateNewAXTreeID();
    tree_data.tree_id = new_id;
    update.tree_data = tree_data;
    update.has_tree_data = true;
    update.root_id = root_data.id;
    update.nodes = {root_data,       page_1_data,      page_1_text_data,
                    page_2_data,     page_2_text_data, page_3_data,
                    page_3_text_data};

    return new AXTree(update);
  }

  // Creates a document with three static text objects each containing text in a
  // different language.
  std::unique_ptr<AXTree> CreateMultilingualDocument(
      std::vector<int>* text_offsets) {
    EXPECT_NE(nullptr, text_offsets);
    text_offsets->push_back(0);

    base::string16 english_text;
    for (int i = 0; i < 3; ++i) {
      base::string16 grapheme = base::WideToUTF16(kGraphemeClusters[i]);
      EXPECT_EQ(1u, grapheme.length())
          << "All English characters should be one UTF16 code unit in length.";
      text_offsets->push_back(text_offsets->back() + int{grapheme.length()});
      english_text.append(grapheme);
    }

    base::string16 hindi_text;
    for (int i = 3; i < 5; ++i) {
      base::string16 grapheme = base::WideToUTF16(kGraphemeClusters[i]);
      EXPECT_LE(2u, grapheme.length()) << "All Hindi characters should be two "
                                          "or more UTF16 code units in length.";
      text_offsets->push_back(text_offsets->back() + int{grapheme.length()});
      hindi_text.append(grapheme);
    }

    base::string16 thai_text;
    for (int i = 5; i < 8; ++i) {
      base::string16 grapheme = base::WideToUTF16(kGraphemeClusters[i]);
      EXPECT_LT(0u, grapheme.length())
          << "One of the Thai characters should be one UTF16 code unit, "
             "whilst others should be two or more.";
      text_offsets->push_back(text_offsets->back() + int{grapheme.length()});
      thai_text.append(grapheme);
    }

    AXNodeData root_data;
    root_data.id = 1;
    root_data.role = ax::mojom::Role::kRootWebArea;

    AXNodeData text_data1;
    text_data1.id = 2;
    text_data1.role = ax::mojom::Role::kStaticText;
    text_data1.SetName(english_text);

    AXNodeData text_data2;
    text_data2.id = 3;
    text_data2.role = ax::mojom::Role::kStaticText;
    text_data2.SetName(hindi_text);

    AXNodeData text_data3;
    text_data3.id = 4;
    text_data3.role = ax::mojom::Role::kStaticText;
    text_data3.SetName(thai_text);

    root_data.child_ids = {text_data1.id, text_data2.id, text_data3.id};
    return CreateAXTree({root_data, text_data1, text_data2, text_data3});
  }

  void AssertTextLengthEquals(const AXTree* tree,
                              AXNode::AXID node_id,
                              int expected_text_length) {
    TestPositionType text_position = AXNodePosition::CreateTextPosition(
        tree->data().tree_id, node_id, 0 /* text_offset */,
        ax::mojom::TextAffinity::kUpstream);
    ASSERT_NE(nullptr, text_position);
    ASSERT_TRUE(text_position->IsTextPosition());
    ASSERT_EQ(expected_text_length, text_position->MaxTextOffset());
    ASSERT_EQ(expected_text_length,
              static_cast<int>(text_position->GetText().length()));
  }

  // Creates a new AXTree from a vector of nodes.
  // Assumes the first node in the vector is the root.
  std::unique_ptr<AXTree> CreateAXTree(const std::vector<AXNodeData>& nodes) {
    AXTreeUpdate update;
    AXTreeData tree_data;
    tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
    update.tree_data = tree_data;
    update.has_tree_data = true;
    update.root_id = nodes[0].id;
    update.nodes = nodes;

    tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
    update.tree_data = tree_data;
    std::unique_ptr<AXTree> tree = std::make_unique<AXTree>(update);
    AXNodePosition::SetTree(tree.get());
    return tree;
  }

  AXNodeData root_;
  AXNodeData button_;
  AXNodeData check_box_;
  AXNodeData text_field_;
  AXNodeData static_text1_;
  AXNodeData line_break_;
  AXNodeData static_text2_;
  AXNodeData inline_box1_;
  AXNodeData inline_box2_;

  AXTree tree_;

  DISALLOW_COPY_AND_ASSIGN(AXPositionTest);
};

// Used by AXPositionExpandToEnclosingTextBoundaryTestWithParam.
//
// Every test instance starts from a pre-determined position and calls the
// ExpandToEnclosingTextBoundary method with the arguments provided in this
// struct.
struct ExpandToEnclosingTextBoundaryTestParam {
  ExpandToEnclosingTextBoundaryTestParam() = default;

  // Required by GTest framework.
  ExpandToEnclosingTextBoundaryTestParam(
      const ExpandToEnclosingTextBoundaryTestParam& other) = default;
  ExpandToEnclosingTextBoundaryTestParam& operator=(
      const ExpandToEnclosingTextBoundaryTestParam& other) = default;

  ~ExpandToEnclosingTextBoundaryTestParam() = default;

  // The text boundary to expand to.
  AXTextBoundary boundary;

  // Determines how to expand to the enclosing range when the starting position
  // is already at a text boundary.
  AXRangeExpandBehavior expand_behavior;

  // The text position that should be returned for the anchor of the range.
  std::string expected_anchor_position;

  // The text position that should be returned for the focus of the range.
  std::string expected_focus_position;
};

// This is a fixture for a set of parameterized tests that test the
// |ExpandToEnclosingTextBoundary| method with all possible input arguments.
class AXPositionExpandToEnclosingTextBoundaryTestWithParam
    : public AXPositionTest,
      public testing::WithParamInterface<
          ExpandToEnclosingTextBoundaryTestParam> {
 public:
  AXPositionExpandToEnclosingTextBoundaryTestWithParam() = default;
  ~AXPositionExpandToEnclosingTextBoundaryTestWithParam() override = default;

  DISALLOW_COPY_AND_ASSIGN(
      AXPositionExpandToEnclosingTextBoundaryTestWithParam);
};

// Used by AXPositionCreatePositionAtTextBoundaryTestWithParam.
//
// Every test instance starts from a pre-determined position and calls the
// CreatePositionAtTextBoundary method with the arguments provided in this
// struct.
struct CreatePositionAtTextBoundaryTestParam {
  CreatePositionAtTextBoundaryTestParam() = default;

  // Required by GTest framework.
  CreatePositionAtTextBoundaryTestParam(
      const CreatePositionAtTextBoundaryTestParam& other) = default;
  CreatePositionAtTextBoundaryTestParam& operator=(
      const CreatePositionAtTextBoundaryTestParam& other) = default;

  ~CreatePositionAtTextBoundaryTestParam() = default;

  // The text boundary to move to.
  AXTextBoundary boundary;

  // The direction to move to.
  AXTextBoundaryDirection direction;

  // What to do when the starting position is already at a text boundary, or
  // when the movement operation will cause us to cross the starting object's
  // boundary.
  AXBoundaryBehavior boundary_behavior;

  // The text position that should be returned, if the method was called on a
  // text position instance.
  std::string expected_text_position;
};

// This is a fixture for a set of parameterized tests that test the
// |CreatePositionAtTextBoundary| method with all possible input arguments.
class AXPositionCreatePositionAtTextBoundaryTestWithParam
    : public AXPositionTest,
      public testing::WithParamInterface<
          CreatePositionAtTextBoundaryTestParam> {
 public:
  AXPositionCreatePositionAtTextBoundaryTestWithParam() = default;
  ~AXPositionCreatePositionAtTextBoundaryTestWithParam() override = default;

  DISALLOW_COPY_AND_ASSIGN(AXPositionCreatePositionAtTextBoundaryTestWithParam);
};

// Used by |AXPositionTextNavigationTestWithParam|.
//
// The test starts from a pre-determined position and repeats a text navigation
// operation, such as |CreateNextWordStartPosition|, until it runs out of
// expectations.
struct TextNavigationTestParam {
  TextNavigationTestParam() = default;

  // Required by GTest framework.
  TextNavigationTestParam(const TextNavigationTestParam& other) = default;
  TextNavigationTestParam& operator=(const TextNavigationTestParam& other) =
      default;

  ~TextNavigationTestParam() = default;

  // Stores the method that should be called repeatedly by the test to create
  // the next position.
  base::RepeatingCallback<TestPositionType(const TestPositionType&)> TestMethod;

  // The node at which the test should start.
  AXNode::AXID start_node_id;

  // The text offset at which the test should start.
  int start_offset;

  // A list of positions that should be returned from the method being tested,
  // in stringified form.
  std::vector<std::string> expectations;
};

// This is a fixture for a set of parameterized tests that ensure that text
// navigation operations, such as |CreateNextWordStartPosition|, work properly.
//
// Starting from a given position, test instances call a given text navigation
// method repeatedly and compare the return values to a set of expectations.
//
// TODO(nektar): Only text positions are tested for now.
class AXPositionTextNavigationTestWithParam
    : public AXPositionTest,
      public testing::WithParamInterface<TextNavigationTestParam> {
 public:
  AXPositionTextNavigationTestWithParam() = default;
  ~AXPositionTextNavigationTestWithParam() override = default;

  DISALLOW_COPY_AND_ASSIGN(AXPositionTextNavigationTestWithParam);
};

const char* AXPositionTest::TEXT_VALUE = "Line 1\nLine 2";

void AXPositionTest::SetUp() {
  // root_
  //  |
  //  +------------+-----------+
  //  |            |           |
  // button_  check_box_  text_field_
  //                           |
  //               +-----------+------------+
  //               |           |            |
  //        static_text1_  line_break_   static_text2_
  //               |                        |
  //        inline_box1_                 inline_box2_

  root_.id = ROOT_ID;
  button_.id = BUTTON_ID;
  check_box_.id = CHECK_BOX_ID;
  text_field_.id = TEXT_FIELD_ID;
  static_text1_.id = STATIC_TEXT1_ID;
  inline_box1_.id = INLINE_BOX1_ID;
  line_break_.id = LINE_BREAK_ID;
  static_text2_.id = STATIC_TEXT2_ID;
  inline_box2_.id = INLINE_BOX2_ID;

  root_.role = ax::mojom::Role::kRootWebArea;
  root_.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject, true);

  button_.role = ax::mojom::Role::kButton;
  button_.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                           true);
  button_.SetHasPopup(ax::mojom::HasPopup::kMenu);
  button_.SetName("Button");
  button_.relative_bounds.bounds = gfx::RectF(20, 20, 200, 30);
  root_.child_ids.push_back(button_.id);

  check_box_.role = ax::mojom::Role::kCheckBox;
  check_box_.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                              true);
  check_box_.SetCheckedState(ax::mojom::CheckedState::kTrue);
  check_box_.SetName("Check box");
  check_box_.relative_bounds.bounds = gfx::RectF(20, 50, 200, 30);
  root_.child_ids.push_back(check_box_.id);

  text_field_.role = ax::mojom::Role::kTextField;
  text_field_.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                               true);
  text_field_.AddState(ax::mojom::State::kEditable);
  text_field_.SetValue(TEXT_VALUE);
  text_field_.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCachedLineStarts,
      std::vector<int32_t>{0, 7});
  text_field_.child_ids.push_back(static_text1_.id);
  text_field_.child_ids.push_back(line_break_.id);
  text_field_.child_ids.push_back(static_text2_.id);
  root_.child_ids.push_back(text_field_.id);

  static_text1_.role = ax::mojom::Role::kStaticText;
  static_text1_.AddState(ax::mojom::State::kEditable);
  static_text1_.SetName("Line 1");
  static_text1_.child_ids.push_back(inline_box1_.id);
  static_text1_.AddIntAttribute(
      ax::mojom::IntAttribute::kTextStyle,
      static_cast<int32_t>(ax::mojom::TextStyle::kBold));

  inline_box1_.role = ax::mojom::Role::kInlineTextBox;
  inline_box1_.AddState(ax::mojom::State::kEditable);
  inline_box1_.SetName("Line 1");
  inline_box1_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                   std::vector<int32_t>{0, 5});
  inline_box1_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                   std::vector<int32_t>{4, 6});
  inline_box1_.AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                               line_break_.id);

  line_break_.role = ax::mojom::Role::kLineBreak;
  line_break_.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                               true);
  line_break_.AddState(ax::mojom::State::kEditable);
  line_break_.SetName("\n");
  line_break_.AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                              inline_box1_.id);

  static_text2_.role = ax::mojom::Role::kStaticText;
  static_text2_.AddState(ax::mojom::State::kEditable);
  static_text2_.SetName("Line 2");
  static_text2_.child_ids.push_back(inline_box2_.id);
  static_text2_.AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize, 1.0f);

  inline_box2_.role = ax::mojom::Role::kInlineTextBox;
  inline_box2_.AddState(ax::mojom::State::kEditable);
  inline_box2_.SetName("Line 2");
  inline_box2_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                   std::vector<int32_t>{0, 5});
  inline_box2_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                   std::vector<int32_t>{4, 6});

  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.push_back(root_);
  initial_state.nodes.push_back(button_);
  initial_state.nodes.push_back(check_box_);
  initial_state.nodes.push_back(text_field_);
  initial_state.nodes.push_back(static_text1_);
  initial_state.nodes.push_back(inline_box1_);
  initial_state.nodes.push_back(line_break_);
  initial_state.nodes.push_back(static_text2_);
  initial_state.nodes.push_back(inline_box2_);
  initial_state.has_tree_data = true;
  initial_state.tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  initial_state.tree_data.title = "Dialog title";
  AXSerializableTree src_tree(initial_state);

  std::unique_ptr<AXTreeSource<const AXNode*, AXNodeData, AXTreeData>>
      tree_source(src_tree.CreateTreeSource());
  AXTreeSerializer<const AXNode*, AXNodeData, AXTreeData> serializer(
      tree_source.get());
  AXTreeUpdate update;
  serializer.SerializeChanges(src_tree.root(), &update);
  ASSERT_TRUE(tree_.Unserialize(update));
  AXNodePosition::SetTree(&tree_);
}

void AXPositionTest::TearDown() {
  AXNodePosition::SetTree(nullptr);
}

}  // namespace

TEST_F(AXPositionTest, Clone) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType copy_position = null_position->Clone();
  ASSERT_NE(nullptr, copy_position);
  EXPECT_TRUE(copy_position->IsNullPosition());

  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  copy_position = tree_position->Clone();
  ASSERT_NE(nullptr, copy_position);
  EXPECT_TRUE(copy_position->IsTreePosition());
  EXPECT_EQ(root_.id, copy_position->anchor_id());
  EXPECT_EQ(1, copy_position->child_index());
  EXPECT_EQ(AXNodePosition::INVALID_OFFSET, copy_position->text_offset());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, tree_position);
  copy_position = tree_position->Clone();
  ASSERT_NE(nullptr, copy_position);
  EXPECT_TRUE(copy_position->IsTreePosition());
  EXPECT_EQ(root_.id, copy_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, copy_position->child_index());
  EXPECT_EQ(AXNodePosition::INVALID_OFFSET, copy_position->text_offset());

  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  copy_position = text_position->Clone();
  ASSERT_NE(nullptr, copy_position);
  EXPECT_TRUE(copy_position->IsTextPosition());
  EXPECT_EQ(text_field_.id, copy_position->anchor_id());
  EXPECT_EQ(0, copy_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kUpstream, copy_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  copy_position = text_position->Clone();
  ASSERT_NE(nullptr, copy_position);
  EXPECT_TRUE(copy_position->IsTextPosition());
  EXPECT_EQ(text_field_.id, copy_position->anchor_id());
  EXPECT_EQ(0, copy_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, copy_position->affinity());
  EXPECT_EQ(AXNodePosition::INVALID_INDEX, copy_position->child_index());
}

TEST_F(AXPositionTest, Serialize) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType copy_position =
      AXNodePosition::Unserialize(null_position->Serialize());
  ASSERT_NE(nullptr, copy_position);
  EXPECT_TRUE(copy_position->IsNullPosition());

  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  copy_position = AXNodePosition::Unserialize(tree_position->Serialize());
  ASSERT_NE(nullptr, copy_position);
  EXPECT_TRUE(copy_position->IsTreePosition());
  EXPECT_EQ(root_.id, copy_position->anchor_id());
  EXPECT_EQ(1, copy_position->child_index());
  EXPECT_EQ(AXNodePosition::INVALID_OFFSET, copy_position->text_offset());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, tree_position);
  copy_position = AXNodePosition::Unserialize(tree_position->Serialize());
  ASSERT_NE(nullptr, copy_position);
  EXPECT_TRUE(copy_position->IsTreePosition());
  EXPECT_EQ(root_.id, copy_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, copy_position->child_index());
  EXPECT_EQ(AXNodePosition::INVALID_OFFSET, copy_position->text_offset());

  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  copy_position = AXNodePosition::Unserialize(text_position->Serialize());
  ASSERT_NE(nullptr, copy_position);
  EXPECT_TRUE(copy_position->IsTextPosition());
  EXPECT_EQ(text_field_.id, copy_position->anchor_id());
  EXPECT_EQ(0, copy_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kUpstream, copy_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  copy_position = AXNodePosition::Unserialize(text_position->Serialize());
  ASSERT_NE(nullptr, copy_position);
  EXPECT_TRUE(copy_position->IsTextPosition());
  EXPECT_EQ(text_field_.id, copy_position->anchor_id());
  EXPECT_EQ(0, copy_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, copy_position->affinity());
  EXPECT_EQ(AXNodePosition::INVALID_INDEX, copy_position->child_index());
}

TEST_F(AXPositionTest, GetTextFromNullPosition) {
  TestPositionType text_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsNullPosition());
  ASSERT_EQ(base::WideToUTF16(L""), text_position->GetText());
}

TEST_F(AXPositionTest, GetTextFromRoot) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(base::WideToUTF16(L"Line 1\nLine 2"), text_position->GetText());
}

TEST_F(AXPositionTest, GetTextFromButton) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, button_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(base::WideToUTF16(L""), text_position->GetText());
}

TEST_F(AXPositionTest, GetTextFromCheckbox) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(base::WideToUTF16(L""), text_position->GetText());
}

TEST_F(AXPositionTest, GetTextFromTextField) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(base::WideToUTF16(L"Line 1\nLine 2"), text_position->GetText());
}

TEST_F(AXPositionTest, GetTextFromStaticText) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, static_text1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(base::WideToUTF16(L"Line 1"), text_position->GetText());
}

TEST_F(AXPositionTest, GetTextFromInlineTextBox) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(base::WideToUTF16(L"Line 1"), text_position->GetText());
}

TEST_F(AXPositionTest, GetTextFromLineBreak) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(base::WideToUTF16(L"\n"), text_position->GetText());
}

TEST_F(AXPositionTest, GetMaxTextOffsetFromNullPosition) {
  TestPositionType text_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsNullPosition());
  ASSERT_EQ(AXNodePosition::INVALID_OFFSET, text_position->MaxTextOffset());
}

TEST_F(AXPositionTest, GetMaxTextOffsetFromRoot) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(13, text_position->MaxTextOffset());
}

TEST_F(AXPositionTest, GetMaxTextOffsetFromButton) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, button_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(0, text_position->MaxTextOffset());
}

TEST_F(AXPositionTest, GetMaxTextOffsetFromCheckbox) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(0, text_position->MaxTextOffset());
}

TEST_F(AXPositionTest, GetMaxTextOffsetFromTextfield) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(13, text_position->MaxTextOffset());
}

TEST_F(AXPositionTest, GetMaxTextOffsetFromStaticText) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, static_text1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(6, text_position->MaxTextOffset());
}

TEST_F(AXPositionTest, GetMaxTextOffsetFromInlineTextBox) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(6, text_position->MaxTextOffset());
}

TEST_F(AXPositionTest, GetMaxTextOffsetFromLineBreak) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  ASSERT_EQ(1, text_position->MaxTextOffset());
}

TEST_F(AXPositionTest, GetMaxTextOffsetUpdate) {
  AXNodePosition::SetTree(nullptr);

  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;

  AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  AXNodeData more_text_data;
  more_text_data.id = 3;
  more_text_data.role = ax::mojom::Role::kStaticText;
  more_text_data.SetName("more text");

  root_data.child_ids = {2, 3};

  std::unique_ptr<AXTree> new_tree =
      CreateAXTree({root_data, text_data, more_text_data});

  AssertTextLengthEquals(new_tree.get(), text_data.id, 9);
  AssertTextLengthEquals(new_tree.get(), root_data.id, 18);

  text_data.SetName("Adjusted line 1");
  new_tree = CreateAXTree({root_data, text_data, more_text_data});
  AssertTextLengthEquals(new_tree.get(), text_data.id, 15);
  AssertTextLengthEquals(new_tree.get(), root_data.id, 24);

  // Value should override name
  text_data.SetValue("Value should override name");
  new_tree = CreateAXTree({root_data, text_data, more_text_data});
  AssertTextLengthEquals(new_tree.get(), text_data.id, 26);
  AssertTextLengthEquals(new_tree.get(), root_data.id, 35);

  // An empty value should fall back to name
  text_data.SetValue("");
  new_tree = CreateAXTree({root_data, text_data, more_text_data});
  AssertTextLengthEquals(new_tree.get(), text_data.id, 15);
  AssertTextLengthEquals(new_tree.get(), root_data.id, 24);
}

TEST_F(AXPositionTest, AtStartOfAnchorWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  EXPECT_FALSE(null_position->AtStartOfAnchor());
}

TEST_F(AXPositionTest, AtStartOfAnchorWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  EXPECT_TRUE(tree_position->AtStartOfAnchor());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  EXPECT_FALSE(tree_position->AtStartOfAnchor());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  EXPECT_FALSE(tree_position->AtStartOfAnchor());

  // A "before text" position.
  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, tree_position);
  EXPECT_TRUE(tree_position->AtStartOfAnchor());

  // An "after text" position.
  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  EXPECT_FALSE(tree_position->AtStartOfAnchor());
}

TEST_F(AXPositionTest, AtStartOfAnchorWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtStartOfAnchor());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtStartOfAnchor());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtStartOfAnchor());
}

TEST_F(AXPositionTest, AtEndOfAnchorWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  EXPECT_FALSE(null_position->AtEndOfAnchor());
}

TEST_F(AXPositionTest, AtEndOfAnchorWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  EXPECT_TRUE(tree_position->AtEndOfAnchor());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 2 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  EXPECT_FALSE(tree_position->AtEndOfAnchor());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  EXPECT_FALSE(tree_position->AtEndOfAnchor());
}

TEST_F(AXPositionTest, AtEndOfAnchorWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtEndOfAnchor());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtEndOfAnchor());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtEndOfAnchor());
}

TEST_F(AXPositionTest, AtStartOfLineWithTextPosition) {
  // An upstream affinity should not affect the outcome since there is no soft
  // line break.
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtStartOfLine());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtStartOfLine());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtStartOfLine());

  // An "after text" position anchored at the line break should be equivalent to
  // a "before text" position at the start of the next line.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtStartOfLine());

  // An upstream affinity should not affect the outcome since there is no soft
  // line break.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtStartOfLine());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtStartOfLine());
}

TEST_F(AXPositionTest, AtEndOfLineWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtEndOfLine());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtEndOfLine());

  // A "before text" position anchored at the line break should visually be the
  // same as a text position at the end of the previous line.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtEndOfLine());

  // The following position comes after the soft line break, so it should not be
  // marked as the end of the line.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtEndOfLine());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtEndOfLine());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtEndOfLine());
}

TEST_F(AXPositionTest, AtStartOfBlankLine) {
  // Modify the test tree so that the line break will appear on a line of its
  // own, i.e. as creating a blank line.
  inline_box1_.RemoveIntAttribute(ax::mojom::IntAttribute::kNextOnLineId);
  line_break_.RemoveIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId);
  AXTreeUpdate update;
  update.nodes = {inline_box1_, line_break_};
  ASSERT_TRUE(tree_.Unserialize(update));

  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  ASSERT_TRUE(tree_position->IsTreePosition());
  EXPECT_TRUE(tree_position->AtStartOfLine());

  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtStartOfLine());

  // A text position after a blank line should be equivalent to a "before text"
  // position at the line that comes after it.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtStartOfLine());
}

TEST_F(AXPositionTest, AtEndOfBlankLine) {
  // Modify the test tree so that the line break will appear on a line of its
  // own, i.e. as creating a blank line.
  inline_box1_.RemoveIntAttribute(ax::mojom::IntAttribute::kNextOnLineId);
  line_break_.RemoveIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId);
  AXTreeUpdate update;
  update.nodes = {inline_box1_, line_break_};
  ASSERT_TRUE(tree_.Unserialize(update));

  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  ASSERT_TRUE(tree_position->IsTreePosition());
  EXPECT_FALSE(tree_position->AtEndOfLine());

  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtEndOfLine());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtEndOfLine());
}

TEST_F(AXPositionTest, AtStartOfParagraphWithTextPosition) {
  // An upstream affinity should not affect the outcome since there is no soft
  // line break.
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtStartOfParagraph());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtStartOfParagraph());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtStartOfParagraph());

  // An "after text" position anchored at the line break should not be the same
  // as a text position at the start of the next paragraph.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtStartOfParagraph());

  // An upstream affinity should not affect the outcome since there is no soft
  // line break.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtStartOfParagraph());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtStartOfParagraph());
}

TEST_F(AXPositionTest, AtEndOfParagraphWithTextPosition) {
  // End of |inline_box1_| is not the end of paragraph since it's
  // followed by a whitespace-only line breaking object
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtEndOfParagraph());

  // The start of |line_break_| is not the end of paragraph since it's
  // not the end of its anchor.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtEndOfParagraph());

  // The end of |line_break_| is the end of paragraph since it's
  // a line breaking object without additional trailing whitespace.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtEndOfParagraph());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_FALSE(text_position->AtEndOfParagraph());

  // The end of |inline_box2_| is the end of paragraph since it's
  // followed by the end of document.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  EXPECT_TRUE(text_position->AtEndOfParagraph());
}

TEST_F(AXPositionTest, ParagraphEdgesWithPreservedNewLine) {
  // This test updates the tree structure to test a specific edge case -
  // At{Start|End}OfParagraph when an ancestor position can resolve to a
  // preserved newline descendant.
  // ++1 kRootWebArea isLineBreakingObject
  // ++++2 kStaticText
  // ++++++3 kInlineTextBox "some text"
  // ++++4 kGenericContainer isLineBreakingObject
  // ++++++5 kStaticText
  // ++++++++6 kInlineTextBox "\n" isLineBreakingObject
  // ++++++++7 kInlineTextBox "more text"
  AXNodePosition::SetTree(nullptr);

  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                             true);

  AXNodeData static_text_data_1;
  static_text_data_1.id = 2;
  static_text_data_1.role = ax::mojom::Role::kStaticText;
  static_text_data_1.SetName("some text");

  AXNodeData some_text_data;
  some_text_data.id = 3;
  some_text_data.role = ax::mojom::Role::kInlineTextBox;
  some_text_data.SetName("some text");

  AXNodeData container_data;
  container_data.id = 4;
  container_data.role = ax::mojom::Role::kGenericContainer;
  container_data.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);

  AXNodeData static_text_data_2;
  static_text_data_2.id = 5;
  static_text_data_2.role = ax::mojom::Role::kStaticText;
  static_text_data_2.SetName("\nmore text");

  AXNodeData preserved_newline_data;
  preserved_newline_data.id = 6;
  preserved_newline_data.role = ax::mojom::Role::kInlineTextBox;
  preserved_newline_data.SetName("\n");
  preserved_newline_data.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);

  AXNodeData more_text_data;
  more_text_data.id = 7;
  more_text_data.role = ax::mojom::Role::kInlineTextBox;
  more_text_data.SetName("more text");

  static_text_data_1.child_ids = {3};
  container_data.child_ids = {5};
  static_text_data_2.child_ids = {6, 7};
  root_data.child_ids = {2, 4};

  std::unique_ptr<AXTree> new_tree = CreateAXTree(
      {root_data, static_text_data_1, some_text_data, container_data,
       static_text_data_2, preserved_newline_data, more_text_data});

  TestPositionType text_position1 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, root_data.id, 8 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position1->AtEndOfParagraph());
  EXPECT_FALSE(text_position1->AtStartOfParagraph());

  TestPositionType text_position2 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, root_data.id, 9 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position2->AtEndOfParagraph());
  EXPECT_FALSE(text_position2->AtStartOfParagraph());

  TestPositionType text_position3 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, root_data.id, 9 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  EXPECT_FALSE(text_position3->AtEndOfParagraph());
  EXPECT_FALSE(text_position3->AtStartOfParagraph());

  TestPositionType text_position4 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, root_data.id, 10 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position4->AtEndOfParagraph());
  EXPECT_TRUE(text_position4->AtStartOfParagraph());

  TestPositionType text_position5 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, root_data.id, 10 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  EXPECT_TRUE(text_position5->AtEndOfParagraph());
  EXPECT_FALSE(text_position5->AtStartOfParagraph());

  TestPositionType text_position6 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, container_data.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position6->AtEndOfParagraph());
  EXPECT_FALSE(text_position6->AtStartOfParagraph());

  TestPositionType text_position7 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, container_data.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position7->AtEndOfParagraph());
  EXPECT_TRUE(text_position7->AtStartOfParagraph());

  TestPositionType text_position8 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, container_data.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  EXPECT_TRUE(text_position8->AtEndOfParagraph());
  EXPECT_FALSE(text_position8->AtStartOfParagraph());

  TestPositionType text_position9 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, static_text_data_2.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position9->AtEndOfParagraph());
  EXPECT_TRUE(text_position9->AtStartOfParagraph());

  TestPositionType text_position10 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, static_text_data_2.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  EXPECT_TRUE(text_position10->AtEndOfParagraph());
  EXPECT_FALSE(text_position10->AtStartOfParagraph());

  TestPositionType text_position11 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, preserved_newline_data.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position11->AtEndOfParagraph());
  EXPECT_FALSE(text_position11->AtStartOfParagraph());

  TestPositionType text_position12 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, preserved_newline_data.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_TRUE(text_position12->AtEndOfParagraph());
  EXPECT_FALSE(text_position12->AtStartOfParagraph());

  TestPositionType text_position13 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, more_text_data.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position13->AtEndOfParagraph());
  EXPECT_TRUE(text_position13->AtStartOfParagraph());

  TestPositionType text_position14 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, more_text_data.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position14->AtEndOfParagraph());
  EXPECT_FALSE(text_position14->AtStartOfParagraph());
}

TEST_F(
    AXPositionTest,
    PreviousParagraphEndStopAtAnchorBoundaryWithConsecutiveParentChildLineBreakingObjects) {
  // This test updates the tree structure to test a specific edge case -
  // CreatePreviousParagraphEndPosition(), stopping at an anchor boundary,
  // with consecutive parent-child line breaking objects.
  // ++1 rootWebArea
  // ++++2 staticText name="first"
  // ++++3 genericContainer isLineBreakingObject
  // ++++++4 genericContainer isLineBreakingObject
  // ++++++5 staticText name="second"
  AXNodePosition::SetTree(nullptr);

  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;

  AXNodeData static_text_data_a;
  static_text_data_a.id = 2;
  static_text_data_a.role = ax::mojom::Role::kStaticText;
  static_text_data_a.SetName("first");

  AXNodeData container_data_a;
  container_data_a.id = 3;
  container_data_a.role = ax::mojom::Role::kGenericContainer;
  container_data_a.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);

  AXNodeData container_data_b;
  container_data_b.id = 4;
  container_data_b.role = ax::mojom::Role::kGenericContainer;
  container_data_b.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);

  AXNodeData static_text_data_b;
  static_text_data_b.id = 5;
  static_text_data_b.role = ax::mojom::Role::kStaticText;
  static_text_data_b.SetName("second");

  root_data.child_ids = {static_text_data_a.id, container_data_a.id};
  container_data_a.child_ids = {container_data_b.id, static_text_data_b.id};

  std::unique_ptr<AXTree> new_tree =
      CreateAXTree({root_data, static_text_data_a, container_data_a,
                    container_data_b, static_text_data_b});

  TestPositionType test_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, root_data.id, 11 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);

  test_position = test_position->CreatePreviousParagraphEndPosition(
      ui::AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(root_data.id, test_position->anchor_id());
  EXPECT_EQ(5, test_position->text_offset());
}

TEST_F(AXPositionTest,
       AtStartOrEndOfParagraphWithLeadingAndTrailingDocumentWhitespace) {
  // This test updates the tree structure to test a specific edge case -
  // At{Start|End}OfParagraph when an ancestor position can resolve to a
  // preserved newline descendant.
  // ++1 kRootWebArea isLineBreakingObject
  // ++++2 kGenericContainer isLineBreakingObject
  // ++++++3 kStaticText
  // ++++++++4 kInlineTextBox "\n" isLineBreakingObject
  // ++++5 kGenericContainer isLineBreakingObject
  // ++++++6 kStaticText
  // ++++++++7 kInlineTextBox "some"
  // ++++++++8 kInlineTextBox " "
  // ++++++++9 kInlineTextBox "text"
  // ++++10 kGenericContainer isLineBreakingObject
  // ++++++11 kStaticText
  // ++++++++12 kInlineTextBox "\n" isLineBreakingObject
  AXNodePosition::SetTree(nullptr);

  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                             true);

  AXNodeData container_data_a;
  container_data_a.id = 2;
  container_data_a.role = ax::mojom::Role::kGenericContainer;
  container_data_a.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);

  AXNodeData static_text_data_a;
  static_text_data_a.id = 3;
  static_text_data_a.role = ax::mojom::Role::kStaticText;
  static_text_data_a.SetName("\n");

  AXNodeData inline_text_data_a;
  inline_text_data_a.id = 4;
  inline_text_data_a.role = ax::mojom::Role::kInlineTextBox;
  inline_text_data_a.SetName("\n");
  inline_text_data_a.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);

  AXNodeData container_data_b;
  container_data_b.id = 5;
  container_data_b.role = ax::mojom::Role::kGenericContainer;
  container_data_b.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);

  AXNodeData static_text_data_b;
  static_text_data_b.id = 6;
  static_text_data_b.role = ax::mojom::Role::kStaticText;
  static_text_data_b.SetName("some text");

  AXNodeData inline_text_data_b_1;
  inline_text_data_b_1.id = 7;
  inline_text_data_b_1.role = ax::mojom::Role::kInlineTextBox;
  inline_text_data_b_1.SetName("some");

  AXNodeData inline_text_data_b_2;
  inline_text_data_b_2.id = 8;
  inline_text_data_b_2.role = ax::mojom::Role::kInlineTextBox;
  inline_text_data_b_2.SetName(" ");

  AXNodeData inline_text_data_b_3;
  inline_text_data_b_3.id = 9;
  inline_text_data_b_3.role = ax::mojom::Role::kInlineTextBox;
  inline_text_data_b_3.SetName("text");

  AXNodeData container_data_c;
  container_data_c.id = 10;
  container_data_c.role = ax::mojom::Role::kGenericContainer;
  container_data_c.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);

  AXNodeData static_text_data_c;
  static_text_data_c.id = 11;
  static_text_data_c.role = ax::mojom::Role::kStaticText;
  static_text_data_c.SetName("\n");

  AXNodeData inline_text_data_c;
  inline_text_data_c.id = 12;
  inline_text_data_c.role = ax::mojom::Role::kInlineTextBox;
  inline_text_data_c.SetName("\n");
  inline_text_data_c.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);

  root_data.child_ids = {container_data_a.id, container_data_b.id,
                         container_data_c.id};
  container_data_a.child_ids = {static_text_data_a.id};
  static_text_data_a.child_ids = {inline_text_data_a.id};
  container_data_b.child_ids = {static_text_data_b.id};
  static_text_data_b.child_ids = {inline_text_data_b_1.id,
                                  inline_text_data_b_2.id,
                                  inline_text_data_b_3.id};
  container_data_c.child_ids = {static_text_data_c.id};
  static_text_data_c.child_ids = {inline_text_data_c.id};

  std::unique_ptr<AXTree> new_tree = CreateAXTree(
      {root_data, container_data_a, container_data_b, container_data_c,
       static_text_data_a, static_text_data_b, static_text_data_c,
       inline_text_data_a, inline_text_data_b_1, inline_text_data_b_2,
       inline_text_data_b_3, inline_text_data_c});

  TestPositionType text_position1 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_a.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position1->AtEndOfParagraph());
  EXPECT_TRUE(text_position1->AtStartOfParagraph());

  TestPositionType text_position2 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_a.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_TRUE(text_position2->AtEndOfParagraph());
  EXPECT_FALSE(text_position2->AtStartOfParagraph());

  TestPositionType text_position3 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_b_1.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position3->AtEndOfParagraph());
  EXPECT_TRUE(text_position3->AtStartOfParagraph());

  TestPositionType text_position4 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_b_1.id, 4 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position4->AtEndOfParagraph());
  EXPECT_FALSE(text_position4->AtStartOfParagraph());

  TestPositionType text_position5 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_b_2.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position5->AtEndOfParagraph());
  EXPECT_FALSE(text_position5->AtStartOfParagraph());

  TestPositionType text_position6 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_b_2.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position6->AtEndOfParagraph());
  EXPECT_FALSE(text_position6->AtStartOfParagraph());

  TestPositionType text_position7 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_b_3.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position7->AtEndOfParagraph());
  EXPECT_FALSE(text_position7->AtStartOfParagraph());

  TestPositionType text_position8 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_b_3.id, 4 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position8->AtEndOfParagraph());
  EXPECT_FALSE(text_position8->AtStartOfParagraph());

  TestPositionType text_position9 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_c.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position9->AtEndOfParagraph());
  EXPECT_FALSE(text_position9->AtStartOfParagraph());

  TestPositionType text_position10 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_c.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_TRUE(text_position10->AtEndOfParagraph());
  EXPECT_FALSE(text_position10->AtStartOfParagraph());
}

TEST_F(AXPositionTest, AtStartOrEndOfParagraphWithIgnoredNodes) {
  // This test updates the tree structure to test a specific edge case -
  // At{Start|End}OfParagraph when there are ignored nodes present near
  // a paragraph boundary.
  // ++1 kRootWebArea isLineBreakingObject
  // ++++2 kGenericContainer ignored
  // ++++++3 kStaticText ignored
  // ++++++++4 kInlineTextBox "ignored text" ignored
  // ++++5 kGenericContainer
  // ++++++6 kStaticText
  // ++++++++7 kInlineTextBox "some"
  // ++++++++8 kInlineTextBox " "
  // ++++++++9 kInlineTextBox "text"
  // ++++10 kGenericContainer ignored
  // ++++++11 kStaticText ignored
  // ++++++++12 kInlineTextBox "ignored text" ignored
  AXNodePosition::SetTree(nullptr);

  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;
  root_data.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                             true);

  AXNodeData container_data_a;
  container_data_a.id = 2;
  container_data_a.role = ax::mojom::Role::kGenericContainer;
  container_data_a.AddState(ax::mojom::State::kIgnored);

  AXNodeData static_text_data_a;
  static_text_data_a.id = 3;
  static_text_data_a.role = ax::mojom::Role::kStaticText;
  static_text_data_a.SetName("ignored text");
  static_text_data_a.AddState(ax::mojom::State::kIgnored);

  AXNodeData inline_text_data_a;
  inline_text_data_a.id = 4;
  inline_text_data_a.role = ax::mojom::Role::kInlineTextBox;
  inline_text_data_a.SetName("ignored text");
  inline_text_data_a.AddState(ax::mojom::State::kIgnored);

  AXNodeData container_data_b;
  container_data_b.id = 5;
  container_data_b.role = ax::mojom::Role::kGenericContainer;

  AXNodeData static_text_data_b;
  static_text_data_b.id = 6;
  static_text_data_b.role = ax::mojom::Role::kStaticText;
  static_text_data_b.SetName("some text");

  AXNodeData inline_text_data_b_1;
  inline_text_data_b_1.id = 7;
  inline_text_data_b_1.role = ax::mojom::Role::kInlineTextBox;
  inline_text_data_b_1.SetName("some");

  AXNodeData inline_text_data_b_2;
  inline_text_data_b_2.id = 8;
  inline_text_data_b_2.role = ax::mojom::Role::kInlineTextBox;
  inline_text_data_b_2.SetName(" ");

  AXNodeData inline_text_data_b_3;
  inline_text_data_b_3.id = 9;
  inline_text_data_b_3.role = ax::mojom::Role::kInlineTextBox;
  inline_text_data_b_3.SetName("text");

  AXNodeData container_data_c;
  container_data_c.id = 10;
  container_data_c.role = ax::mojom::Role::kGenericContainer;
  container_data_c.AddState(ax::mojom::State::kIgnored);

  AXNodeData static_text_data_c;
  static_text_data_c.id = 11;
  static_text_data_c.role = ax::mojom::Role::kStaticText;
  static_text_data_c.SetName("ignored text");
  static_text_data_c.AddState(ax::mojom::State::kIgnored);

  AXNodeData inline_text_data_c;
  inline_text_data_c.id = 12;
  inline_text_data_c.role = ax::mojom::Role::kInlineTextBox;
  inline_text_data_c.SetName("ignored text");
  inline_text_data_c.AddState(ax::mojom::State::kIgnored);

  root_data.child_ids = {container_data_a.id, container_data_b.id,
                         container_data_c.id};
  container_data_a.child_ids = {static_text_data_a.id};
  static_text_data_a.child_ids = {inline_text_data_a.id};
  container_data_b.child_ids = {static_text_data_b.id};
  static_text_data_b.child_ids = {inline_text_data_b_1.id,
                                  inline_text_data_b_2.id,
                                  inline_text_data_b_3.id};
  container_data_c.child_ids = {static_text_data_c.id};
  static_text_data_c.child_ids = {inline_text_data_c.id};

  std::unique_ptr<AXTree> new_tree = CreateAXTree(
      {root_data, container_data_a, container_data_b, container_data_c,
       static_text_data_a, static_text_data_b, static_text_data_c,
       inline_text_data_a, inline_text_data_b_1, inline_text_data_b_2,
       inline_text_data_b_3, inline_text_data_c});

  TestPositionType text_position1 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_a.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position1->AtEndOfParagraph());
  EXPECT_FALSE(text_position1->AtStartOfParagraph());

  TestPositionType text_position2 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_a.id, 12 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position2->AtEndOfParagraph());
  EXPECT_FALSE(text_position2->AtStartOfParagraph());

  TestPositionType text_position3 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_b_1.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position3->AtEndOfParagraph());
  EXPECT_TRUE(text_position3->AtStartOfParagraph());

  TestPositionType text_position4 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_b_1.id, 4 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position4->AtEndOfParagraph());
  EXPECT_FALSE(text_position4->AtStartOfParagraph());

  TestPositionType text_position5 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_b_2.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position5->AtEndOfParagraph());
  EXPECT_FALSE(text_position5->AtStartOfParagraph());

  TestPositionType text_position6 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_b_2.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position6->AtEndOfParagraph());
  EXPECT_FALSE(text_position6->AtStartOfParagraph());

  TestPositionType text_position7 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_b_3.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position7->AtEndOfParagraph());
  EXPECT_FALSE(text_position7->AtStartOfParagraph());

  TestPositionType text_position8 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_b_3.id, 4 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_TRUE(text_position8->AtEndOfParagraph());
  EXPECT_FALSE(text_position8->AtStartOfParagraph());

  TestPositionType text_position9 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_c.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position9->AtEndOfParagraph());
  EXPECT_FALSE(text_position9->AtStartOfParagraph());

  TestPositionType text_position10 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_text_data_c.id, 12 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_FALSE(text_position10->AtEndOfParagraph());
  EXPECT_FALSE(text_position10->AtStartOfParagraph());
}

TEST_F(AXPositionTest, LowestCommonAncestor) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  // An "after children" position.
  TestPositionType root_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, root_position);
  // A "before text" position.
  TestPositionType button_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, button_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, button_position);
  TestPositionType text_field_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 2 /* child_index */);
  ASSERT_NE(nullptr, text_field_position);
  TestPositionType static_text1_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, static_text1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, static_text1_position);
  TestPositionType static_text2_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, static_text2_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, static_text2_position);
  TestPositionType inline_box1_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, inline_box1_position);
  ASSERT_TRUE(inline_box1_position->IsTextPosition());
  TestPositionType inline_box2_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, inline_box2_position);
  ASSERT_TRUE(inline_box2_position->IsTextPosition());

  TestPositionType test_position =
      root_position->LowestCommonAncestor(*null_position.get());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  test_position = root_position->LowestCommonAncestor(*root_position.get());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  // The child index should be for an "after children" position, i.e. it should
  // be unchanged.
  EXPECT_EQ(3, test_position->child_index());

  test_position =
      button_position->LowestCommonAncestor(*text_field_position.get());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  // The child index should point to the button.
  EXPECT_EQ(0, test_position->child_index());

  test_position =
      static_text2_position->LowestCommonAncestor(*static_text1_position.get());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  // The child index should point to the second static text node.
  EXPECT_EQ(2, test_position->child_index());

  test_position =
      static_text1_position->LowestCommonAncestor(*text_field_position.get());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  // The child index should point to the first static text node.
  EXPECT_EQ(0, test_position->child_index());

  test_position =
      inline_box1_position->LowestCommonAncestor(*inline_box2_position.get());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position =
      inline_box2_position->LowestCommonAncestor(*inline_box1_position.get());
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  // The text offset should point to the second line.
  EXPECT_EQ(7, test_position->text_offset());
}

TEST_F(AXPositionTest, AsTreePositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->AsTreePosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, AsTreePositionWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position = tree_position->AsTreePosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->child_index());
  EXPECT_EQ(AXNodePosition::INVALID_OFFSET, test_position->text_offset());
}

TEST_F(AXPositionTest, AsTreePositionWithTextPosition) {
  // Create a text position pointing to the last character in the text field.
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 12 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->AsTreePosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  // The created tree position should point to the second static text node
  // inside the text field.
  EXPECT_EQ(2, test_position->child_index());
  // But its text offset should be unchanged.
  EXPECT_EQ(12, test_position->text_offset());

  // Test for a "before text" position.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsTreePosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());
  EXPECT_EQ(0, test_position->text_offset());

  // Test for an "after text" position.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsTreePosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());
  EXPECT_EQ(6, test_position->text_offset());
}

TEST_F(AXPositionTest, AsTextPositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->AsTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, AsTextPositionWithTreePosition) {
  // Create a tree position pointing to the line break node inside the text
  // field.
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position = tree_position->AsTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  // The created text position should point to the 6th character inside the text
  // field, i.e. the line break.
  EXPECT_EQ(6, test_position->text_offset());
  // But its child index should be unchanged.
  EXPECT_EQ(1, test_position->child_index());
  // And the affinity cannot be anything other than downstream because we
  // haven't moved up the tree and so there was no opportunity to introduce any
  // ambiguity regarding the new position.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Test for a "before text" position.
  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->AsTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Test for an "after text" position.
  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->AsTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  EXPECT_EQ(0, test_position->child_index());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, AsTextPositionWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->AsTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
  EXPECT_EQ(AXNodePosition::INVALID_INDEX, test_position->child_index());
}

TEST_F(AXPositionTest, AsLeafTextPositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, AsLeafTextPositionWithTreePosition) {
  // Create a tree position pointing to the first static text node inside the
  // text field.
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position = tree_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Create a tree position pointing to the line break node inside the text
  // field.
  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Create a text position pointing to the second static text node inside the
  // text field.
  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 2 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, AsLeafTextPositionWithTextPosition) {
  // Create a text position pointing to the end of the root (an "after text"
  // position).
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 13 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Create a text position on the root, pointing to the line break character
  // inside the text field but with an upstream affinity which will cause the
  // leaf text position to be placed after the text of the first inline text
  // box.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Create a text position pointing to the line break character inside the text
  // field but with an upstream affinity which will cause the leaf text position
  // to be placed after the text of the first inline text box.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Create a text position on the root, pointing to the line break character
  // inside the text field.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Create a text position pointing to the line break character inside the text
  // field.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Create a text position pointing to the offset after the last character in
  // the text field, (an "after text" position).
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 13 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Create a root text position that points to the middle of a leaf text
  // position, should maintain its relative text_offset ("Lin<e> 2")
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 10 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(3, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  // Create a root text position that points to the middle of a leaf text
  // position, should maintain its relative text_offset ("Lin<e> 2")
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 10 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(3, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, AsLeafTextPositionWithTextPositionAndEmptyTextSandwich) {
  // This test updates the tree structure to test a specific edge case -
  // AsLeafTextPosition when there is an empty leaf text node between
  // two non-empty text nodes.
  AXNodePosition::SetTree(nullptr);

  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;

  AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kInlineTextBox;
  text_data.SetName("some text");

  AXNodeData button_data;
  button_data.id = 3;
  button_data.role = ax::mojom::Role::kButton;
  button_data.SetName("");

  AXNodeData more_text_data;
  more_text_data.id = 4;
  more_text_data.role = ax::mojom::Role::kInlineTextBox;
  more_text_data.SetName("more text");

  root_data.child_ids = {text_data.id, button_data.id, more_text_data.id};

  std::unique_ptr<AXTree> new_tree =
      CreateAXTree({root_data, text_data, button_data, more_text_data});
  AXNodePosition::SetTree(new_tree.get());

  // Create a text position on the root pointing to just after the
  // first static text leaf node.
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_data.id, 9 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(button_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_data.id, 9 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPosition();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(text_data.id, test_position->anchor_id());
  EXPECT_EQ(9, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, CreatePositionAtInvalidGraphemeBoundary) {
  std::vector<int> text_offsets;
  std::unique_ptr<AXTree> new_tree = CreateMultilingualDocument(&text_offsets);
  ASSERT_NE(nullptr, new_tree.get());
  ASSERT_NE(nullptr, new_tree->root());

  TestPositionType test_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, new_tree->root()->id(), 4 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(new_tree->root()->id(), test_position->anchor_id());
  EXPECT_EQ(4, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  test_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, new_tree->root()->id(), 10 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(new_tree->root()->id(), test_position->anchor_id());
  EXPECT_EQ(10, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kUpstream, test_position->affinity());
}

TEST_F(AXPositionTest, CreatePositionAtStartOfAnchorWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position =
      null_position->CreatePositionAtStartOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtStartOfAnchorWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position =
      tree_position->CreatePositionAtStartOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreatePositionAtStartOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  // An "after text" position.
  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreatePositionAtStartOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());
}

TEST_F(AXPositionTest, CreatePositionAtStartOfAnchorWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position =
      text_position->CreatePositionAtStartOfAnchor();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->CreatePositionAtStartOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  // Affinity should have been reset to the default value.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, CreatePositionAtEndOfAnchorWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->CreatePositionAtEndOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtEndOfAnchorWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position = tree_position->CreatePositionAtEndOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(3, test_position->child_index());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreatePositionAtEndOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(3, test_position->child_index());
}

TEST_F(AXPositionTest, CreatePositionAtEndOfAnchorWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->CreatePositionAtEndOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->CreatePositionAtEndOfAnchor();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  // Affinity should have been reset to the default value.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, CreatePositionAtPreviousFormatStartWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position =
      null_position->CreatePreviousFormatStartPosition(
          AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = null_position->CreatePreviousFormatStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = null_position->CreatePreviousFormatStartPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtPreviousFormatStartWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, static_text1_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  ASSERT_TRUE(tree_position->IsTreePosition());

  TestPositionType test_position =
      tree_position->CreatePreviousFormatStartPosition(
          AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(static_text1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  test_position = test_position->CreatePreviousFormatStartPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());

  // StopIfAlreadyAtBoundary shouldn't move, since it's already at a boundary.
  test_position = test_position->CreatePreviousFormatStartPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());

  // StopAtLastAnchorBoundary should stop at the start of the document while
  // CrossBoundary should return a null position when crossing it.
  test_position = test_position->CreatePreviousFormatStartPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());

  test_position = test_position->CreatePreviousFormatStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtPreviousFormatStartWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 2 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  TestPositionType test_position =
      text_position->CreatePreviousFormatStartPosition(
          AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  test_position = test_position->CreatePreviousFormatStartPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // StopIfAlreadyAtBoundary shouldn't move, since it's already at a boundary.
  test_position = test_position->CreatePreviousFormatStartPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // StopAtLastAnchorBoundary should stop at the start of the document while
  // CrossBoundary should return a null position when crossing it.
  test_position = test_position->CreatePreviousFormatStartPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreatePreviousFormatStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtNextFormatEndWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = null_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtNextFormatEndWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  ASSERT_TRUE(tree_position->IsTreePosition());

  TestPositionType test_position = tree_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  test_position = test_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  // StopIfAlreadyAtBoundary shouldn't move, since it's already at a boundary.
  test_position = test_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  // StopAtLastAnchorBoundary should stop at the end of the document while
  // CrossBoundary should return a null position when crossing it.
  test_position = test_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  test_position = test_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtNextFormatEndWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  TestPositionType test_position = text_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());

  test_position = test_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());

  // StopIfAlreadyAtBoundary shouldn't move, since it's already at a boundary.
  test_position = test_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());

  // StopAtLastAnchorBoundary should stop at the end of the document while
  // CrossBoundary should return a null position when crossing it.
  test_position = test_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());

  test_position = test_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtFormatBoundaryWithTextPosition) {
  // This test updates the tree structure to test a specific edge case -
  // CreatePositionAtFormatBoundary when text lies at the beginning and end
  // of the AX tree.
  AXNodePosition::SetTree(nullptr);

  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;

  AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  AXNodeData more_text_data;
  more_text_data.id = 3;
  more_text_data.role = ax::mojom::Role::kStaticText;
  more_text_data.SetName("more text");

  root_data.child_ids = {text_data.id, more_text_data.id};

  std::unique_ptr<AXTree> new_tree =
      CreateAXTree({root_data, text_data, more_text_data});
  AXNodePosition::SetTree(new_tree.get());

  // Test CreatePreviousFormatStartPosition at the start of the document.
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_data.id, 8 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  TestPositionType test_position =
      text_position->CreatePreviousFormatStartPosition(
          AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // Test CreateNextFormatEndPosition at the end of the document.
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, more_text_data.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  test_position = text_position->CreateNextFormatEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(more_text_data.id, test_position->anchor_id());
  EXPECT_EQ(9, test_position->text_offset());
  AXNodePosition::SetTree(&tree_);
}

TEST_F(AXPositionTest, CreatePositionAtPageBoundaryWithTextPosition) {
  AXNodeData root_data, page_1_data, page_1_text_data, page_2_data,
      page_2_text_data, page_3_data, page_3_text_data;
  std::unique_ptr<AXTree> new_tree(CreateMultipageDocument(
      root_data, page_1_data, page_1_text_data, page_2_data, page_2_text_data,
      page_3_data, page_3_text_data));
  AXNodePosition::SetTree(new_tree.get());

  // Test CreateNextPageStartPosition at the start of the document.
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, page_1_text_data.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  // StopIfAlreadyAtBoundary shouldn't move at all since it's at a boundary.
  TestPositionType test_position = text_position->CreateNextPageStartPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(page_1_text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = text_position->CreateNextPageStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(page_2_text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = text_position->CreateNextPageStartPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(page_2_text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // Test CreateNextPageEndPosition until the end of document is reached.
  test_position = test_position->CreateNextPageEndPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(page_2_text_data.id, test_position->anchor_id());
  EXPECT_EQ(19, test_position->text_offset());

  test_position = test_position->CreateNextPageEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(page_3_text_data.id, test_position->anchor_id());
  EXPECT_EQ(24, test_position->text_offset());

  test_position = test_position->CreateNextPageEndPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(page_3_text_data.id, test_position->anchor_id());
  EXPECT_EQ(24, test_position->text_offset());

  // StopAtLastAnchorBoundary shouldn't move past the end of the document.
  test_position = test_position->CreateNextPageStartPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(page_3_text_data.id, test_position->anchor_id());
  EXPECT_EQ(24, test_position->text_offset());

  test_position = test_position->CreateNextPageEndPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(page_3_text_data.id, test_position->anchor_id());
  EXPECT_EQ(24, test_position->text_offset());

  // Moving forward past the end should return a null position.
  TestPositionType null_position = test_position->CreateNextPageStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, null_position);
  EXPECT_TRUE(null_position->IsNullPosition());

  null_position = test_position->CreateNextPageEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, null_position);
  EXPECT_TRUE(null_position->IsNullPosition());

  // Now move backward through the document.
  text_position = test_position->CreatePreviousPageEndPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, text_position);
  EXPECT_TRUE(text_position->IsTextPosition());
  EXPECT_EQ(page_3_text_data.id, text_position->anchor_id());
  EXPECT_EQ(24, text_position->text_offset());

  test_position = text_position->CreatePreviousPageEndPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(page_2_text_data.id, test_position->anchor_id());
  EXPECT_EQ(19, test_position->text_offset());

  test_position = text_position->CreatePreviousPageEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(page_2_text_data.id, test_position->anchor_id());
  EXPECT_EQ(19, test_position->text_offset());

  test_position = test_position->CreatePreviousPageStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(page_2_text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreatePreviousPageStartPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(page_1_text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreatePreviousPageStartPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(page_1_text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // StopAtLastAnchorBoundary shouldn't move past the start of the document.
  test_position = test_position->CreatePreviousPageStartPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(page_1_text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreatePreviousPageEndPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(page_1_text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // Moving before the start should return a null position.
  null_position = test_position->CreatePreviousPageStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, null_position);
  EXPECT_TRUE(null_position->IsNullPosition());

  null_position = test_position->CreatePreviousPageEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, null_position);
  EXPECT_TRUE(null_position->IsNullPosition());
  AXNodePosition::SetTree(&tree_);
}

TEST_F(AXPositionTest, CreatePositionAtPageBoundaryWithTreePosition) {
  AXNodeData root_data, page_1_data, page_1_text_data, page_2_data,
      page_2_text_data, page_3_data, page_3_text_data;
  std::unique_ptr<AXTree> new_tree(CreateMultipageDocument(
      root_data, page_1_data, page_1_text_data, page_2_data, page_2_text_data,
      page_3_data, page_3_text_data));
  AXNodePosition::SetTree(new_tree.get());

  // Test CreateNextPageStartPosition at the start of the document.
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      new_tree->data().tree_id, page_1_data.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  ASSERT_TRUE(tree_position->IsTreePosition());

  // StopIfAlreadyAtBoundary shouldn't move at all since it's at a boundary.
  TestPositionType test_position = tree_position->CreateNextPageStartPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(page_1_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  test_position = tree_position->CreateNextPageStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(page_2_text_data.id, test_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());

  test_position = tree_position->CreateNextPageStartPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(page_2_text_data.id, test_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());

  // Test CreateNextPageEndPosition until the end of document is reached.
  test_position = tree_position->CreateNextPageEndPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(page_2_text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  test_position = test_position->CreateNextPageEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(page_3_text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  test_position = test_position->CreateNextPageEndPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(page_3_text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  // StopAtLastAnchorBoundary shouldn't move past the end of the document.
  test_position = test_position->CreateNextPageStartPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(page_3_text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  test_position = test_position->CreateNextPageEndPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(page_3_text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  // Moving forward past the end should return a null position.
  TestPositionType null_position = test_position->CreateNextPageStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, null_position);
  EXPECT_TRUE(null_position->IsNullPosition());

  null_position = test_position->CreateNextPageEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, null_position);
  EXPECT_TRUE(null_position->IsNullPosition());

  // Now move backward through the document.
  tree_position = test_position->CreatePreviousPageEndPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, tree_position);
  EXPECT_TRUE(tree_position->IsTreePosition());
  EXPECT_EQ(page_3_text_data.id, tree_position->anchor_id());
  EXPECT_EQ(0, tree_position->child_index());

  test_position = tree_position->CreatePreviousPageEndPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(page_2_text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  test_position = tree_position->CreatePreviousPageEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(page_2_text_data.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->child_index());

  test_position = test_position->CreatePreviousPageStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(page_2_text_data.id, test_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());

  test_position = test_position->CreatePreviousPageStartPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(page_1_text_data.id, test_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());

  test_position = test_position->CreatePreviousPageStartPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(page_1_text_data.id, test_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());

  // StopAtLastAnchorBoundary shouldn't move past the start of the document.
  test_position = test_position->CreatePreviousPageStartPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(page_1_text_data.id, test_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());

  test_position = test_position->CreatePreviousPageEndPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(page_1_text_data.id, test_position->anchor_id());
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());

  // Moving before the start should return a null position.
  null_position = test_position->CreatePreviousPageStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, null_position);
  EXPECT_TRUE(null_position->IsNullPosition());

  null_position = test_position->CreatePreviousPageEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, null_position);
  EXPECT_TRUE(null_position->IsNullPosition());
  AXNodePosition::SetTree(&tree_);
}

TEST_F(AXPositionTest, CreatePagePositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position =
      null_position->CreatePreviousPageStartPosition(
          AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  test_position = null_position->CreateNextPageStartPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  test_position = null_position->CreatePreviousPageEndPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  test_position = null_position->CreatePreviousPageStartPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtStartOfDocumentWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position =
      null_position->CreatePositionAtStartOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePagePositionWithNonPaginatedDocument) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, static_text1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);

  // Non-paginated documents should move to the start of the document for
  // CreatePreviousPageStartPosition (treating the entire document as a single
  // page)
  TestPositionType test_position =
      text_position->CreatePreviousPageStartPosition(
          AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // Since there is no next page, CreateNextPageStartPosition should return a
  // null position
  test_position = text_position->CreateNextPageStartPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  // Since there is no previous page, CreatePreviousPageEndPosition should
  // return a null position
  test_position = text_position->CreatePreviousPageEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  // Since there are no distinct pages, CreateNextPageEndPosition should move
  // to the end of the document, as if it's one large page.
  test_position = text_position->CreateNextPageEndPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());

  // CreatePreviousPageStartPosition should move back to the beginning of the
  // document
  test_position = test_position->CreatePreviousPageStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // Since there's no next page, CreateNextPageStartPosition should return a
  // null position
  test_position = test_position->CreateNextPageStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  // Since there's no previous page, CreatePreviousPageEndPosition should return
  // a null position
  test_position = text_position->CreatePreviousPageEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  // Since there's no previous page, CreatePreviousPageStartPosition should
  // return a null position
  test_position = text_position->CreatePreviousPageStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtStartOfDocumentWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position =
      tree_position->CreatePositionAtStartOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(root_.id, test_position->anchor_id());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreatePositionAtStartOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(root_.id, test_position->anchor_id());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreatePositionAtStartOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(root_.id, test_position->anchor_id());
}

TEST_F(AXPositionTest, CreatePositionAtStartOfDocumentWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  TestPositionType test_position =
      text_position->CreatePositionAtStartOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(root_.id, test_position->anchor_id());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  test_position = text_position->CreatePositionAtStartOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(root_.id, test_position->anchor_id());
  // Affinity should have been reset to the default value.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, CreatePositionAtEndOfDocumentWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position =
      null_position->CreatePositionAtEndOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreatePositionAtEndOfDocumentWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position =
      tree_position->CreatePositionAtEndOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreatePositionAtEndOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreatePositionAtEndOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
}

TEST_F(AXPositionTest, CreatePositionAtEndOfDocumentWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  TestPositionType test_position =
      text_position->CreatePositionAtEndOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  test_position = text_position->CreatePositionAtEndOfDocument();
  EXPECT_NE(nullptr, test_position);
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  // Affinity should have been reset to the default value.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, CreateChildPositionAtWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->CreateChildPositionAt(0);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateChildPositionAtWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 2 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position = tree_position->CreateChildPositionAt(1);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  // Since the anchor is a leaf node, |child_index| should signify that this is
  // a "before text" position.
  EXPECT_EQ(AXNodePosition::BEFORE_TEXT, test_position->child_index());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, button_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreateChildPositionAt(0);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateChildPositionAtWithTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, static_text1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->CreateChildPositionAt(0);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, static_text2_.id, 4 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->CreateChildPositionAt(1);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateParentPositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->CreateParentPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateParentPositionWithTreePosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, check_box_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  TestPositionType test_position = tree_position->CreateParentPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  // |child_index| should point to the check box node.
  EXPECT_EQ(1, test_position->child_index());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, tree_position);
  test_position = tree_position->CreateParentPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateParentPositionWithTextPosition) {
  // Create a position that points at the end of the first line, right after the
  // check box.
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position = text_position->CreateParentPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(root_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  // Since the same text offset in the root could be used to point to the
  // beginning of the second line, affinity should have been adjusted to
  // upstream.
  EXPECT_EQ(ax::mojom::TextAffinity::kUpstream, test_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->CreateParentPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(static_text2_.id, test_position->anchor_id());
  EXPECT_EQ(5, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  test_position = test_position->CreateParentPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  // |text_offset| should point to the same offset on the second line where the
  // static text node position was pointing at.
  EXPECT_EQ(12, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, CreateNextAndPreviousLeafTextPositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->CreateNextLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = null_position->CreatePreviousLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateNextLeafTextPosition) {
  TestPositionType check_box_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, check_box_position);
  TestPositionType test_position =
      check_box_position->CreateNextLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // The text offset on the root points to the button since it is the first
  // available leaf text position, even though it has no text content.
  TestPositionType root_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, root_position);
  ASSERT_TRUE(root_position->IsTextPosition());
  test_position = root_position->CreateNextLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  TestPositionType button_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, button_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, button_position);
  ASSERT_TRUE(button_position->IsTextPosition());
  test_position = button_position->CreateNextLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreateNextLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreateNextLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreateNextLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreateNextLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  TestPositionType text_field_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 2 /* child_index */);
  ASSERT_NE(nullptr, text_field_position);
  test_position = text_field_position->CreateNextLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // The root text position should resolve to its leaf text position,
  // maintaining its text_offset
  TestPositionType root_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 10 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, root_position2);
  ASSERT_TRUE(root_position2->IsTextPosition());
  test_position = root_position2->CreateNextLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(3, test_position->text_offset());
}

TEST_F(AXPositionTest, CreatePreviousLeafTextPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position =
      text_position->CreatePreviousLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // Create a "before text" tree position on the second line of the text box.
  TestPositionType before_text_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box2_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, before_text_position);
  test_position = before_text_position->CreatePreviousLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreatePreviousLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreatePreviousLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreatePreviousLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  test_position = test_position->CreatePreviousLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  TestPositionType text_field_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 2 /* child_index */);
  ASSERT_NE(nullptr, text_field_position);
  test_position = text_field_position->CreatePreviousLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // The text offset on the root points to the text coming from inside the check
  // box.
  TestPositionType check_box_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, check_box_position);
  ASSERT_TRUE(check_box_position->IsTextPosition());
  test_position = check_box_position->CreatePreviousLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(button_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // The root text position should resolve to its leaf text position,
  // maintaining its text_offset
  TestPositionType root_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 10 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, root_position2);
  ASSERT_TRUE(root_position2->IsTextPosition());
  test_position = root_position2->CreatePreviousLeafTextPosition();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, test_position->tree_id());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(3, test_position->text_offset());
}

TEST_F(AXPositionTest, CreateNextLeafTreePosition) {
  TestPositionType root_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  ASSERT_TRUE(root_position->IsTreePosition());

  TestPositionType button_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, button_.id, AXNodePosition::BEFORE_TEXT);
  TestPositionType checkbox_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, check_box_.id, AXNodePosition::BEFORE_TEXT);
  TestPositionType inline_box1_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, AXNodePosition::BEFORE_TEXT);
  TestPositionType line_break_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, line_break_.id, AXNodePosition::BEFORE_TEXT);
  TestPositionType inline_box2_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box2_.id, AXNodePosition::BEFORE_TEXT);

  TestPositionType test_position = root_position->CreateNextLeafTreePosition();
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(*test_position, *button_position);

  test_position = test_position->CreateNextLeafTreePosition();
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(*test_position, *checkbox_position);

  test_position = test_position->CreateNextLeafTreePosition();
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(*test_position, *inline_box1_position);

  test_position = test_position->CreateNextLeafTreePosition();
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(*test_position, *line_break_position);

  test_position = test_position->CreateNextLeafTreePosition();
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(*test_position, *inline_box2_position);

  test_position = test_position->CreateNextLeafTreePosition();
  EXPECT_TRUE(test_position->IsNullPosition());

  TestPositionType root_text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 2 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  EXPECT_TRUE(root_text_position->IsTextPosition());

  test_position = root_text_position->CreateNextLeafTreePosition();
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(*test_position, *inline_box1_position);

  TestPositionType inline_box1_text_position =
      AXNodePosition::CreateTextPosition(tree_.data().tree_id, inline_box1_.id,
                                         2 /* text_offset */,
                                         ax::mojom::TextAffinity::kDownstream);
  EXPECT_TRUE(inline_box1_text_position->IsTextPosition());

  test_position = inline_box1_text_position->CreateNextLeafTreePosition();
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(*test_position, *line_break_position);
}

TEST_F(AXPositionTest, CreatePreviousLeafTreePosition) {
  TestPositionType inline_box2_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box2_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_TRUE(inline_box2_position->IsTreePosition());

  TestPositionType line_break_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, line_break_.id, AXNodePosition::BEFORE_TEXT);
  TestPositionType inline_box1_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, AXNodePosition::BEFORE_TEXT);
  TestPositionType checkbox_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, check_box_.id, AXNodePosition::BEFORE_TEXT);
  TestPositionType button_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, button_.id, AXNodePosition::BEFORE_TEXT);

  TestPositionType test_position =
      inline_box2_position->CreatePreviousLeafTreePosition();
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(*test_position, *line_break_position);

  test_position = test_position->CreatePreviousLeafTreePosition();
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(*test_position, *inline_box1_position);

  test_position = test_position->CreatePreviousLeafTreePosition();
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(*test_position, *checkbox_position);

  test_position = test_position->CreatePreviousLeafTreePosition();
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(*test_position, *button_position);

  test_position = test_position->CreatePreviousLeafTreePosition();
  EXPECT_TRUE(test_position->IsNullPosition());

  TestPositionType inline_box2_text_position =
      AXNodePosition::CreateTextPosition(tree_.data().tree_id, inline_box2_.id,
                                         2 /* text_offset */,
                                         ax::mojom::TextAffinity::kDownstream);
  EXPECT_TRUE(inline_box2_text_position->IsTextPosition());

  test_position = inline_box2_text_position->CreatePreviousLeafTreePosition();
  EXPECT_TRUE(test_position->IsTreePosition());
  EXPECT_EQ(*test_position, *line_break_position);
}

TEST_F(AXPositionTest,
       AsLeafTextPositionBeforeAndAfterCharacterWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  ASSERT_TRUE(null_position->IsNullPosition());
  TestPositionType test_position =
      null_position->AsLeafTextPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = null_position->AsLeafTextPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest,
       AsLeafTextPositionBeforeAndAfterCharacterAtInvalidGraphemeBoundary) {
  std::vector<int> text_offsets;
  std::unique_ptr<AXTree> new_tree = CreateMultilingualDocument(&text_offsets);
  ASSERT_NE(nullptr, new_tree.get());
  ASSERT_NE(nullptr, new_tree->root());

  TestPositionType test_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, new_tree->root()->id(), 4 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  test_position = test_position->AsLeafTextPositionAfterCharacter();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(new_tree->root()->children()[1]->id(), test_position->anchor_id());
  // "text_offset_" should have been adjusted to the next grapheme boundary.
  EXPECT_EQ(2, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  test_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, new_tree->root()->id(), 10 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  test_position = test_position->AsLeafTextPositionBeforeCharacter();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(new_tree->root()->children()[2]->id(), test_position->anchor_id());
  // "text_offset_" should have been adjusted to the previous grapheme boundary.
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  test_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, new_tree->root()->id(), 10 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  test_position = test_position->AsLeafTextPositionBeforeCharacter();
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(new_tree->root()->children()[2]->id(), test_position->anchor_id());
  // The same as above, "text_offset_" should have been adjusted to the previous
  // grapheme boundary.
  EXPECT_EQ(0, test_position->text_offset());
  // An upstream affinity should have had no effect on the outcome and so, it
  // should have been reset in order to provide consistent output from the
  // method regardless of input affinity.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, AsLeafTextPositionBeforeCharacterNoAdjustment) {
  // A text offset that is on the line break right after "Line 1".
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position =
      text_position->AsLeafTextPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  // A text offset that is before the line break right after "Line 1".
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 13 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, static_text1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
}

TEST_F(AXPositionTest, AsLeafTextPositionAfterCharacterNoAdjustment) {
  // A text offset that is after "Line 2".
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 13 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position =
      text_position->AsLeafTextPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());

  // A text offset that is before "Line 2".
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 7 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());

  // A text offset that is on the line break right after "Line 1".
  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 13 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
}

TEST_F(AXPositionTest, AsLeafTextPositionBeforeCharacter) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 3 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position =
      text_position->AsLeafTextPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(3, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 13 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionBeforeCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, AsLeafTextPositionAfterCharacter) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionType test_position =
      text_position->AsLeafTextPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(5, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());
  test_position = text_position->AsLeafTextPositionAfterCharacter();
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateNextAndPreviousCharacterPositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = null_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateNextCharacterPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 4 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  TestPositionType test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(4, test_position->text_offset());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(5, test_position->text_offset());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(5, test_position->text_offset());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(5, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(5, test_position->text_offset());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(6, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());
  // Affinity should have been reset to downstream.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 12 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  test_position = text_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  EXPECT_EQ(13, test_position->text_offset());
  // Affinity should have been reset to downstream.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, CreatePreviousCharacterPosition) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  TestPositionType test_position =
      text_position->CreatePreviousCharacterPosition(
          AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(5, test_position->text_offset());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(4, test_position->text_offset());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(4, test_position->text_offset());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(4, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(1, test_position->text_offset());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box2_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(line_break_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(inline_box1_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopAtAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopAtLastAnchorBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(check_box_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());

  text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  test_position = text_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(text_field_.id, test_position->anchor_id());
  EXPECT_EQ(0, test_position->text_offset());
  // Affinity should have been reset to downstream.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, CreateNextCharacterPositionAtGraphemeBoundary) {
  std::vector<int> text_offsets;
  std::unique_ptr<AXTree> new_tree = CreateMultilingualDocument(&text_offsets);
  ASSERT_NE(nullptr, new_tree.get());
  ASSERT_NE(nullptr, new_tree->root());

  TestPositionType test_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, new_tree->root()->id(), 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, test_position);
  ASSERT_TRUE(test_position->IsTextPosition());

  for (auto iter = (text_offsets.begin() + 1); iter != text_offsets.end();
       ++iter) {
    const int text_offset = *iter;
    test_position = test_position->CreateNextCharacterPosition(
        AXBoundaryBehavior::CrossBoundary);
    ASSERT_NE(nullptr, test_position);
    EXPECT_TRUE(test_position->IsTextPosition());

    testing::Message message;
    message << "Expecting character boundary at " << text_offset << " in\n"
            << *test_position;
    SCOPED_TRACE(message);

    EXPECT_EQ(new_tree->root()->id(), test_position->anchor_id());
    EXPECT_EQ(text_offset, test_position->text_offset());
    EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
  }

  test_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, new_tree->root()->id(), 3 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  test_position = test_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(new_tree->root()->id(), test_position->anchor_id());
  EXPECT_EQ(3, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  test_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, new_tree->root()->id(), 4 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  test_position = test_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(new_tree->root()->id(), test_position->anchor_id());
  EXPECT_EQ(5, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  test_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, new_tree->root()->id(), 9 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  test_position = test_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(new_tree->root()->id(), test_position->anchor_id());
  EXPECT_EQ(9, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kUpstream, test_position->affinity());

  test_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, new_tree->root()->id(), 10 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  test_position = test_position->CreateNextCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(new_tree->root()->id(), test_position->anchor_id());
  EXPECT_EQ(12, test_position->text_offset());
  // Affinity should have been reset to downstream because there was a move.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, CreatePreviousCharacterPositionAtGraphemeBoundary) {
  std::vector<int> text_offsets;
  std::unique_ptr<AXTree> new_tree = CreateMultilingualDocument(&text_offsets);
  ASSERT_NE(nullptr, new_tree.get());
  ASSERT_NE(nullptr, new_tree->root());

  TestPositionType test_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, new_tree->root()->id(),
      text_offsets.back() /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, test_position);
  ASSERT_TRUE(test_position->IsTextPosition());

  for (auto iter = (text_offsets.rbegin() + 1); iter != text_offsets.rend();
       ++iter) {
    const int text_offset = *iter;
    test_position = test_position->CreatePreviousCharacterPosition(
        AXBoundaryBehavior::CrossBoundary);
    ASSERT_NE(nullptr, test_position);
    EXPECT_TRUE(test_position->IsTextPosition());

    testing::Message message;
    message << "Expecting character boundary at " << text_offset << " in\n"
            << *test_position;
    SCOPED_TRACE(message);

    EXPECT_EQ(new_tree->root()->id(), test_position->anchor_id());
    EXPECT_EQ(text_offset, test_position->text_offset());
    EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
  }

  test_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, new_tree->root()->id(), 3 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  test_position = test_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(new_tree->root()->id(), test_position->anchor_id());
  EXPECT_EQ(3, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  test_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, new_tree->root()->id(), 4 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  test_position = test_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(new_tree->root()->id(), test_position->anchor_id());
  EXPECT_EQ(3, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());

  test_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, new_tree->root()->id(), 9 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  test_position = test_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(new_tree->root()->id(), test_position->anchor_id());
  EXPECT_EQ(9, test_position->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kUpstream, test_position->affinity());

  test_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, new_tree->root()->id(), 10 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  test_position = test_position->CreatePreviousCharacterPosition(
      AXBoundaryBehavior::StopIfAlreadyAtBoundary);
  ASSERT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsTextPosition());
  EXPECT_EQ(new_tree->root()->id(), test_position->anchor_id());
  EXPECT_EQ(9, test_position->text_offset());
  // Affinity should have been reset to downstream because there was a move.
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, test_position->affinity());
}

TEST_F(AXPositionTest, ReciprocalCreateNextAndPreviousCharacterPosition) {
  TestPositionType tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  TestPositionType text_position = tree_position->AsTextPosition();
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  size_t next_character_moves = 0;
  while (!text_position->IsNullPosition()) {
    TestPositionType moved_position =
        text_position->CreateNextCharacterPosition(
            AXBoundaryBehavior::CrossBoundary);
    ASSERT_NE(nullptr, moved_position);

    text_position = std::move(moved_position);
    ++next_character_moves;
  }

  tree_position = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, root_.child_ids.size() /* child_index */);
  text_position = tree_position->AsTextPosition();
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  size_t previous_character_moves = 0;
  while (!text_position->IsNullPosition()) {
    TestPositionType moved_position =
        text_position->CreatePreviousCharacterPosition(
            AXBoundaryBehavior::CrossBoundary);
    ASSERT_NE(nullptr, moved_position);

    text_position = std::move(moved_position);
    ++previous_character_moves;
  }

  EXPECT_EQ(next_character_moves, previous_character_moves);
  EXPECT_EQ(strlen(TEXT_VALUE), next_character_moves - 1);
}

TEST_F(AXPositionTest, CreateNextAndPreviousWordStartPositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->CreateNextWordStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = null_position->CreatePreviousWordStartPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, CreateNextAndPreviousWordEndPositionWithNullPosition) {
  TestPositionType null_position = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position);
  TestPositionType test_position = null_position->CreateNextWordEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
  test_position = null_position->CreatePreviousWordEndPosition(
      AXBoundaryBehavior::CrossBoundary);
  EXPECT_NE(nullptr, test_position);
  EXPECT_TRUE(test_position->IsNullPosition());
}

TEST_F(AXPositionTest, OperatorEquals) {
  TestPositionType null_position1 = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position1);
  TestPositionType null_position2 = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position2);
  EXPECT_EQ(*null_position1, *null_position2);

  // Child indices must match.
  TestPositionType button_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, button_position1);
  TestPositionType button_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, button_position2);
  EXPECT_EQ(*button_position1, *button_position2);

  // Both child indices are invalid. It should result in equivalent null
  // positions.
  TestPositionType tree_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 4 /* child_index */);
  ASSERT_NE(nullptr, tree_position1);
  TestPositionType tree_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, AXNodePosition::INVALID_INDEX);
  ASSERT_NE(nullptr, tree_position2);
  EXPECT_EQ(*tree_position1, *tree_position2);

  // An invalid position should not be equivalent to an "after children"
  // position.
  tree_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position1);
  tree_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, -1 /* child_index */);
  ASSERT_NE(nullptr, tree_position2);
  EXPECT_NE(*tree_position1, *tree_position2);

  // Two "after children" positions on the same node should be equivalent.
  tree_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position1);
  tree_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position2);
  EXPECT_EQ(*tree_position1, *tree_position2);

  // Two "before text" positions on the same node should be equivalent.
  tree_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, tree_position1);
  tree_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, tree_position2);
  EXPECT_EQ(*tree_position1, *tree_position2);

  // Both text offsets are invalid. It should result in equivalent null
  // positions.
  TestPositionType text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 15 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsNullPosition());
  TestPositionType text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, -1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsNullPosition());
  EXPECT_EQ(*text_position1, *text_position2);

  text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsTextPosition());
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_EQ(*text_position1, *text_position2);

  // Affinities should not matter.
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_EQ(*text_position1, *text_position2);

  // Text offsets should match.
  text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsTextPosition());
  EXPECT_NE(*text_position1, *text_position2);

  // Two "after text" positions on the same node should be equivalent.
  text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsTextPosition());
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_EQ(*text_position1, *text_position2);

  // Two text positions that are consecutive, one "before text" and one "after
  // text".
  text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsTextPosition());
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_EQ(*text_position1, *text_position2);
}

TEST_F(AXPositionTest, OperatorEqualsSameTextOffsetSameAnchorId) {
  TestPositionType text_position_one = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position_one);
  ASSERT_TRUE(text_position_one->IsTextPosition());

  TestPositionType text_position_two = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position_two);
  ASSERT_TRUE(text_position_two->IsTextPosition());

  ASSERT_TRUE(*text_position_one == *text_position_two);
  ASSERT_TRUE(*text_position_two == *text_position_one);
}

TEST_F(AXPositionTest, OperatorEqualsSameTextOffsetDifferentAnchorIdRoot) {
  TestPositionType text_position_one = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, root_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position_one);
  ASSERT_TRUE(text_position_one->IsTextPosition());

  TestPositionType text_position_two = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position_two);
  ASSERT_TRUE(text_position_two->IsTextPosition());

  ASSERT_TRUE(*text_position_one == *text_position_two);
  ASSERT_TRUE(*text_position_two == *text_position_one);
}

TEST_F(AXPositionTest, OperatorEqualsSameTextOffsetDifferentAnchorIdLeaf) {
  TestPositionType text_position_one = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, button_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position_one);
  ASSERT_TRUE(text_position_one->IsTextPosition());

  TestPositionType text_position_two = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, check_box_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position_two);
  ASSERT_TRUE(text_position_two->IsTextPosition());

  ASSERT_TRUE(*text_position_one == *text_position_two);
  ASSERT_TRUE(*text_position_two == *text_position_one);
}

TEST_F(AXPositionTest, OperatorsLessThanAndGreaterThan) {
  TestPositionType null_position1 = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position1);
  TestPositionType null_position2 = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position2);
  EXPECT_FALSE(*null_position1 < *null_position2);
  EXPECT_FALSE(*null_position1 > *null_position2);

  TestPositionType button_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, button_position1);
  TestPositionType button_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 1 /* child_index */);
  ASSERT_NE(nullptr, button_position2);
  EXPECT_LT(*button_position1, *button_position2);
  EXPECT_GT(*button_position2, *button_position1);

  TestPositionType tree_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 2 /* child_index */);
  ASSERT_NE(nullptr, tree_position1);
  // An "after children" position.
  TestPositionType tree_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position2);
  EXPECT_LT(*tree_position1, *tree_position2);
  EXPECT_GT(*tree_position2, *tree_position1);

  // A "before text" position.
  tree_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, AXNodePosition::BEFORE_TEXT);
  ASSERT_NE(nullptr, tree_position1);
  // An "after text" position.
  tree_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* child_index */);
  ASSERT_NE(nullptr, tree_position2);
  EXPECT_LT(*tree_position1, *tree_position2);
  EXPECT_GT(*tree_position2, *tree_position1);

  // Two text positions that share a common anchor.
  TestPositionType text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 2 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsTextPosition());
  TestPositionType text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_GT(*text_position1, *text_position2);
  EXPECT_LT(*text_position2, *text_position1);

  // Affinities should not matter.
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_GT(*text_position1, *text_position2);
  EXPECT_LT(*text_position2, *text_position1);

  // An "after text" position.
  text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsTextPosition());
  // A "before text" position.
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_GT(*text_position1, *text_position2);
  EXPECT_LT(*text_position2, *text_position1);

  // A text position that is an ancestor of another.
  text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 6 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsTextPosition());
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box1_.id, 5 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_GT(*text_position1, *text_position2);
  EXPECT_LT(*text_position2, *text_position1);

  // Two text positions that share a common ancestor.
  text_position1 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, inline_box2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_TRUE(text_position1->IsTextPosition());
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_GT(*text_position1, *text_position2);
  EXPECT_LT(*text_position2, *text_position1);

  // Two consequtive positions. One "before text" and one "after text".
  text_position2 = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position2);
  ASSERT_TRUE(text_position2->IsTextPosition());
  EXPECT_EQ(*text_position1, *text_position2);
}

TEST_F(AXPositionTest, Swap) {
  TestPositionType null_position1 = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position1);
  TestPositionType null_position2 = AXNodePosition::CreateNullPosition();
  ASSERT_NE(nullptr, null_position2);

  swap(*null_position1, *null_position2);
  EXPECT_TRUE(null_position1->IsNullPosition());
  EXPECT_TRUE(null_position2->IsNullPosition());

  TestPositionType tree_position1 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, root_.id, 2 /* child_index */);
  ASSERT_NE(nullptr, tree_position1);
  TestPositionType tree_position2 = AXNodePosition::CreateTreePosition(
      tree_.data().tree_id, text_field_.id, 3 /* child_index */);
  ASSERT_NE(nullptr, tree_position2);

  swap(*tree_position1, *tree_position2);
  EXPECT_TRUE(tree_position1->IsTreePosition());
  EXPECT_EQ(tree_.data().tree_id, tree_position1->tree_id());
  EXPECT_EQ(text_field_.id, tree_position1->anchor_id());
  EXPECT_EQ(3, tree_position1->child_index());
  EXPECT_TRUE(tree_position1->IsTreePosition());
  EXPECT_EQ(tree_.data().tree_id, tree_position2->tree_id());
  EXPECT_EQ(root_.id, tree_position2->anchor_id());
  EXPECT_EQ(2, tree_position2->child_index());

  swap(*tree_position1, *null_position1);
  EXPECT_TRUE(tree_position1->IsNullPosition());
  EXPECT_TRUE(null_position1->IsTreePosition());
  EXPECT_EQ(tree_.data().tree_id, null_position1->tree_id());
  EXPECT_EQ(text_field_.id, null_position1->anchor_id());
  EXPECT_EQ(3, null_position1->child_index());

  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, line_break_.id, 1 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);

  swap(*text_position, *null_position1);
  EXPECT_TRUE(null_position1->IsTextPosition());
  EXPECT_EQ(tree_.data().tree_id, text_position->tree_id());
  EXPECT_EQ(line_break_.id, null_position1->anchor_id());
  EXPECT_EQ(1, null_position1->text_offset());
  EXPECT_EQ(ax::mojom::TextAffinity::kDownstream, null_position1->affinity());
  EXPECT_TRUE(text_position->IsTreePosition());
  EXPECT_EQ(tree_.data().tree_id, text_position->tree_id());
  EXPECT_EQ(text_field_.id, text_position->anchor_id());
  EXPECT_EQ(3, text_position->child_index());
}

TEST_F(AXPositionTest, CreateNextAnchorPosition) {
  // This test updates the tree structure to test a specific edge case -
  // CreateNextAnchorPosition on an empty text field.
  AXNodePosition::SetTree(nullptr);

  AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kRootWebArea;

  AXNodeData text_data;
  text_data.id = 2;
  text_data.role = ax::mojom::Role::kStaticText;
  text_data.SetName("some text");

  AXNodeData text_field_data;
  text_field_data.id = 3;
  text_field_data.role = ax::mojom::Role::kTextField;

  AXNodeData empty_text_data;
  empty_text_data.id = 4;
  empty_text_data.role = ax::mojom::Role::kStaticText;
  empty_text_data.SetName("");

  AXNodeData more_text_data;
  more_text_data.id = 5;
  more_text_data.role = ax::mojom::Role::kStaticText;
  more_text_data.SetName("more text");

  root_data.child_ids = {text_data.id, text_field_data.id, more_text_data.id};
  text_field_data.child_ids = {empty_text_data.id};

  std::unique_ptr<AXTree> new_tree = CreateAXTree(
      {root_data, text_data, text_field_data, empty_text_data, more_text_data});
  AXNodePosition::SetTree(new_tree.get());

  // Test that CreateNextAnchorPosition will successfully navigate past the
  // empty text field.
  TestPositionType text_position1 = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, text_data.id, 8 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position1);
  ASSERT_FALSE(text_position1->CreateNextAnchorPosition()
                   ->CreateNextAnchorPosition()
                   ->IsNullPosition());
}

TEST_F(AXPositionTest, CreateLinePositionsMultipleAnchorsInSingleLine) {
  // This test updates the tree structure to test a specific edge case -
  // Create next and previous line start/end positions on a single line composed
  // by multiple anchors; only two line boundaries should be resolved: either
  // the start of the "before" text or at the end of "after".
  // ++1 kRootWebArea
  // ++++2 kStaticText
  // ++++++3 kInlineTextBox "before" kNextOnLineId=6
  // ++++4 kGenericContainer
  // ++++++5 kStaticText
  // ++++++++6 kInlineTextBox "inside" kPreviousOnLineId=3 kNextOnLineId=8
  // ++++7 kStaticText
  // ++++++8 kInlineTextBox "after" kPreviousOnLineId=6
  AXNodePosition::SetTree(&tree_);

  AXNodeData root;
  AXNodeData inline_box1;
  AXNodeData inline_box2;
  AXNodeData inline_box3;
  AXNodeData inline_block;
  AXNodeData static_text1;
  AXNodeData static_text2;
  AXNodeData static_text3;

  root.id = 1;
  static_text1.id = 2;
  inline_box1.id = 3;
  inline_block.id = 4;
  static_text2.id = 5;
  inline_box2.id = 6;
  static_text3.id = 7;
  inline_box3.id = 8;

  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {static_text1.id, inline_block.id, static_text3.id};

  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.SetName("before");
  static_text1.child_ids = {inline_box1.id};

  inline_box1.role = ax::mojom::Role::kInlineTextBox;
  inline_box1.SetName("before");
  inline_box1.AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                              inline_box2.id);

  inline_block.role = ax::mojom::Role::kGenericContainer;
  inline_block.child_ids = {static_text2.id};

  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.SetName("inside");
  static_text2.child_ids = {inline_box2.id};

  inline_box2.role = ax::mojom::Role::kInlineTextBox;
  inline_box2.SetName("inside");
  inline_box2.AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                              inline_box1.id);
  inline_box2.AddIntAttribute(ax::mojom::IntAttribute::kNextOnLineId,
                              inline_box3.id);

  static_text3.role = ax::mojom::Role::kStaticText;
  static_text3.SetName("after");
  static_text3.child_ids = {inline_box3.id};

  inline_box3.role = ax::mojom::Role::kInlineTextBox;
  inline_box3.SetName("after");
  inline_box3.AddIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                              inline_box2.id);

  std::unique_ptr<AXTree> new_tree =
      CreateAXTree({root, static_text1, inline_box1, inline_block, static_text2,
                    inline_box2, static_text3, inline_box3});

  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      new_tree->data().tree_id, inline_block.id, 3 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_NE(nullptr, text_position);
  ASSERT_TRUE(text_position->IsTextPosition());

  TestPositionType next_line_start_position =
      text_position->CreateNextLineStartPosition(
          AXBoundaryBehavior::StopAtLastAnchorBoundary);
  ASSERT_NE(nullptr, next_line_start_position);
  EXPECT_TRUE(next_line_start_position->IsTextPosition());
  EXPECT_EQ(inline_box3.id, next_line_start_position->anchor_id());
  EXPECT_EQ(5, next_line_start_position->text_offset());

  TestPositionType previous_line_start_position =
      text_position->CreatePreviousLineStartPosition(
          AXBoundaryBehavior::StopAtLastAnchorBoundary);
  ASSERT_NE(nullptr, previous_line_start_position);
  EXPECT_TRUE(previous_line_start_position->IsTextPosition());
  EXPECT_EQ(inline_box1.id, previous_line_start_position->anchor_id());
  EXPECT_EQ(0, previous_line_start_position->text_offset());

  TestPositionType next_line_end_position =
      text_position->CreateNextLineEndPosition(
          AXBoundaryBehavior::StopAtLastAnchorBoundary);
  ASSERT_NE(nullptr, next_line_end_position);
  EXPECT_TRUE(next_line_end_position->IsTextPosition());
  EXPECT_EQ(inline_box3.id, next_line_end_position->anchor_id());
  EXPECT_EQ(5, next_line_end_position->text_offset());

  TestPositionType previous_line_end_position =
      text_position->CreatePreviousLineEndPosition(
          AXBoundaryBehavior::StopAtLastAnchorBoundary);
  ASSERT_NE(nullptr, previous_line_end_position);
  EXPECT_TRUE(previous_line_end_position->IsTextPosition());
  EXPECT_EQ(inline_box1.id, previous_line_end_position->anchor_id());
  EXPECT_EQ(0, previous_line_end_position->text_offset());
}

//
// Parameterized tests.
//

TEST_P(AXPositionExpandToEnclosingTextBoundaryTestWithParam,
       TextPositionBeforeLine2) {
  // Create a text position right before "Line 2". This should be at the start
  // of many text boundaries, e.g. line, paragraph and word.
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, text_field_.id, 7 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(text_position->IsTextPosition());
  TestPositionRange range = text_position->ExpandToEnclosingTextBoundary(
      GetParam().boundary, GetParam().expand_behavior);
  EXPECT_EQ(GetParam().expected_anchor_position, range.anchor()->ToString());
  EXPECT_EQ(GetParam().expected_focus_position, range.focus()->ToString());
}

TEST_P(AXPositionCreatePositionAtTextBoundaryTestWithParam,
       TextPositionBeforeStaticText) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, static_text2_.id, 0 /* text_offset */,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(text_position->IsTextPosition());
  text_position = text_position->CreatePositionAtTextBoundary(
      GetParam().boundary, GetParam().direction, GetParam().boundary_behavior);
  EXPECT_NE(nullptr, text_position);
  EXPECT_EQ(GetParam().expected_text_position, text_position->ToString());
}

TEST_P(AXPositionTextNavigationTestWithParam,
       TraverseTreeStartingWithAffinityDownstream) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, GetParam().start_node_id, GetParam().start_offset,
      ax::mojom::TextAffinity::kDownstream);
  ASSERT_TRUE(text_position->IsTextPosition());
  for (const std::string& expectation : GetParam().expectations) {
    text_position = GetParam().TestMethod.Run(text_position);
    EXPECT_NE(nullptr, text_position);
    EXPECT_EQ(expectation, text_position->ToString());
  }
}

TEST_P(AXPositionTextNavigationTestWithParam,
       TraverseTreeStartingWithAffinityUpstream) {
  TestPositionType text_position = AXNodePosition::CreateTextPosition(
      tree_.data().tree_id, GetParam().start_node_id, GetParam().start_offset,
      ax::mojom::TextAffinity::kUpstream);
  ASSERT_TRUE(text_position->IsTextPosition());
  for (const std::string& expectation : GetParam().expectations) {
    text_position = GetParam().TestMethod.Run(text_position);
    EXPECT_NE(nullptr, text_position);
    EXPECT_EQ(expectation, text_position->ToString());
  }
}

//
// Instantiations of parameterized tests.
//

INSTANTIATE_TEST_SUITE_P(
    ExpandToEnclosingTextBoundary,
    AXPositionExpandToEnclosingTextBoundaryTestWithParam,
    testing::Values(
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kCharacter, AXRangeExpandBehavior::kLeftFirst,
            "TextPosition anchor_id=4 text_offset=6 affinity=downstream "
            "annotated_text=Line 1<\n>Line 2",
            "TextPosition anchor_id=4 text_offset=7 affinity=downstream "
            "annotated_text=Line 1\n<L>ine 2"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kCharacter, AXRangeExpandBehavior::kRightFirst,
            "TextPosition anchor_id=4 text_offset=7 affinity=downstream "
            "annotated_text=Line 1\n<L>ine 2",
            "TextPosition anchor_id=4 text_offset=8 affinity=downstream "
            "annotated_text=Line 1\nL<i>ne 2"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kFormatChange, AXRangeExpandBehavior::kLeftFirst,
            "TextPosition anchor_id=4 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 1\nLine 2",
            "TextPosition anchor_id=4 text_offset=13 affinity=downstream "
            "annotated_text=Line 1\nLine 2<>"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kFormatChange, AXRangeExpandBehavior::kRightFirst,
            "TextPosition anchor_id=4 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 1\nLine 2",
            "TextPosition anchor_id=4 text_offset=13 affinity=downstream "
            "annotated_text=Line 1\nLine 2<>"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kLineEnd, AXRangeExpandBehavior::kLeftFirst,
            "TextPosition anchor_id=4 text_offset=6 affinity=downstream "
            "annotated_text=Line 1<\n>Line 2",
            "TextPosition anchor_id=4 text_offset=13 affinity=downstream "
            "annotated_text=Line 1\nLine 2<>"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kLineEnd, AXRangeExpandBehavior::kRightFirst,
            "TextPosition anchor_id=4 text_offset=6 affinity=downstream "
            "annotated_text=Line 1<\n>Line 2",
            "TextPosition anchor_id=4 text_offset=13 affinity=downstream "
            "annotated_text=Line 1\nLine 2<>"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kLineStart, AXRangeExpandBehavior::kLeftFirst,
            "TextPosition anchor_id=4 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 1\nLine 2",
            "TextPosition anchor_id=4 text_offset=7 affinity=downstream "
            "annotated_text=Line 1\n<L>ine 2"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kLineStart, AXRangeExpandBehavior::kRightFirst,
            "TextPosition anchor_id=4 text_offset=7 affinity=downstream "
            "annotated_text=Line 1\n<L>ine 2",
            "TextPosition anchor_id=4 text_offset=13 affinity=downstream "
            "annotated_text=Line 1\nLine 2<>"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kLineStartOrEnd, AXRangeExpandBehavior::kLeftFirst,
            "TextPosition anchor_id=4 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 1\nLine 2",
            "TextPosition anchor_id=4 text_offset=6 affinity=downstream "
            "annotated_text=Line 1<\n>Line 2"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kLineStartOrEnd, AXRangeExpandBehavior::kRightFirst,
            "TextPosition anchor_id=4 text_offset=7 affinity=downstream "
            "annotated_text=Line 1\n<L>ine 2",
            "TextPosition anchor_id=4 text_offset=13 affinity=downstream "
            "annotated_text=Line 1\nLine 2<>"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kObject, AXRangeExpandBehavior::kLeftFirst,
            "TextPosition anchor_id=4 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 1\nLine 2",
            "TextPosition anchor_id=4 text_offset=13 affinity=downstream "
            "annotated_text=Line 1\nLine 2<>"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kObject, AXRangeExpandBehavior::kRightFirst,
            "TextPosition anchor_id=4 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 1\nLine 2",
            "TextPosition anchor_id=4 text_offset=13 affinity=downstream "
            "annotated_text=Line 1\nLine 2<>"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kParagraphEnd, AXRangeExpandBehavior::kLeftFirst,
            "TextPosition anchor_id=4 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 1\nLine 2",
            "TextPosition anchor_id=4 text_offset=7 affinity=upstream "
            "annotated_text=Line 1\n<L>ine 2"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kParagraphEnd, AXRangeExpandBehavior::kRightFirst,
            "TextPosition anchor_id=4 text_offset=7 affinity=upstream "
            "annotated_text=Line 1\n<L>ine 2",
            "TextPosition anchor_id=4 text_offset=13 affinity=downstream "
            "annotated_text=Line 1\nLine 2<>"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kParagraphStart, AXRangeExpandBehavior::kLeftFirst,
            "TextPosition anchor_id=4 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 1\nLine 2",
            "TextPosition anchor_id=4 text_offset=7 affinity=downstream "
            "annotated_text=Line 1\n<L>ine 2"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kParagraphStart, AXRangeExpandBehavior::kRightFirst,
            "TextPosition anchor_id=4 text_offset=7 affinity=downstream "
            "annotated_text=Line 1\n<L>ine 2",
            "TextPosition anchor_id=4 text_offset=13 affinity=downstream "
            "annotated_text=Line 1\nLine 2<>"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kParagraphStartOrEnd,
            AXRangeExpandBehavior::kLeftFirst,
            "TextPosition anchor_id=4 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 1\nLine 2",
            "TextPosition anchor_id=4 text_offset=7 affinity=upstream "
            "annotated_text=Line 1\n<L>ine 2"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kParagraphStartOrEnd,
            AXRangeExpandBehavior::kRightFirst,
            "TextPosition anchor_id=4 text_offset=7 affinity=downstream "
            "annotated_text=Line 1\n<L>ine 2",
            "TextPosition anchor_id=4 text_offset=13 affinity=downstream "
            "annotated_text=Line 1\nLine 2<>"},
        // TODO(accessibility): Add tests for sentence boundary.
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kWebPage, AXRangeExpandBehavior::kLeftFirst,
            "TextPosition anchor_id=1 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 1\nLine 2",
            "TextPosition anchor_id=9 text_offset=6 affinity=downstream "
            "annotated_text=Line 2<>"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kWebPage, AXRangeExpandBehavior::kRightFirst,
            "TextPosition anchor_id=1 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 1\nLine 2",
            "TextPosition anchor_id=9 text_offset=6 affinity=downstream "
            "annotated_text=Line 2<>"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kWordEnd, AXRangeExpandBehavior::kLeftFirst,
            "TextPosition anchor_id=4 text_offset=6 affinity=downstream "
            "annotated_text=Line 1<\n>Line 2",
            "TextPosition anchor_id=4 text_offset=11 affinity=downstream "
            "annotated_text=Line 1\nLine< >2"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kWordEnd, AXRangeExpandBehavior::kRightFirst,
            "TextPosition anchor_id=4 text_offset=6 affinity=downstream "
            "annotated_text=Line 1<\n>Line 2",
            "TextPosition anchor_id=4 text_offset=11 affinity=downstream "
            "annotated_text=Line 1\nLine< >2"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kWordStart, AXRangeExpandBehavior::kLeftFirst,
            "TextPosition anchor_id=4 text_offset=5 affinity=downstream "
            "annotated_text=Line <1>\nLine 2",
            "TextPosition anchor_id=4 text_offset=7 affinity=downstream "
            "annotated_text=Line 1\n<L>ine 2"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kWordStart, AXRangeExpandBehavior::kRightFirst,
            "TextPosition anchor_id=4 text_offset=7 affinity=downstream "
            "annotated_text=Line 1\n<L>ine 2",
            "TextPosition anchor_id=4 text_offset=12 affinity=downstream "
            "annotated_text=Line 1\nLine <2>"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kWordStartOrEnd, AXRangeExpandBehavior::kLeftFirst,
            "TextPosition anchor_id=4 text_offset=5 affinity=downstream "
            "annotated_text=Line <1>\nLine 2",
            "TextPosition anchor_id=4 text_offset=6 affinity=downstream "
            "annotated_text=Line 1<\n>Line 2"},
        ExpandToEnclosingTextBoundaryTestParam{
            AXTextBoundary::kWordStartOrEnd, AXRangeExpandBehavior::kRightFirst,
            "TextPosition anchor_id=4 text_offset=7 affinity=downstream "
            "annotated_text=Line 1\n<L>ine 2",
            "TextPosition anchor_id=4 text_offset=11 affinity=downstream "
            "annotated_text=Line 1\nLine< >2"}));

// Only test with AXBoundaryBehavior::CrossBoundary for now.
// TODO(accessibility): Add more tests for other boundary behaviors if needed.
INSTANTIATE_TEST_SUITE_P(
    CreatePositionAtTextBoundary,
    AXPositionCreatePositionAtTextBoundaryTestWithParam,
    testing::Values(
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kCharacter, AXTextBoundaryDirection::kBackwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=7 text_offset=0 affinity=downstream "
            "annotated_text=<\n>"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kCharacter, AXTextBoundaryDirection::kForwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=8 text_offset=1 affinity=downstream "
            "annotated_text=L<i>ne 2"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kFormatChange, AXTextBoundaryDirection::kBackwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=7 text_offset=0 affinity=downstream "
            "annotated_text=<\n>"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kFormatChange, AXTextBoundaryDirection::kForwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=8 text_offset=6 affinity=downstream "
            "annotated_text=Line 2<>"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kLineEnd, AXTextBoundaryDirection::kBackwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=6 text_offset=6 affinity=downstream "
            "annotated_text=Line 1<>"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kLineEnd, AXTextBoundaryDirection::kForwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=8 text_offset=6 affinity=downstream "
            "annotated_text=Line 2<>"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kLineStart, AXTextBoundaryDirection::kBackwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=6 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 1"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kLineStart, AXTextBoundaryDirection::kForwards,
            AXBoundaryBehavior::CrossBoundary, "NullPosition"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kLineStartOrEnd,
            AXTextBoundaryDirection::kBackwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=6 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 1"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kLineStartOrEnd, AXTextBoundaryDirection::kForwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=8 text_offset=6 affinity=downstream "
            "annotated_text=Line 2<>"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kObject, AXTextBoundaryDirection::kBackwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=8 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 2"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kObject, AXTextBoundaryDirection::kForwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=8 text_offset=6 affinity=downstream "
            "annotated_text=Line 2<>"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kParagraphEnd, AXTextBoundaryDirection::kBackwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=3 text_offset=0 affinity=downstream "
            "annotated_text=<>"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kParagraphEnd, AXTextBoundaryDirection::kForwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=8 text_offset=6 affinity=downstream "
            "annotated_text=Line 2<>"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kParagraphStart,
            AXTextBoundaryDirection::kBackwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=6 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 1"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kParagraphStart, AXTextBoundaryDirection::kForwards,
            AXBoundaryBehavior::CrossBoundary, "NullPosition"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kParagraphStartOrEnd,
            AXTextBoundaryDirection::kBackwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=6 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 1"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kParagraphStartOrEnd,
            AXTextBoundaryDirection::kForwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=8 text_offset=6 affinity=downstream "
            "annotated_text=Line 2<>"},
        // TODO(accessibility): Add tests for sentence boundary.
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kWebPage, AXTextBoundaryDirection::kBackwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=1 text_offset=0 affinity=downstream "
            "annotated_text=<L>ine 1\nLine 2"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kWebPage, AXTextBoundaryDirection::kForwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=9 text_offset=6 affinity=downstream "
            "annotated_text=Line 2<>"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kWordEnd, AXTextBoundaryDirection::kBackwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=6 text_offset=6 affinity=downstream "
            "annotated_text=Line 1<>"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kWordEnd, AXTextBoundaryDirection::kForwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=8 text_offset=4 affinity=downstream "
            "annotated_text=Line< >2"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kWordStart, AXTextBoundaryDirection::kBackwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=6 text_offset=5 affinity=downstream "
            "annotated_text=Line <1>"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kWordStart, AXTextBoundaryDirection::kForwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=8 text_offset=5 affinity=downstream "
            "annotated_text=Line <2>"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kWordStartOrEnd,
            AXTextBoundaryDirection::kBackwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=6 text_offset=5 affinity=downstream "
            "annotated_text=Line <1>"},
        CreatePositionAtTextBoundaryTestParam{
            AXTextBoundary::kWordStartOrEnd, AXTextBoundaryDirection::kForwards,
            AXBoundaryBehavior::CrossBoundary,
            "TextPosition anchor_id=8 text_offset=4 affinity=downstream "
            "annotated_text=Line< >2"}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextWordStartPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>\nLine 2",
             "TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=12 "
             "affinity=downstream annotated_text=Line 1\nLine <2>",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>\nLine 2",
             "TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=12 "
             "affinity=downstream annotated_text=Line 1\nLine <2>",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>",
             "TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=9 text_offset=5 "
             "affinity=downstream annotated_text=Line <2>",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=5 "
             "affinity=downstream annotated_text=Line <2>",
             "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextWordStartPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>\nLine 2",
             "TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=12 "
             "affinity=downstream annotated_text=Line 1\nLine <2>",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>\nLine 2",
             "TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=12 "
             "affinity=downstream annotated_text=Line 1\nLine <2>",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>",
             "TextPosition anchor_id=5 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=5 "
             "affinity=downstream annotated_text=Line <2>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextWordStartPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>",
             "TextPosition anchor_id=5 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=5 "
             "affinity=downstream annotated_text=Line <2>",
             "TextPosition anchor_id=9 text_offset=5 "
             "affinity=downstream annotated_text=Line <2>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextWordStartPositionWithBoundaryBehaviorStopAtLastAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>\nLine 2",
             "TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=12 "
             "affinity=downstream annotated_text=Line 1\nLine <2>",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>\nLine 2",
             "TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=12 "
             "affinity=downstream annotated_text=Line 1\nLine <2>",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>",
             "TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=9 text_offset=5 "
             "affinity=downstream annotated_text=Line <2>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=5 "
             "affinity=downstream annotated_text=Line <2>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousWordStartPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at end of root. */,
            {"TextPosition anchor_id=1 text_offset=12 "
             "affinity=downstream annotated_text=Line 1\nLine <2>",
             "TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=12 "
             "affinity=downstream annotated_text=Line 1\nLine <2>",
             "TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>\nLine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            STATIC_TEXT1_ID,
            5 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=6 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>",
             "TextPosition anchor_id=6 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousWordStartPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at end of root. */,
            {"TextPosition anchor_id=1 text_offset=12 "
             "affinity=downstream annotated_text=Line 1\nLine <2>",
             "TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=12 "
             "affinity=downstream annotated_text=Line 1\nLine <2>",
             "TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>\nLine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            5 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousWordStartPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at end of root. */,
            {"TextPosition anchor_id=1 text_offset=12 "
             "affinity=downstream annotated_text=Line 1\nLine <2>",
             "TextPosition anchor_id=1 text_offset=12 "
             "affinity=downstream annotated_text=Line 1\nLine <2>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=12 "
             "affinity=downstream annotated_text=Line 1\nLine <2>",
             "TextPosition anchor_id=4 text_offset=12 "
             "affinity=downstream annotated_text=Line 1\nLine <2>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            STATIC_TEXT1_ID,
            5 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousWordStartPositionWithBoundaryBehaviorStopAtLastAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            ROOT_ID,
            13 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=12 "
             "affinity=downstream annotated_text=Line 1\nLine <2>",
             "TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=12 "
             "affinity=downstream annotated_text=Line 1\nLine <2>",
             "TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>\nLine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            5 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=6 text_offset=5 "
             "affinity=downstream annotated_text=Line <1>",
             "TextPosition anchor_id=6 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextWordEndPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1\nLine 2",
             "TextPosition anchor_id=1 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=1 text_offset=11 "
             "affinity=downstream annotated_text=Line 1\nLine< >2",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1\nLine 2",
             "TextPosition anchor_id=4 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=4 text_offset=11 "
             "affinity=downstream annotated_text=Line 1\nLine< >2",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1",
             "TextPosition anchor_id=5 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>",
             "TextPosition anchor_id=9 text_offset=4 "
             "affinity=downstream annotated_text=Line< >2",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextWordEndPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1\nLine 2",
             "TextPosition anchor_id=1 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=1 text_offset=11 "
             "affinity=downstream annotated_text=Line 1\nLine< >2",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1\nLine 2",
             "TextPosition anchor_id=4 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=4 text_offset=11 "
             "affinity=downstream annotated_text=Line 1\nLine< >2",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1",
             "TextPosition anchor_id=5 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>",
             "TextPosition anchor_id=5 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextWordEndPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1\nLine 2",
             "TextPosition anchor_id=1 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1\nLine 2",
             "TextPosition anchor_id=4 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1",
             "TextPosition anchor_id=5 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=4 "
             "affinity=downstream annotated_text=Line< >2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextWordEndPositionWithBoundaryBehaviorStopAtLastAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1\nLine 2",
             "TextPosition anchor_id=1 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=1 text_offset=11 "
             "affinity=downstream annotated_text=Line 1\nLine< >2",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1\nLine 2",
             "TextPosition anchor_id=4 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=4 text_offset=11 "
             "affinity=downstream annotated_text=Line 1\nLine< >2",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1",
             "TextPosition anchor_id=5 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>",
             "TextPosition anchor_id=9 text_offset=4 "
             "affinity=downstream annotated_text=Line< >2",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextWordEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousWordEndPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at end of root. */,
            {"TextPosition anchor_id=1 text_offset=11 "
             "affinity=downstream annotated_text=Line 1\nLine< >2",
             "TextPosition anchor_id=1 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=1 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1\nLine 2",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=11 "
             "affinity=downstream annotated_text=Line 1\nLine< >2",
             "TextPosition anchor_id=4 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=4 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1\nLine 2",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            STATIC_TEXT1_ID,
            5 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=6 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>",
             "TextPosition anchor_id=6 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1",
             "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousWordEndPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at end of root. */,
            {
                "TextPosition anchor_id=1 text_offset=11 "
                "affinity=downstream annotated_text=Line 1\nLine< >2",
                "TextPosition anchor_id=1 text_offset=6 "
                "affinity=downstream annotated_text=Line 1<\n>Line 2",
                "TextPosition anchor_id=1 text_offset=4 "
                "affinity=downstream annotated_text=Line< >1\nLine 2",
                "TextPosition anchor_id=1 text_offset=0 "
                "affinity=downstream annotated_text=<L>ine 1\nLine 2",
            }},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=11 "
             "affinity=downstream annotated_text=Line 1\nLine< >2",
             "TextPosition anchor_id=4 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=4 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1\nLine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            5 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1",
             "TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousWordEndPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at end of root. */,
            {"TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            STATIC_TEXT1_ID,
            5 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1",
             "TextPosition anchor_id=5 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=4 "
             "affinity=downstream annotated_text=Line< >2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousWordEndPositionWithBoundaryBehaviorStopAtLastAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at end of root. */,
            {"TextPosition anchor_id=1 text_offset=11 "
             "affinity=downstream annotated_text=Line 1\nLine< >2",
             "TextPosition anchor_id=1 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=1 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=11 "
             "affinity=downstream annotated_text=Line 1\nLine< >2",
             "TextPosition anchor_id=4 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=4 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1\nLine 2",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            5 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousWordEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=6 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>",
             "TextPosition anchor_id=6 text_offset=4 "
             "affinity=downstream annotated_text=Line< >1",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextLineStartPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextLineStartPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextLineStartPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextLineStartPositionWithBoundaryBehaviorStopAtLastAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousLineStartPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at the end of root. */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            STATIC_TEXT1_ID,
            5 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=6 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousLineStartPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at the end of root. */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            5 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousLineStartPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at the end of root. */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            STATIC_TEXT1_ID,
            5 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousLineStartPositionWithBoundaryBehaviorStopAtLastAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at the end of root. */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            5 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=6 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextLineEndPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextLineEndPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>",
             "TextPosition anchor_id=5 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextLineEndPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=1 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=4 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>",
             "TextPosition anchor_id=5 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextLineEndPositionWithBoundaryBehaviorStopAtLastAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextLineEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousLineEndPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at end of root. */,
            {"TextPosition anchor_id=1 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            ROOT_ID,
            5 /* text_offset on the last character of "Line 1". */,
            {"NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            TEXT_FIELD_ID,
            5 /* text_offset on the last character of "Line 1". */,
            {"NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=6 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            INLINE_BOX2_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=6 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>",
             "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousLineEndPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at end of root. */,
            {"TextPosition anchor_id=1 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            ROOT_ID,
            5 /* text_offset on the last character of "Line 1". */,
            {"TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            5 /* text_offset on the last character of "Line 1". */,
            {"TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousLineEndPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            ROOT_ID,
            12 /* text_offset one before the end of root. */,
            {"TextPosition anchor_id=1 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=1 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            TEXT_FIELD_ID,
            12 /* text_offset one before the end of text field */,
            {"TextPosition anchor_id=4 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=4 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            INLINE_BOX1_ID,
            2 /* text_offset */,
            {"NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=6 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>",
             "TextPosition anchor_id=6 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            INLINE_BOX2_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=6 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>",
             "TextPosition anchor_id=6 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousLineEndPositionWithBoundaryBehaviorStopAtLastAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at end of root. */,
            {"TextPosition anchor_id=1 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<\n>Line 2",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            ROOT_ID,
            5 /* text_offset on the last character of "Line 1". */,
            {"TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            5 /* text_offset on the last character of "Line 1". */,
            {"TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=6 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousLineEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=6 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextParagraphStartPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextParagraphStartPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextParagraphStartPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextParagraphStartPositionWithBoundaryBehaviorStopAtLastAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousParagraphStartPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at the end of root. */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            STATIC_TEXT1_ID,
            5 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphStartPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=6 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousParagraphStartPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at the end of root. */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            5 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphStartPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousParagraphStartPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at the end of root. */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            STATIC_TEXT1_ID,
            5 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphStartPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousParagraphStartPositionWithBoundaryBehaviorStopAtLastAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at the end of root. */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=downstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            5 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphStartPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=6 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextParagraphEndPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=7 text_offset=1 "
             "affinity=downstream annotated_text=\n<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextParagraphEndPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=5 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>",
             "TextPosition anchor_id=5 text_offset=6 "
             "affinity=downstream annotated_text=Line 1<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextParagraphEndPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=7 text_offset=1 "
             "affinity=downstream annotated_text=\n<>",
             "TextPosition anchor_id=7 text_offset=1 "
             "affinity=downstream annotated_text=\n<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            LINE_BREAK_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=7 text_offset=1 "
             "affinity=downstream annotated_text=\n<>",
             "TextPosition anchor_id=7 text_offset=1 "
             "affinity=downstream annotated_text=\n<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            LINE_BREAK_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=7 text_offset=1 "
             "affinity=downstream annotated_text=\n<>",
             "TextPosition anchor_id=7 text_offset=1 "
             "affinity=downstream annotated_text=\n<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreateNextParagraphEndPositionWithBoundaryBehaviorStopAtLastAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            ROOT_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=1 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>",
             "TextPosition anchor_id=4 text_offset=13 "
             "affinity=downstream annotated_text=Line 1\nLine 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            STATIC_TEXT1_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=7 text_offset=1 "
             "affinity=downstream annotated_text=\n<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreateNextParagraphEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>",
             "TextPosition anchor_id=9 text_offset=6 "
             "affinity=downstream annotated_text=Line 2<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousParagraphEndPositionWithBoundaryBehaviorCrossBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at end of root. */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=3 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            ROOT_ID,
            5 /* text_offset on the last character of "Line 1". */,
            {"TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            TEXT_FIELD_ID,
            5 /* text_offset on the last character of "Line 1". */,
            {"TextPosition anchor_id=3 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=7 text_offset=1 "
             "affinity=downstream annotated_text=\n<>",
             "TextPosition anchor_id=3 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "NullPosition"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::CrossBoundary);
            }),
            INLINE_BOX2_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=3 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "NullPosition"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousParagraphEndPositionWithBoundaryBehaviorStopAtAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at end of root. */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            ROOT_ID,
            5 /* text_offset on the last character of "Line 1". */,
            {"TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            5 /* text_offset on the last character of "Line 1". */,
            {"TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=4 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopAtAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2",
             "TextPosition anchor_id=9 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 2"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousParagraphEndPositionWithBoundaryBehaviorStopIfAlreadyAtBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            ROOT_ID,
            12 /* text_offset one before the end of root. */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            TEXT_FIELD_ID,
            12 /* text_offset one before the end of text field */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=4 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            INLINE_BOX1_ID,
            2 /* text_offset */,
            {"TextPosition anchor_id=3 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=3 text_offset=0 "
             "affinity=downstream annotated_text=<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=7 text_offset=1 "
             "affinity=downstream annotated_text=\n<>",
             "TextPosition anchor_id=7 text_offset=1 "
             "affinity=downstream annotated_text=\n<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            INLINE_BOX2_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=7 text_offset=1 "
             "affinity=downstream annotated_text=\n<>",
             "TextPosition anchor_id=7 text_offset=1 "
             "affinity=downstream annotated_text=\n<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            LINE_BREAK_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=3 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             "TextPosition anchor_id=3 text_offset=0 "
             "affinity=downstream annotated_text=<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopIfAlreadyAtBoundary);
            }),
            LINE_BREAK_ID,
            1 /* text_offset */,
            {"TextPosition anchor_id=7 text_offset=1 "
             "affinity=downstream annotated_text=\n<>",
             "TextPosition anchor_id=7 text_offset=1 "
             "affinity=downstream annotated_text=\n<>"}}));

INSTANTIATE_TEST_SUITE_P(
    CreatePreviousParagraphEndPositionWithBoundaryBehaviorStopAtLastAnchorBoundary,
    AXPositionTextNavigationTestWithParam,
    testing::Values(
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            ROOT_ID,
            13 /* text_offset at end of root. */,
            {"TextPosition anchor_id=1 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            13 /* text_offset at end of text field */,
            {"TextPosition anchor_id=4 text_offset=7 "
             "affinity=upstream annotated_text=Line 1\n<L>ine 2",
             "TextPosition anchor_id=3 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             // The following text position is equal to the previous one.
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            ROOT_ID,
            5 /* text_offset on the last character of "Line 1". */,
            {"TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2",
             "TextPosition anchor_id=1 text_offset=0 "
             "affinity=downstream annotated_text=<L>ine 1\nLine 2"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            TEXT_FIELD_ID,
            5 /* text_offset on the last character of "Line 1". */,
            {"TextPosition anchor_id=3 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             // The following text position is equal to the previous one.
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            4 /* text_offset */,
            {"TextPosition anchor_id=7 text_offset=1 "
             "affinity=downstream annotated_text=\n<>",
             "TextPosition anchor_id=3 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             // The following text position is equal to the previous one.
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}},
        TextNavigationTestParam{
            base::BindRepeating([](const TestPositionType& position) {
              return position->CreatePreviousParagraphEndPosition(
                  AXBoundaryBehavior::StopAtLastAnchorBoundary);
            }),
            INLINE_BOX2_ID,
            0 /* text_offset */,
            {"TextPosition anchor_id=3 text_offset=0 "
             "affinity=downstream annotated_text=<>",
             // The following text position is equal to the previous one.
             "TextPosition anchor_id=2 text_offset=0 "
             "affinity=downstream annotated_text=<>"}}));

}  // namespace ui
