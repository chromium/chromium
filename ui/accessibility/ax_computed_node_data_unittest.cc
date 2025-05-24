// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_computed_node_data.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/test_single_ax_tree_manager.h"

namespace ui {

namespace {

class AXComputedNodeDataTest : public ::testing::Test,
                               public TestSingleAXTreeManager {
 public:
  AXComputedNodeDataTest();
  ~AXComputedNodeDataTest() override;
  AXComputedNodeDataTest(const AXComputedNodeDataTest& other) = delete;
  AXComputedNodeDataTest& operator=(const AXComputedNodeDataTest& other) =
      delete;

  void SetUp() override;

 protected:
  // Numbers at the end of variable names indicate their position under the
  // root.
  AXNodeData root_;
  AXNodeData paragraph_0_;
  AXNodeData static_text_0_0_ignored_;
  AXNodeData paragraph_1_ignored_;
  AXNodeData static_text_1_0_;
  AXNodeData paragraph_2_ignored_;
  AXNodeData link_2_0_ignored_;
  AXNodeData static_text_2_0_0_;
  AXNodeData static_text_2_0_1_;

  raw_ptr<AXNode> root_node_;
};

AXComputedNodeDataTest::AXComputedNodeDataTest() = default;
AXComputedNodeDataTest::~AXComputedNodeDataTest() = default;

void AXComputedNodeDataTest::SetUp() {
  // ++kRootWebArea contenteditable
  // ++++kParagraph
  // ++++++kStaticText IGNORED "i"
  // ++++kParagraph IGNORED
  // ++++++kStaticText "t_1"
  // ++++kParagraph IGNORED
  // ++++++kLink IGNORED
  // ++++++++kStaticText "s+t++2...0.  0"
  // ++++++++kStaticText "s t\n2\r0\r\n1"

  root_.id = 1;
  paragraph_0_.id = 2;
  static_text_0_0_ignored_.id = 3;
  paragraph_1_ignored_.id = 4;
  static_text_1_0_.id = 5;
  paragraph_2_ignored_.id = 6;
  link_2_0_ignored_.id = 7;
  static_text_2_0_0_.id = 8;
  static_text_2_0_1_.id = 9;

  root_.role = ax::mojom::Role::kRootWebArea;
  root_.AddBoolAttribute(ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot,
                         true);
  root_.child_ids = {paragraph_0_.id, paragraph_1_ignored_.id,
                     paragraph_2_ignored_.id};

  paragraph_0_.role = ax::mojom::Role::kParagraph;
  paragraph_0_.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                                true);
  paragraph_0_.child_ids = {static_text_0_0_ignored_.id};

  static_text_0_0_ignored_.role = ax::mojom::Role::kStaticText;
  static_text_0_0_ignored_.AddState(ax::mojom::State::kIgnored);
  // Ignored text should not appear anywhere and should not be used to separate
  // words.
  static_text_0_0_ignored_.SetName("i");

  paragraph_1_ignored_.role = ax::mojom::Role::kParagraph;
  paragraph_1_ignored_.AddState(ax::mojom::State::kIgnored);
  paragraph_1_ignored_.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);
  paragraph_1_ignored_.child_ids = {static_text_1_0_.id};

  static_text_1_0_.role = ax::mojom::Role::kStaticText;
  // An underscore should separate words.
  static_text_1_0_.SetName("t_1");

