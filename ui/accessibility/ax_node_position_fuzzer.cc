// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_fuzzer_util.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"

// Max amount of fuzz data needed to create the next position
const size_t kNextNodePositionMaxDataSize = 4;

// Min/Max node size for generated tree.
const size_t kMinNodeCount = 10;
const size_t kMaxNodeCount = kMinNodeCount + 50;

// Min fuzz data needed for fuzzer to function.
// Tree of minimum size with text for each node + 2 positions.
const size_t kMinFuzzDataSize =
    kMinNodeCount * AXTreeFuzzerGenerator::kMinimumNewNodeFuzzDataSize +
    kMinNodeCount * AXTreeFuzzerGenerator::kMinTextFuzzDataSize +
    2 * kNextNodePositionMaxDataSize;
// Cap fuzz data to avoid slowness.
const size_t kMaxFuzzDataSize = 3500;

using TestPositionType =
    std::unique_ptr<ui::AXPosition<ui::AXNodePosition, ui::AXNode>>;
using TestPositionRange =
    ui::AXRange<ui::AXPosition<ui::AXNodePosition, ui::AXNode>>;

// Helper to create positions in the given tree.
class AXNodePositionFuzzerGenerator {
 public:
  AXNodePositionFuzzerGenerator(ui::AXTree* tree,
                                ui::AXNodeID max_id,
                                FuzzerData& fuzzer_data);

  TestPositionType CreateNewPosition();
  TestPositionType GenerateNextPosition(TestPositionType& current_position,
                                        TestPositionType& previous_position);

  static void CallPositionAPIs(TestPositionType& position,
                               TestPositionType& other_position);

 private:
  static ax::mojom::MoveDirection GenerateMoveDirection(unsigned char byte);
  static ax::mojom::TextAffinity GenerateTextAffinity(unsigned char byte);
  static ui::AXPositionKind GeneratePositionKind(unsigned char byte);
  static ui::AXPositionAdjustmentBehavior GenerateAdjustmentBehavior(
      unsigned char byte);
  static ui::AXMovementOptions GenerateMovementOptions(
      unsigned char behavior_byte,
      unsigned char detection_byte);

  TestPositionType CreateNewPosition(ui::AXNodeID anchor_id,
                                     int child_index_or_text_offset,
                                     ui::AXPositionKind position_kind,
                                     ax::mojom::TextAffinity affinity);

  raw_ptr<ui::AXTree> tree_;
  const ui::AXNodeID max_id_;
  const raw_ref<FuzzerData> fuzzer_data_;
};

AXNodePositionFuzzerGenerator::AXNodePositionFuzzerGenerator(
    ui::AXTree* tree,
    ui::AXNodeID max_id,
    FuzzerData& fuzzer_data)
    : tree_(tree), max_id_(max_id), fuzzer_data_(fuzzer_data) {}

TestPositionType AXNodePositionFuzzerGenerator::CreateNewPosition() {
  return CreateNewPosition(fuzzer_data_->NextByte(), fuzzer_data_->NextByte(),
                           GeneratePositionKind(fuzzer_data_->NextByte()),
                           GenerateTextAffinity(fuzzer_data_->NextByte()));
}

TestPositionType AXNodePositionFuzzerGenerator::CreateNewPosition(
    ui::AXNodeID anchor_id,
    int child_index_or_text_offset,
    ui::AXPositionKind position_kind,
    ax::mojom::TextAffinity affinity) {
  // To ensure that anchor_id is between |ui::kInvalidAXNodeID| and the max ID
  // of the tree (non-inclusive), get a number [0, max_id - 1) and then shift by
  // 1 to get [1, max_id)
  anchor_id = (anchor_id % (max_id_ - 1)) + 1;
  ui::AXNode* anchor = tree_->GetFromId(anchor_id);
  DCHECK(anchor);

  switch (position_kind) {
    case ui::AXPositionKind::TREE_POSITION:
      // Avoid division by zero in the case where the node has no children.
      child_index_or_text_offset =
          anchor->GetChildCount()
              ? child_index_or_text_offset % anchor->GetChildCount()
              : 0;
      return ui::AXNodePosition::CreateTreePosition(*anchor,
                                                    child_index_or_text_offset);
    case ui::AXPositionKind::TEXT_POSITION: {
      // Avoid division by zero in the case where the node has no text.
      child_index_or_text_offset =
          anchor->GetTextContentLengthUTF16()
              ? child_index_or_text_offset % anchor->GetTextContentLengthUTF16()
              : 0;
      return ui::AXNodePosition::CreateTextPosition(
          *anchor, child_index_or_text_offset, affinity);
      case ui::AXPositionKind::NULL_POSITION:
        NOTREACHED_IN_MIGRATION();
        return ui::AXNodePosition::CreateNullPosition();
    }
  }
}

