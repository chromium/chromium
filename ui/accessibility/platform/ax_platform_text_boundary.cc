// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_text_boundary.h"

namespace ui {

#if BUILDFLAG(USE_ATK)
AXTextBoundary FromAtkTextBoundary(AtkTextBoundary boundary) {
  // These are listed in order of their definition in the ATK header.
  switch (boundary) {
    case ATK_TEXT_BOUNDARY_CHAR:
      return AXTextBoundary::kCharacter;
    case ATK_TEXT_BOUNDARY_WORD_START:
      return AXTextBoundary::kWordStart;
    case ATK_TEXT_BOUNDARY_WORD_END:
      return AXTextBoundary::kWordEnd;
    case ATK_TEXT_BOUNDARY_SENTENCE_START:
      return AXTextBoundary::kSentenceStart;
    case ATK_TEXT_BOUNDARY_SENTENCE_END:
      return AXTextBoundary::kSentenceEnd;
    case ATK_TEXT_BOUNDARY_LINE_START:
      return AXTextBoundary::kLineStart;
    case ATK_TEXT_BOUNDARY_LINE_END:
      return AXTextBoundary::kLineEnd;
  }
}

#if ATK_CHECK_VERSION(2, 10, 0)
AXTextBoundary FromAtkTextGranularity(AtkTextGranularity granularity) {
  // These are listed in order of their definition in the ATK header.
  switch (granularity) {
    case ATK_TEXT_GRANULARITY_CHAR:
      return AXTextBoundary::kCharacter;
    case ATK_TEXT_GRANULARITY_WORD:
      return AXTextBoundary::kWordStart;
    case ATK_TEXT_GRANULARITY_SENTENCE:
      return AXTextBoundary::kSentenceStart;
    case ATK_TEXT_GRANULARITY_LINE:
      return AXTextBoundary::kLineStart;
    case ATK_TEXT_GRANULARITY_PARAGRAPH:
      return AXTextBoundary::kParagraphStart;
  }
}
#endif  // ATK_CHECK_VERSION(2, 10, 0)
#endif  // BUILDFLAG(USE_ATK)

#ifdef OS_WIN
AXTextBoundary FromIA2TextBoundary(IA2TextBoundaryType boundary) {
  switch (boundary) {
    case IA2_TEXT_BOUNDARY_CHAR:
      return AXTextBoundary::kCharacter;
    case IA2_TEXT_BOUNDARY_WORD:
      return AXTextBoundary::kWordStart;
    case IA2_TEXT_BOUNDARY_LINE:
      return AXTextBoundary::kLineStart;
    case IA2_TEXT_BOUNDARY_SENTENCE:
      return AXTextBoundary::kSentenceStart;
    case IA2_TEXT_BOUNDARY_PARAGRAPH:
      return AXTextBoundary::kParagraphStart;
    case IA2_TEXT_BOUNDARY_ALL:
      return AXTextBoundary::kObject;
  }
}

AXTextBoundary FromUIATextUnit(TextUnit unit) {
  // These are listed in order of their definition in the Microsoft
  // documentation.
  switch (unit) {
    case TextUnit_Character:
      return AXTextBoundary::kCharacter;
    case TextUnit_Format:
      return AXTextBoundary::kFormatChange;
    case TextUnit_Word:
      return AXTextBoundary::kWordStart;
    case TextUnit_Line:
      return AXTextBoundary::kLineStart;
    case TextUnit_Paragraph:
      return AXTextBoundary::kParagraphStart;
    case TextUnit_Page:
      // UI Automation's TextUnit_Page cannot be reliably supported in a Web
      // document. We return kWebPage which is the next best thing.
    case TextUnit_Document:
      return AXTextBoundary::kWebPage;
  }
}
#endif  // OS_WIN

}  // namespace ui
