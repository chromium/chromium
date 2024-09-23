// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_POSITION_H_
#define UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_POSITION_H_

#include "base/component_export.h"
#include "gin/wrappable.h"
#include "ui/accessibility/ax_node_position.h"

namespace gin {
class Arguments;
}

namespace ui {

// A class that wraps an AXPosition to make available in javascript.
//
// For new additions, consider whether it should be public to the
// chrome.automation extension api. If so, please update
// extensions/common/api/automation.idl.
class COMPONENT_EXPORT(AX_PLATFORM) AutomationPosition final
    : public gin::Wrappable<AutomationPosition> {
 public:
  AutomationPosition(const AXNode& node,
                     AXPositionKind kind,
                     int offset,
                     bool is_upstream);

  AutomationPosition(const AutomationPosition&) = delete;
  AutomationPosition& operator=(const AutomationPosition&) = delete;

  ~AutomationPosition() override;

  static gin::WrapperInfo kWrapperInfo;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

 private:
  std::string GetTreeID(gin::Arguments* arguments);
  int GetAnchorID(gin::Arguments* arguments);
  int GetChildIndex(gin::Arguments* arguments);
  int GetTextOffset(gin::Arguments* arguments);
  std::string GetAffinity(gin::Arguments* arguments);
  bool IsNullPosition(gin::Arguments* arguments);
  bool IsTreePosition(gin::Arguments* arguments);
  bool IsTextPosition(gin::Arguments* arguments);
  bool IsLeafTextPosition(gin::Arguments* arguments);
  bool AtStartOfAnchor(gin::Arguments* arguments);
  bool AtEndOfAnchor(gin::Arguments* arguments);
  bool AtStartOfWord(gin::Arguments* arguments);
  bool AtEndOfWord(gin::Arguments* arguments);
  bool AtStartOfLine(gin::Arguments* arguments);
  bool AtEndOfLine(gin::Arguments* arguments);
  bool AtStartOfParagraph(gin::Arguments* arguments);
  bool AtEndOfParagraph(gin::Arguments* arguments);
  bool AtStartOfPage(gin::Arguments* arguments);
  bool AtEndOfPage(gin::Arguments* arguments);
  bool AtStartOfFormat(gin::Arguments* arguments);
  bool AtEndOfFormat(gin::Arguments* arguments);
  bool AtStartOfContent(gin::Arguments* arguments);
  bool AtEndOfContent(gin::Arguments* arguments);
  void AsTreePosition(gin::Arguments* arguments);
  void AsTextPosition(gin::Arguments* arguments);
  void AsLeafTextPosition(gin::Arguments* arguments);
  void MoveToPositionAtStartOfAnchor(gin::Arguments* arguments);
  void MoveToPositionAtEndOfAnchor(gin::Arguments* arguments);
  void MoveToPositionAtStartOfContent(gin::Arguments* arguments);
  void MoveToPositionAtEndOfContent(gin::Arguments* arguments);
  void MoveToParentPosition(gin::Arguments* arguments);
  void MoveToNextLeafTreePosition(gin::Arguments* arguments);
  void MoveToPreviousLeafTreePosition(gin::Arguments* arguments);
  void MoveToNextLeafTextPosition(gin::Arguments* arguments);
  void MoveToPreviousLeafTextPosition(gin::Arguments* arguments);
  void MoveToNextCharacterPosition(gin::Arguments* arguments);
  void MoveToPreviousCharacterPosition(gin::Arguments* arguments);
  void MoveToNextWordStartPosition(gin::Arguments* arguments);
  void MoveToPreviousWordStartPosition(gin::Arguments* arguments);
  void MoveToNextWordEndPosition(gin::Arguments* arguments);
  void MoveToPreviousWordEndPosition(gin::Arguments* arguments);
  void MoveToNextLineStartPosition(gin::Arguments* arguments);
  void MoveToPreviousLineStartPosition(gin::Arguments* arguments);
  void MoveToNextLineEndPosition(gin::Arguments* arguments);
  void MoveToPreviousLineEndPosition(gin::Arguments* arguments);
  void MoveToPreviousFormatStartPosition(gin::Arguments* arguments);
  void MoveToNextFormatEndPosition(gin::Arguments* arguments);
  void MoveToNextParagraphStartPosition(gin::Arguments* arguments);
  void MoveToPreviousParagraphStartPosition(gin::Arguments* arguments);
  void MoveToNextParagraphEndPosition(gin::Arguments* arguments);
  void MoveToPreviousParagraphEndPosition(gin::Arguments* arguments);
  void MoveToNextPageStartPosition(gin::Arguments* arguments);
  void MoveToPreviousPageStartPosition(gin::Arguments* arguments);
  void MoveToNextPageEndPosition(gin::Arguments* arguments);
  void MoveToPreviousPageEndPosition(gin::Arguments* arguments);
  void MoveToNextAnchorPosition(gin::Arguments* arguments);
  void MoveToPreviousAnchorPosition(gin::Arguments* arguments);
  int MaxTextOffset(gin::Arguments* arguments);
  bool IsPointingToLineBreak(gin::Arguments* arguments);
  bool IsInTextObject(gin::Arguments* arguments);
  bool IsInWhiteSpace(gin::Arguments* arguments);
  bool IsValid(gin::Arguments* arguments);
  std::u16string GetText(gin::Arguments* arguments);

  AXNodePosition::AXPositionInstance position_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_POSITION_H_