ax::mojom::MoveDirection AXNodePositionFuzzerGenerator::GenerateMoveDirection(
    unsigned char byte) {
  constexpr unsigned char max_value =
      static_cast<unsigned char>(ax::mojom::MoveDirection::kMaxValue);
  return static_cast<ax::mojom::MoveDirection>(byte % max_value);
}

ax::mojom::TextAffinity AXNodePositionFuzzerGenerator::GenerateTextAffinity(
    unsigned char byte) {
  constexpr unsigned char max_value =
      static_cast<unsigned char>(ax::mojom::TextAffinity::kMaxValue);
  return static_cast<ax::mojom::TextAffinity>(byte % max_value);
}

ui::AXPositionKind AXNodePositionFuzzerGenerator::GeneratePositionKind(
    unsigned char byte) {
  return byte % 2 ? ui::AXPositionKind::TREE_POSITION
                  : ui::AXPositionKind::TEXT_POSITION;
}

ui::AXPositionAdjustmentBehavior
AXNodePositionFuzzerGenerator::GenerateAdjustmentBehavior(unsigned char byte) {
  return byte % 2 ? ui::AXPositionAdjustmentBehavior::kMoveBackward
                  : ui::AXPositionAdjustmentBehavior::kMoveForward;
}

ui::AXMovementOptions AXNodePositionFuzzerGenerator::GenerateMovementOptions(
    unsigned char behavior_byte,
    unsigned char detection_byte) {
  return ui::AXMovementOptions(
      static_cast<ui::AXBoundaryBehavior>(behavior_byte % 3),
      static_cast<ui::AXBoundaryDetection>(detection_byte % 3));
}

TestPositionType AXNodePositionFuzzerGenerator::GenerateNextPosition(
    TestPositionType& current_position,
    TestPositionType& previous_position) {
  switch (fuzzer_data_->NextByte() % 55) {
    case 0:
    default:
      return CreateNewPosition();
    case 1:
      return current_position->AsValidPosition();
    case 2:
      return current_position->AsTreePosition();
    case 3:
      return current_position->AsLeafTreePosition();
    case 4:
      return current_position->AsTextPosition();
    case 5:
      return current_position->AsLeafTextPosition();
    case 6:
      return current_position->AsDomSelectionPosition();
    case 7:
      return current_position->AsUnignoredPosition(
          GenerateAdjustmentBehavior(fuzzer_data_->NextByte()));
    case 8:
      return current_position->CreateAncestorPosition(
          previous_position->GetAnchor(),
          GenerateMoveDirection(fuzzer_data_->NextByte()));
    case 9:
      return current_position->CreatePositionAtStartOfAnchor();
    case 10:
      return current_position->CreatePositionAtEndOfAnchor();
    case 11:
      return current_position->CreatePositionAtStartOfAXTree();
    case 12:
      return current_position->CreatePositionAtEndOfAXTree();
    case 13:
      return current_position->CreatePositionAtStartOfContent();
    case 14:
      return current_position->CreatePositionAtEndOfContent();
    case 15:
      return current_position->CreateChildPositionAt(fuzzer_data_->NextByte() %
                                                     10);
    case 16:
      return current_position->CreateParentPosition(
          GenerateMoveDirection(fuzzer_data_->NextByte()));
    case 17:
      return current_position->CreateNextLeafTreePosition();
    case 18:
      return current_position->CreatePreviousLeafTreePosition();
    case 19:
      return current_position->CreateNextLeafTextPosition();
    case 20:
      return current_position->CreatePreviousLeafTextPosition();
    case 21:
      return current_position->AsLeafTextPositionBeforeCharacter();
    case 22:
      return current_position->AsLeafTextPositionAfterCharacter();
    case 23:
      return current_position->CreatePreviousCharacterPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 24:
      return current_position->CreateNextWordStartPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 25:
      return current_position->CreatePreviousWordStartPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 26:
      return current_position->CreateNextWordEndPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 27:
      return current_position->CreatePreviousWordEndPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 28:
      return current_position->CreateNextLineStartPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 29:
      return current_position->CreatePreviousLineStartPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 30:
      return current_position->CreateNextLineEndPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 31:
      return current_position->CreatePreviousLineEndPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 32:
      return current_position->CreateNextFormatStartPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 33:
      return current_position->CreatePreviousFormatStartPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 34:
      return current_position->CreateNextFormatEndPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 35:
      return current_position->CreatePreviousFormatEndPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 36:
      return current_position->CreateNextSentenceStartPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 37:
      return current_position->CreatePreviousSentenceStartPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 38:
      return current_position->CreateNextSentenceEndPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 39:
      return current_position->CreatePreviousSentenceEndPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 40:
      return current_position->CreateNextParagraphStartPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 41:
      return current_position
          ->CreateNextParagraphStartPositionSkippingEmptyParagraphs(
              GenerateMovementOptions(fuzzer_data_->NextByte(),
                                      fuzzer_data_->NextByte()));
    case 42:
      return current_position->CreatePreviousParagraphStartPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 43:
      return current_position
          ->CreatePreviousParagraphStartPositionSkippingEmptyParagraphs(
              GenerateMovementOptions(fuzzer_data_->NextByte(),
                                      fuzzer_data_->NextByte()));
    case 44:
      return current_position->CreateNextParagraphEndPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 45:
      return current_position->CreatePreviousParagraphEndPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 46:
      return current_position->CreateNextPageStartPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 47:
      return current_position->CreatePreviousPageStartPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 48:
      return current_position->CreateNextPageEndPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 49:
      return current_position->CreatePreviousPageEndPosition(
          GenerateMovementOptions(fuzzer_data_->NextByte(),
                                  fuzzer_data_->NextByte()));
    case 52:
      return current_position->CreateNextAnchorPosition();
    case 53:
      return current_position->CreatePreviousAnchorPosition();
    case 54:
      return current_position->LowestCommonAncestorPosition(
          *previous_position, GenerateMoveDirection(fuzzer_data_->NextByte()));
  }
}