  paragraph_2_ignored_.role = ax::mojom::Role::kParagraph;
  paragraph_2_ignored_.AddState(ax::mojom::State::kIgnored);
  paragraph_2_ignored_.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);
  paragraph_2_ignored_.child_ids = {link_2_0_ignored_.id};

  link_2_0_ignored_.role = ax::mojom::Role::kLink;
  link_2_0_ignored_.AddState(ax::mojom::State::kLinked);
  link_2_0_ignored_.AddState(ax::mojom::State::kIgnored);
  link_2_0_ignored_.child_ids = {static_text_2_0_0_.id, static_text_2_0_1_.id};

  static_text_2_0_0_.role = ax::mojom::Role::kStaticText;
  // A series of punctuation marks, or a stretch of whitespace should separate
  // words.
  static_text_2_0_0_.SetName("s+t++2...0.  0");

  static_text_2_0_1_.role = ax::mojom::Role::kStaticText;
  // Both a carage return as well as a line break should separate lines, but not
  // a space character.
  static_text_2_0_1_.SetName("s t\n2\r0\r\n1");

  AXTreeUpdate initial_state;
  initial_state.root_id = root_.id;
  initial_state.nodes = {root_,
                         paragraph_0_,
                         static_text_0_0_ignored_,
                         paragraph_1_ignored_,
                         static_text_1_0_,
                         paragraph_2_ignored_,
                         link_2_0_ignored_,
                         static_text_2_0_0_,
                         static_text_2_0_1_};
  initial_state.has_tree_data = true;

  AXTreeData tree_data;
  tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  tree_data.title = "Application";
  initial_state.tree_data = tree_data;

  auto tree = std::make_unique<AXTree>();
  ASSERT_TRUE(tree->Unserialize(initial_state)) << tree->error();
  root_node_ = tree->root();
  ASSERT_EQ(root_.id, root_node_->id());

  // `SetTree` is defined in our `TestSingleAXTreeManager` superclass and it
  // passes ownership of the created AXTree to the manager.
  SetTree(std::move(tree));
}

}  // namespace

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::SizeIs;
using ::testing::StrEq;

