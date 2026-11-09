// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/automation/automation_position.h"

#include "gin/arguments.h"
#include "gin/object_template_builder.h"
#include "ui/accessibility/ax_node.h"

namespace ui {

AutomationPosition::AutomationPosition(const AXNode& node,
                                       AXPositionKind kind,
                                       int offset,
                                       bool is_upstream) {
  ax::mojom::TextAffinity affinity = is_upstream
                                         ? ax::mojom::TextAffinity::kUpstream
                                         : ax::mojom::TextAffinity::kDownstream;
  switch (kind) {
    case AXPositionKind::TREE_POSITION:
      position_ = AXNodePosition::CreatePosition(node, offset, affinity);
      break;
    case AXPositionKind::TEXT_POSITION:
      if (offset < 0) {
        offset = 0;
      }
      position_ = AXNodePosition::CreateTextPosition(node, offset, affinity);
      position_->SnapToMaxTextOffsetIfBeyond();
      break;
    case AXPositionKind::NULL_POSITION:
      NOTREACHED();
  }
}

AutomationPosition::~AutomationPosition() = default;

const gin::WrapperInfo* AutomationPosition::wrapper_info() const {
  return &kWrapperInfo;
}

gin::ObjectTemplateBuilder AutomationPosition::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<AutomationPosition>::GetObjectTemplateBuilder(isolate)
      .SetProperty<&AutomationPosition::GetTreeID>("treeID")
      .SetProperty<&AutomationPosition::GetAnchorID>("anchorID")
      .SetProperty<&AutomationPosition::GetChildIndex>("childIndex")
      .SetProperty<&AutomationPosition::GetTextOffset>("textOffset")
      .SetProperty<&AutomationPosition::GetAffinity>("affinity")
      .SetMethod<&AutomationPosition::IsNullPosition>("isNullPosition")
      .SetMethod<&AutomationPosition::IsTreePosition>("isTreePosition")
      .SetMethod<&AutomationPosition::IsTextPosition>("isTextPosition")
      .SetMethod<&AutomationPosition::IsLeafTextPosition>("isLeafTextPosition")
      .SetMethod<&AutomationPosition::AtStartOfAnchor>("atStartOfAnchor")
      .SetMethod<&AutomationPosition::AtEndOfAnchor>("atEndOfAnchor")
      .SetMethod<&AutomationPosition::AtStartOfWord>("atStartOfWord")
      .SetMethod<&AutomationPosition::AtEndOfWord>("atEndOfWord")
      .SetMethod<&AutomationPosition::AtStartOfLine>("atStartOfLine")
      .SetMethod<&AutomationPosition::AtEndOfLine>("atEndOfLine")
      .SetMethod<&AutomationPosition::AtStartOfParagraph>("atStartOfParagraph")
      .SetMethod<&AutomationPosition::AtEndOfParagraph>("atEndOfParagraph")
      .SetMethod<&AutomationPosition::AtStartOfPage>("atStartOfPage")
      .SetMethod<&AutomationPosition::AtEndOfPage>("atEndOfPage")
      .SetMethod<&AutomationPosition::AtStartOfFormat>("atStartOfFormat")
      .SetMethod<&AutomationPosition::AtEndOfFormat>("atEndOfFormat")
      .SetMethod<&AutomationPosition::AtStartOfContent>("atStartOfContent")
      .SetMethod<&AutomationPosition::AtEndOfContent>("atEndOfContent")
      .SetMethod<&AutomationPosition::AsTreePosition>("asTreePosition")
      .SetMethod<&AutomationPosition::AsTextPosition>("asTextPosition")
      .SetMethod<&AutomationPosition::AsLeafTextPosition>("asLeafTextPosition")
      .SetMethod<&AutomationPosition::MoveToPositionAtStartOfAnchor>(
          "moveToPositionAtStartOfAnchor")
      .SetMethod<&AutomationPosition::MoveToPositionAtEndOfAnchor>(
          "moveToPositionAtEndOfAnchor")
      .SetMethod<&AutomationPosition::MoveToPositionAtStartOfContent>(
          "moveToPositionAtStartOfContent")
      .SetMethod<&AutomationPosition::MoveToPositionAtEndOfContent>(
          "moveToPositionAtEndOfContent")
      .SetMethod<&AutomationPosition::MoveToParentPosition>(
          "moveToParentPosition")
      .SetMethod<&AutomationPosition::MoveToNextLeafTreePosition>(
          "moveToNextLeafTreePosition")
      .SetMethod<&AutomationPosition::MoveToPreviousLeafTreePosition>(
          "moveToPreviousLeafTreePosition")
      .SetMethod<&AutomationPosition::MoveToNextLeafTextPosition>(
          "moveToNextLeafTextPosition")
      .SetMethod<&AutomationPosition::MoveToPreviousLeafTextPosition>(
          "moveToPreviousLeafTextPosition")
      .SetMethod<&AutomationPosition::MoveToNextCharacterPosition>(
          "moveToNextCharacterPosition")
      .SetMethod<&AutomationPosition::MoveToPreviousCharacterPosition>(
          "moveToPreviousCharacterPosition")
      .SetMethod<&AutomationPosition::MoveToNextWordStartPosition>(
          "moveToNextWordStartPosition")
      .SetMethod<&AutomationPosition::MoveToPreviousWordStartPosition>(
          "moveToPreviousWordStartPosition")
      .SetMethod<&AutomationPosition::MoveToNextWordEndPosition>(
          "moveToNextWordEndPosition")
      .SetMethod<&AutomationPosition::MoveToPreviousWordEndPosition>(
          "moveToPreviousWordEndPosition")
      .SetMethod<&AutomationPosition::MoveToNextLineStartPosition>(
          "moveToNextLineStartPosition")
      .SetMethod<&AutomationPosition::MoveToPreviousLineStartPosition>(
          "moveToPreviousLineStartPosition")
      .SetMethod<&AutomationPosition::MoveToNextLineEndPosition>(
          "moveToNextLineEndPosition")
      .SetMethod<&AutomationPosition::MoveToPreviousLineEndPosition>(
          "moveToPreviousLineEndPosition")
      .SetMethod<&AutomationPosition::MoveToPreviousFormatStartPosition>(
          "moveToPreviousFormatStartPosition")
      .SetMethod<&AutomationPosition::MoveToNextFormatEndPosition>(
          "moveToNextFormatEndPosition")
      .SetMethod<&AutomationPosition::MoveToNextParagraphStartPosition>(
          "moveToNextParagraphStartPosition")
      .SetMethod<&AutomationPosition::MoveToPreviousParagraphStartPosition>(
          "moveToPreviousParagraphStartPosition")
      .SetMethod<&AutomationPosition::MoveToNextParagraphEndPosition>(
          "moveToNextParagraphEndPosition")
      .SetMethod<&AutomationPosition::MoveToPreviousParagraphEndPosition>(
          "moveToPreviousParagraphEndPosition")
      .SetMethod<&AutomationPosition::MoveToNextPageStartPosition>(
          "moveToNextPageStartPosition")
      .SetMethod<&AutomationPosition::MoveToPreviousPageStartPosition>(
          "moveToPreviousPageStartPosition")
      .SetMethod<&AutomationPosition::MoveToNextPageEndPosition>(
          "moveToNextPageEndPosition")
      .SetMethod<&AutomationPosition::MoveToPreviousPageEndPosition>(
          "moveToPreviousPageEndPosition")
      .SetMethod<&AutomationPosition::MoveToNextAnchorPosition>(
          "moveToNextAnchorPosition")
      .SetMethod<&AutomationPosition::MoveToPreviousAnchorPosition>(
          "moveToPreviousAnchorPosition")
      .SetMethod<&AutomationPosition::MaxTextOffset>("maxTextOffset")
      .SetMethod<&AutomationPosition::IsPointingToLineBreak>(
          "isPointingToLineBreak")
      .SetMethod<&AutomationPosition::IsInTextObject>("isInTextObject")
      .SetMethod<&AutomationPosition::IsInWhiteSpace>("isInWhiteSpace")
      .SetMethod<&AutomationPosition::IsValid>("isValid")
      .SetMethod<&AutomationPosition::GetText>("getText");
}