void AXNodePositionFuzzerGenerator::CallPositionAPIs(
    TestPositionType& position,
    TestPositionType& other_position) {
  // Call APIs on the created position. We don't care about any of the results,
  // we just want to make sure none of these crash or hang.
  std::ignore = position->GetAnchor();
  std::ignore = position->GetAnchorSiblingCount();
  std::ignore = position->IsIgnored();
  std::ignore = position->IsLeaf();
  std::ignore = position->IsValid();
  std::ignore = position->AtStartOfWord();
  std::ignore = position->AtEndOfWord();
  std::ignore = position->AtStartOfLine();
  std::ignore = position->AtEndOfLine();
  std::ignore = position->GetFormatStartBoundaryType();
  std::ignore = position->GetFormatEndBoundaryType();
  std::ignore = position->AtStartOfSentence();
  std::ignore = position->AtEndOfSentence();
  std::ignore = position->AtStartOfParagraph();
  std::ignore = position->AtEndOfParagraph();
  std::ignore = position->AtStartOfInlineBlock();
  std::ignore = position->AtStartOfPage();
  std::ignore = position->AtEndOfPage();
  std::ignore = position->AtStartOfAXTree();
  std::ignore = position->AtEndOfAXTree();
  std::ignore = position->AtStartOfContent();
  std::ignore = position->AtEndOfContent();
  std::ignore = position->LowestCommonAnchor(*other_position);
  std::ignore = position->CompareTo(*other_position);
  std::ignore = position->GetText();
  std::ignore = position->IsPointingToLineBreak();
  std::ignore = position->IsInTextObject();
  std::ignore = position->IsInWhiteSpace();
  std::ignore = position->MaxTextOffset();
  std::ignore = position->GetRole();
}

struct Environment {
  Environment() { CHECK(base::i18n::InitializeICU()); }
  base::AtExitManager at_exit_manager;
};

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
  if (size < kMinFuzzDataSize || size > kMaxFuzzDataSize)
    return 0;
  static Environment env;
  AXTreeFuzzerGenerator generator;
  FuzzerData fuzz_data(data, size);
  const size_t node_count =
      kMinNodeCount + fuzz_data.NextByte() % kMaxNodeCount;
  generator.GenerateInitialUpdate(fuzz_data, node_count);
  ui::AXNodeID max_id = generator.GetMaxAssignedID();

  ui::AXTree* tree = generator.GetTree();

  // Run with --v=1 to aid in debugging a specific crash.
  VLOG(1) << tree->ToString();

  // Check to ensure there is enough fuzz data to create two positions.
  if (fuzz_data.RemainingBytes() < kNextNodePositionMaxDataSize * 2)
    return 0;
  AXNodePositionFuzzerGenerator position_fuzzer(tree, max_id, fuzz_data);

  // Having two positions allows us to test "more interesting" APIs that do work
  // on multiple positions.
  TestPositionType previous_position = position_fuzzer.CreateNewPosition();
  TestPositionType position = position_fuzzer.CreateNewPosition();

  while (fuzz_data.RemainingBytes() > kNextNodePositionMaxDataSize) {
    // Run with --v=1 to aid in debugging a specific crash.
    VLOG(1) << position->ToString() << fuzz_data.RemainingBytes();

    position_fuzzer.CallPositionAPIs(position, previous_position);

    // Determine next position to test:
    TestPositionType next_position =
        position_fuzzer.GenerateNextPosition(position, previous_position);
    previous_position = std::move(position);
    position = std::move(next_position);
  }

  return 0;
}