TEST_F(AXComputedNodeDataTest, UnignoredValues) {
  const AXNode* paragraph_0_node = root_node_->GetChildAtIndex(0);
  const AXNode* static_text_0_0_ignored_node =
      paragraph_0_node->GetChildAtIndex(0);
  const AXNode* paragraph_1_ignored_node = root_node_->GetChildAtIndex(1);
  const AXNode* static_text_1_0_node =
      paragraph_1_ignored_node->GetChildAtIndex(0);
  const AXNode* paragraph_2_ignored_node = root_node_->GetChildAtIndex(2);
  const AXNode* link_2_0_ignored_node =
      paragraph_2_ignored_node->GetChildAtIndex(0);
  const AXNode* static_text_2_0_0_node =
      link_2_0_ignored_node->GetChildAtIndex(0);
  const AXNode* static_text_2_0_1_node =
      link_2_0_ignored_node->GetChildAtIndex(1);

  // Perform the checks twice to ensure that caching returns the same values.
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(
        0,
        root_node_->GetComputedNodeData().GetOrComputeUnignoredIndexInParent());
    EXPECT_EQ(0, paragraph_0_node->GetComputedNodeData()
                     .GetOrComputeUnignoredIndexInParent());
    EXPECT_EQ(1, static_text_1_0_node->GetComputedNodeData()
                     .GetOrComputeUnignoredIndexInParent());
    EXPECT_EQ(2, static_text_2_0_0_node->GetComputedNodeData()
                     .GetOrComputeUnignoredIndexInParent());
    EXPECT_EQ(3, static_text_2_0_1_node->GetComputedNodeData()
                     .GetOrComputeUnignoredIndexInParent());

    EXPECT_EQ(
        kInvalidAXNodeID,
        root_node_->GetComputedNodeData().GetOrComputeUnignoredParentID());
    EXPECT_EQ(nullptr,
              root_node_->GetComputedNodeData().GetOrComputeUnignoredParent());
    EXPECT_FALSE(root_node_->GetComputedNodeData()
                     .GetOrComputeIsDescendantOfPlatformLeaf());
    EXPECT_EQ(root_node_->id(), paragraph_0_node->GetComputedNodeData()
                                    .GetOrComputeUnignoredParentID());
    EXPECT_EQ(
        root_node_,
        paragraph_0_node->GetComputedNodeData().GetOrComputeUnignoredParent());
    EXPECT_FALSE(paragraph_0_node->GetComputedNodeData()
                     .GetOrComputeIsDescendantOfPlatformLeaf());
    EXPECT_EQ(paragraph_0_node->id(),
              static_text_0_0_ignored_node->GetComputedNodeData()
                  .GetOrComputeUnignoredParentID());
    EXPECT_EQ(paragraph_0_node,
              static_text_0_0_ignored_node->GetComputedNodeData()
                  .GetOrComputeUnignoredParent());
    EXPECT_TRUE(static_text_0_0_ignored_node->GetComputedNodeData()
                    .GetOrComputeIsDescendantOfPlatformLeaf());
    EXPECT_EQ(root_node_->id(), paragraph_1_ignored_node->GetComputedNodeData()
                                    .GetOrComputeUnignoredParentID());
    EXPECT_EQ(root_node_, paragraph_1_ignored_node->GetComputedNodeData()
                              .GetOrComputeUnignoredParent());
    EXPECT_FALSE(paragraph_1_ignored_node->GetComputedNodeData()
                     .GetOrComputeIsDescendantOfPlatformLeaf());
    EXPECT_EQ(root_node_->id(), static_text_1_0_node->GetComputedNodeData()
                                    .GetOrComputeUnignoredParentID());
    EXPECT_EQ(root_node_, static_text_1_0_node->GetComputedNodeData()
                              .GetOrComputeUnignoredParent());
    EXPECT_FALSE(static_text_1_0_node->GetComputedNodeData()
                     .GetOrComputeIsDescendantOfPlatformLeaf());
    EXPECT_EQ(root_node_->id(), paragraph_2_ignored_node->GetComputedNodeData()
                                    .GetOrComputeUnignoredParentID());
    EXPECT_EQ(root_node_, paragraph_2_ignored_node->GetComputedNodeData()
                              .GetOrComputeUnignoredParent());
    EXPECT_FALSE(paragraph_2_ignored_node->GetComputedNodeData()
                     .GetOrComputeIsDescendantOfPlatformLeaf());
    EXPECT_EQ(root_node_->id(), link_2_0_ignored_node->GetComputedNodeData()
                                    .GetOrComputeUnignoredParentID());
    EXPECT_EQ(root_node_, link_2_0_ignored_node->GetComputedNodeData()
                              .GetOrComputeUnignoredParent());
    EXPECT_FALSE(link_2_0_ignored_node->GetComputedNodeData()
                     .GetOrComputeIsDescendantOfPlatformLeaf());
    EXPECT_EQ(root_node_->id(), static_text_2_0_0_node->GetComputedNodeData()
                                    .GetOrComputeUnignoredParentID());
    EXPECT_EQ(root_node_, static_text_2_0_0_node->GetComputedNodeData()
                              .GetOrComputeUnignoredParent());
    EXPECT_FALSE(static_text_2_0_0_node->GetComputedNodeData()
                     .GetOrComputeIsDescendantOfPlatformLeaf());
    EXPECT_EQ(root_node_->id(), static_text_2_0_1_node->GetComputedNodeData()
                                    .GetOrComputeUnignoredParentID());
    EXPECT_EQ(root_node_, static_text_2_0_1_node->GetComputedNodeData()
                              .GetOrComputeUnignoredParent());
    EXPECT_FALSE(static_text_2_0_1_node->GetComputedNodeData()
                     .GetOrComputeIsDescendantOfPlatformLeaf());

    EXPECT_EQ(
        4, root_node_->GetComputedNodeData().GetOrComputeUnignoredChildCount());
    EXPECT_EQ(0, paragraph_0_node->GetComputedNodeData()
                     .GetOrComputeUnignoredChildCount());
    EXPECT_EQ(0, static_text_1_0_node->GetComputedNodeData()
                     .GetOrComputeUnignoredChildCount());
    EXPECT_EQ(0, static_text_2_0_0_node->GetComputedNodeData()
                     .GetOrComputeUnignoredChildCount());
    EXPECT_EQ(0, static_text_2_0_1_node->GetComputedNodeData()
                     .GetOrComputeUnignoredChildCount());
    EXPECT_THAT(
        root_node_->GetComputedNodeData().GetOrComputeUnignoredChildIDs(),
        ElementsAre(paragraph_0_.id, static_text_1_0_.id, static_text_2_0_0_.id,
                    static_text_2_0_1_.id));
  }
}