std::string AutomationPosition::GetTreeID(gin::Arguments* arguments) {
  return position_->tree_id().ToString();
}

int AutomationPosition::GetAnchorID(gin::Arguments* arguments) {
  return position_->anchor_id();
}

int AutomationPosition::GetChildIndex(gin::Arguments* arguments) {
  return position_->child_index();
}

int AutomationPosition::GetTextOffset(gin::Arguments* arguments) {
  return position_->text_offset();
}

std::string AutomationPosition::GetAffinity(gin::Arguments* arguments) {
  return ToString(position_->affinity());
}

bool AutomationPosition::IsNullPosition(gin::Arguments* arguments) {
  return position_->IsNullPosition();
}

bool AutomationPosition::IsTreePosition(gin::Arguments* arguments) {
  return position_->IsTreePosition();
}

bool AutomationPosition::IsTextPosition(gin::Arguments* arguments) {
  return position_->IsTextPosition();
}

bool AutomationPosition::IsLeafTextPosition(gin::Arguments* arguments) {
  return position_->IsLeafTextPosition();
}

bool AutomationPosition::AtStartOfAnchor(gin::Arguments* arguments) {
  return position_->AtStartOfAnchor();
}

bool AutomationPosition::AtEndOfAnchor(gin::Arguments* arguments) {
  return position_->AtEndOfAnchor();
}

bool AutomationPosition::AtStartOfWord(gin::Arguments* arguments) {
  return position_->AtStartOfWord();
}

bool AutomationPosition::AtEndOfWord(gin::Arguments* arguments) {
  return position_->AtEndOfWord();
}

bool AutomationPosition::AtStartOfLine(gin::Arguments* arguments) {
  return position_->AtStartOfLine();
}

bool AutomationPosition::AtEndOfLine(gin::Arguments* arguments) {
  return position_->AtEndOfLine();
}

bool AutomationPosition::AtStartOfParagraph(gin::Arguments* arguments) {
  return position_->AtStartOfParagraph();
}