TEST_F(AXComputedNodeDataTest, CanComputeAttribute) {
  EXPECT_TRUE(root_node_->CanComputeStringAttribute(
      ax::mojom::StringAttribute::kValue));
  EXPECT_FALSE(root_node_->CanComputeStringAttribute(
      ax::mojom::StringAttribute::kHtmlTag));
  EXPECT_TRUE(root_node_->CanComputeIntListAttribute(
      ax::mojom::IntListAttribute::kWordStarts));
  EXPECT_FALSE(root_node_->CanComputeIntListAttribute(
      ax::mojom::IntListAttribute::kLabelledbyIds));

  AXNode* paragraph_0_node = root_node_->GetChildAtIndex(0);
  EXPECT_FALSE(paragraph_0_node->CanComputeStringAttribute(
      ax::mojom::StringAttribute::kValue));
  EXPECT_FALSE(paragraph_0_node->CanComputeStringAttribute(
      ax::mojom::StringAttribute::kHtmlTag));
  EXPECT_TRUE(paragraph_0_node->CanComputeIntListAttribute(
      ax::mojom::IntListAttribute::kWordStarts));
  EXPECT_FALSE(paragraph_0_node->CanComputeIntListAttribute(
      ax::mojom::IntListAttribute::kLabelledbyIds));

  // By removing the `ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot`
  // attribute, the root is no longer a content editable.
  root_.RemoveBoolAttribute(ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot);
  root_.AddIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds,
                            {static_text_0_0_ignored_.id});
  paragraph_0_.AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                  "New: \nvalue.");
  paragraph_0_.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "p");
  paragraph_0_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                   {0, 4});

  AXTreeUpdate tree_update;
  tree_update.root_id = root_.id;
  tree_update.nodes = {root_, paragraph_0_};

  ASSERT_TRUE(GetTree()->Unserialize(tree_update));
  root_node_ = GetTree()->root();
  ASSERT_EQ(root_.id, root_node_->id());

  // Computing the value attribute is only supported on non-atomic text fields.
  EXPECT_FALSE(root_node_->CanComputeStringAttribute(
      ax::mojom::StringAttribute::kValue));
  EXPECT_FALSE(root_node_->CanComputeStringAttribute(
      ax::mojom::StringAttribute::kHtmlTag));
  EXPECT_TRUE(root_node_->CanComputeIntListAttribute(
      ax::mojom::IntListAttribute::kWordStarts));
  EXPECT_TRUE(root_node_->HasIntListAttribute(
      ax::mojom::IntListAttribute::kLabelledbyIds));
  EXPECT_FALSE(root_node_->CanComputeIntListAttribute(
      ax::mojom::IntListAttribute::kLabelledbyIds));

  paragraph_0_node = root_node_->GetChildAtIndex(0);
  // However, for maximum flexibility, if the value attribute is already present
  // in the node's data we should use it without checking if the role supports
  // it.
  EXPECT_TRUE(
      paragraph_0_node->HasStringAttribute(ax::mojom::StringAttribute::kValue));
  EXPECT_FALSE(paragraph_0_node->CanComputeStringAttribute(
      ax::mojom::StringAttribute::kValue));
  EXPECT_TRUE(paragraph_0_node->HasStringAttribute(
      ax::mojom::StringAttribute::kHtmlTag));
  EXPECT_FALSE(paragraph_0_node->CanComputeStringAttribute(
      ax::mojom::StringAttribute::kHtmlTag));
  EXPECT_TRUE(paragraph_0_node->CanComputeIntListAttribute(
      ax::mojom::IntListAttribute::kWordStarts));
  EXPECT_FALSE(paragraph_0_node->CanComputeIntListAttribute(
      ax::mojom::IntListAttribute::kLabelledbyIds));
}

TEST_F(AXComputedNodeDataTest, ComputeAttribute) {
  // Embedded object behavior is dependant on platform. We manually set it to a
  // specific value so that test results are consistent across platforms.
  ScopedAXEmbeddedObjectBehaviorSetter embedded_object_behaviour(
      AXEmbeddedObjectBehavior::kSuppressCharacter);

  // Line breaks should be inserted between each paragraph to mirror how HTML's
  // "textContent" works.
  EXPECT_THAT(root_node_->GetComputedNodeData().ComputeAttributeUTF8(
                  ax::mojom::StringAttribute::kValue),
              StrEq("\nt_1\ns+t++2...0.  0s t\n2\r0\r\n1"));

  // Boundaries are delimited by a vertical bar, '|'.
  // Words: "|t|_|1s|+|t|++|2|...|0|.  |0s| |t|\n|2|\r|0|\r\n|1|".
  int32_t word_starts[] = {0, 5, 8, 12, 16, 19, 21, 23, 26};
  int32_t word_ends[] = {4, 6, 9, 13, 18, 20, 22, 24, 27};
  EXPECT_THAT(root_node_->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kWordStarts),
              ElementsAreArray(word_starts));
  EXPECT_THAT(root_node_->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kWordEnds),
              ElementsAreArray(word_ends));

  // Lines: "|t_1s+t++2...0.  0s t|\n|2|\r|0|\r\n|1|".
  int32_t line_starts[] = {0, 21, 23, 26};
  int32_t line_ends[] = {21, 23, 26, 27};
  EXPECT_THAT(root_node_->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kLineStarts),
              ElementsAreArray(line_starts));
  EXPECT_THAT(root_node_->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kLineEnds),
              ElementsAreArray(line_ends));

  // Sentences: "|t_1s+t++2...0.|  |0s t|\n|2|\r|0|\r\n|1|".
  int32_t sentence_starts[] = {0, 21, 23, 26};
  int32_t sentence_ends[] = {21, 23, 26, 27};
  EXPECT_THAT(root_node_->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kSentenceStarts),
              ElementsAreArray(sentence_starts));
  EXPECT_THAT(root_node_->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kSentenceEnds),
              ElementsAreArray(sentence_ends));

  AXNode* paragraph_0_node = root_node_->GetChildAtIndex(0);
  EXPECT_FALSE(paragraph_0_node->CanComputeStringAttribute(
      ax::mojom::StringAttribute::kValue))
      << "The static text child should be ignored.";

  EXPECT_THAT(paragraph_0_node->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kWordStarts),
              SizeIs(0));
  EXPECT_THAT(paragraph_0_node->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kWordEnds),
              SizeIs(0));
  EXPECT_THAT(paragraph_0_node->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kLineStarts),
              SizeIs(0));
  EXPECT_THAT(paragraph_0_node->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kLineEnds),
              SizeIs(0));
  EXPECT_THAT(paragraph_0_node->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kSentenceStarts),
              SizeIs(0));
  EXPECT_THAT(paragraph_0_node->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kSentenceEnds),
              SizeIs(0));

  // By removing the `ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot`
  // attribute, the root is no longer a content editable.
  root_.RemoveBoolAttribute(ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot);
  paragraph_0_.AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                  "New: \nvalue.");
  // Word starts/ends are intentionally set to the wrong values to ensure that
  // `AXNodeData` takes priority over `AXComputedNodeData` if present.
  std::vector<int32_t> wrong_word_starts = {1, 5};
  std::vector<int32_t> wrong_word_ends = {6, 8};
  paragraph_0_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordStarts,
                                   wrong_word_starts);
  paragraph_0_.AddIntListAttribute(ax::mojom::IntListAttribute::kWordEnds,
                                   wrong_word_ends);

  AXTreeUpdate tree_update;
  tree_update.root_id = root_.id;
  tree_update.nodes = {root_, paragraph_0_};

  ASSERT_TRUE(GetTree()->Unserialize(tree_update));
  root_node_ = GetTree()->root();
  ASSERT_EQ(root_.id, root_node_->id());

  EXPECT_FALSE(
      root_node_->CanComputeStringAttribute(ax::mojom::StringAttribute::kValue))
      << "Computing the value attribute is only supported on non-atomic text "
         "fields.";

  // No change to the various boundaries should have been observed since the
  // root's text content hasn't changed.
  EXPECT_THAT(root_node_->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kWordStarts),
              ElementsAreArray(word_starts));
  EXPECT_THAT(root_node_->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kWordEnds),
              ElementsAreArray(word_ends));
  EXPECT_THAT(root_node_->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kLineStarts),
              ElementsAreArray(line_starts));
  EXPECT_THAT(root_node_->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kLineEnds),
              ElementsAreArray(line_ends));
  EXPECT_THAT(root_node_->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kSentenceStarts),
              ElementsAreArray(sentence_starts));
  EXPECT_THAT(root_node_->GetComputedNodeData().ComputeAttribute(
                  ax::mojom::IntListAttribute::kSentenceEnds),
              ElementsAreArray(sentence_ends));

  paragraph_0_node = root_node_->GetChildAtIndex(0);
  EXPECT_FALSE(paragraph_0_node->CanComputeStringAttribute(
      ax::mojom::StringAttribute::kValue));
  EXPECT_THAT(
      paragraph_0_node->GetStringAttribute(ax::mojom::StringAttribute::kValue),
      StrEq("New: \nvalue."))
      << "For maximum flexibility, if the value attribute is already present "
         "in the node's data we should use it without checking if the role "
         "supports it.";

  // Word starts/ends are intentionally set to the wrong values in `AXNodeData`.
  EXPECT_THAT(paragraph_0_node->GetIntListAttribute(
                  ax::mojom::IntListAttribute::kWordStarts),
              ElementsAreArray(wrong_word_starts))
      << "`AXNodeData` should take priority over `AXComputedNodeData`, if "
         "present.";

  EXPECT_THAT(paragraph_0_node->GetIntListAttribute(
                  ax::mojom::IntListAttribute::kWordEnds),
              ElementsAreArray(wrong_word_ends))
      << "`AXNodeData` should take priority over `AXComputedNodeData`, if "
         "present.";

  EXPECT_THAT(paragraph_0_node->GetIntListAttribute(
                  ax::mojom::IntListAttribute::kLineStarts),
              ElementsAre());
  EXPECT_THAT(paragraph_0_node->GetIntListAttribute(
                  ax::mojom::IntListAttribute::kLineEnds),
              ElementsAre());
  EXPECT_THAT(paragraph_0_node->GetIntListAttribute(
                  ax::mojom::IntListAttribute::kSentenceStarts),
              ElementsAre());
  EXPECT_THAT(paragraph_0_node->GetIntListAttribute(
                  ax::mojom::IntListAttribute::kSentenceEnds),
              ElementsAre());
}