bool AutomationPosition::AtEndOfParagraph(gin::Arguments* arguments) {
  return position_->AtEndOfParagraph();
}

bool AutomationPosition::AtStartOfPage(gin::Arguments* arguments) {
  return position_->AtStartOfPage();
}

bool AutomationPosition::AtEndOfPage(gin::Arguments* arguments) {
  return position_->AtEndOfPage();
}

bool AutomationPosition::AtStartOfFormat(gin::Arguments* arguments) {
  return position_->AtStartOfFormat();
}

bool AutomationPosition::AtEndOfFormat(gin::Arguments* arguments) {
  return position_->AtEndOfFormat();
}

bool AutomationPosition::AtStartOfContent(gin::Arguments* arguments) {
  return position_->AtStartOfContent();
}

bool AutomationPosition::AtEndOfContent(gin::Arguments* arguments) {
  return position_->AtEndOfContent();
}

void AutomationPosition::AsTreePosition(gin::Arguments* arguments) {
  position_ = position_->AsTreePosition();
}

void AutomationPosition::AsTextPosition(gin::Arguments* arguments) {
  position_ = position_->AsTextPosition();
}

void AutomationPosition::AsLeafTextPosition(gin::Arguments* arguments) {
  position_ = position_->AsLeafTextPosition();
}

void AutomationPosition::MoveToPositionAtStartOfAnchor(
    gin::Arguments* arguments) {
  position_ = position_->CreatePositionAtStartOfAnchor();
}

void AutomationPosition::MoveToPositionAtEndOfAnchor(
    gin::Arguments* arguments) {
  position_ = position_->CreatePositionAtEndOfAnchor();
}

void AutomationPosition::MoveToPositionAtStartOfContent(
    gin::Arguments* arguments) {
  position_ = position_->CreatePositionAtStartOfContent();
}

void AutomationPosition::MoveToPositionAtEndOfContent(
    gin::Arguments* arguments) {
  position_ = position_->CreatePositionAtEndOfContent();
}

void AutomationPosition::MoveToParentPosition(gin::Arguments* arguments) {
  position_ = position_->CreateParentPosition();
}

void AutomationPosition::MoveToNextLeafTreePosition(gin::Arguments* arguments) {
  position_ = position_->CreateNextLeafTreePosition();
}

void AutomationPosition::MoveToPreviousLeafTreePosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousLeafTreePosition();
}

void AutomationPosition::MoveToNextLeafTextPosition(gin::Arguments* arguments) {
  position_ = position_->CreateNextLeafTextPosition();
}

void AutomationPosition::MoveToPreviousLeafTextPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousLeafTextPosition();
}

void AutomationPosition::MoveToNextCharacterPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreateNextCharacterPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToPreviousCharacterPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousCharacterPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToNextWordStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreateNextWordStartPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToPreviousWordStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousWordStartPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToNextWordEndPosition(gin::Arguments* arguments) {
  position_ = position_->CreateNextWordEndPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToPreviousWordEndPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousWordEndPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToNextLineStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreateNextLineStartPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToPreviousLineStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousLineStartPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToNextLineEndPosition(gin::Arguments* arguments) {
  position_ = position_->CreateNextLineEndPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToPreviousLineEndPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousLineEndPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToPreviousFormatStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousFormatStartPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToNextFormatEndPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreateNextFormatEndPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToNextParagraphStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreateNextParagraphStartPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToPreviousParagraphStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousParagraphStartPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToNextParagraphEndPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreateNextParagraphEndPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToPreviousParagraphEndPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousParagraphEndPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToNextPageStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreateNextPageStartPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToPreviousPageStartPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousPageStartPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToNextPageEndPosition(gin::Arguments* arguments) {
  position_ = position_->CreateNextPageEndPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToPreviousPageEndPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousPageEndPosition(
      {AXBoundaryBehavior::kCrossBoundary,
       AXBoundaryDetection::kDontCheckInitialPosition});
}

void AutomationPosition::MoveToNextAnchorPosition(gin::Arguments* arguments) {
  position_ = position_->CreateNextAnchorPosition();
}

void AutomationPosition::MoveToPreviousAnchorPosition(
    gin::Arguments* arguments) {
  position_ = position_->CreatePreviousAnchorPosition();
}

int AutomationPosition::MaxTextOffset(gin::Arguments* arguments) {
  return position_->MaxTextOffset();
}

bool AutomationPosition::IsPointingToLineBreak(gin::Arguments* arguments) {
  return position_->IsPointingToLineBreak();
}

bool AutomationPosition::IsInTextObject(gin::Arguments* arguments) {
  return position_->IsInTextObject();
}

bool AutomationPosition::IsInWhiteSpace(gin::Arguments* arguments) {
  return position_->IsInWhiteSpace();
}

bool AutomationPosition::IsValid(gin::Arguments* arguments) {
  return position_->IsValid();
}

std::u16string AutomationPosition::GetText(gin::Arguments* arguments) {
  return position_->GetText();
}

}  // namespace ui