TEST_F(AXComputedNodeDataTest, GetOrComputeTextContent) {
  // Embedded object behavior is dependant on platform. We manually set it to a
  // specific value so that test results are consistent across platforms.
  ScopedAXEmbeddedObjectBehaviorSetter embedded_object_behaviour(
      AXEmbeddedObjectBehavior::kSuppressCharacter);

  EXPECT_THAT(root_node_->GetComputedNodeData()
                  .GetOrComputeTextContentWithParagraphBreaksUTF8(),
              StrEq("\nt_1\ns+t++2...0.  0s t\n2\r0\r\n1"));
  EXPECT_THAT(root_node_->GetComputedNodeData().GetOrComputeTextContentUTF8(),
              StrEq("t_1s+t++2...0.  0s t\n2\r0\r\n1"));
  EXPECT_EQ(
      root_node_->GetComputedNodeData().GetOrComputeTextContentLengthUTF8(),
      27);

  // Paragraph_0's text is ignored. Ignored text should not be visible.
  const AXNode* paragraph_0_node = root_node_->GetChildAtIndex(0);
  EXPECT_THAT(paragraph_0_node->GetComputedNodeData()
                  .GetOrComputeTextContentWithParagraphBreaksUTF8(),
              StrEq(""));
  EXPECT_THAT(
      paragraph_0_node->GetComputedNodeData().GetOrComputeTextContentUTF8(),
      StrEq(""));
  EXPECT_EQ(paragraph_0_node->GetComputedNodeData()
                .GetOrComputeTextContentLengthUTF8(),
            0);

  // The two incarnations of the "TextContent" methods should behave identically
  // when line breaks are manually inserted via e.g. a <br> element in HTML, as
  // this case demonstrates.
  const AXNode* paragraph_2_ignored_node = root_node_->GetChildAtIndex(2);
  EXPECT_THAT(paragraph_2_ignored_node->GetComputedNodeData()
                  .GetOrComputeTextContentWithParagraphBreaksUTF8(),
              StrEq("s+t++2...0.  0s t\n2\r0\r\n1"));
  EXPECT_THAT(paragraph_2_ignored_node->GetComputedNodeData()
                  .GetOrComputeTextContentUTF8(),
              StrEq("s+t++2...0.  0s t\n2\r0\r\n1"));
  EXPECT_EQ(paragraph_2_ignored_node->GetComputedNodeData()
                .GetOrComputeTextContentLengthUTF8(),
            24);
}

}  // namespace ui
