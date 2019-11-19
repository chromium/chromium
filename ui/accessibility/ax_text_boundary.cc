// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_text_boundary.h"

namespace ui {

std::string ToString(const AXTextBoundary boundary) {
  switch (boundary) {
    case AXTextBoundary::kCharacter:
      return "CharacterBoundary";
    case AXTextBoundary::kFormatChange:
      return "FormatChangeBoundary";
    case AXTextBoundary::kLineEnd:
      return "LineEndBoundary";
    case AXTextBoundary::kLineStart:
      return "LineStartBoundary";
    case AXTextBoundary::kLineStartOrEnd:
      return "LineStartOrEndBoundary";
    case AXTextBoundary::kObject:
      return "ObjectBoundary";
    case AXTextBoundary::kPageEnd:
      return "PageEndBoundary";
    case AXTextBoundary::kPageStart:
      return "PageStartBoundary";
    case AXTextBoundary::kPageStartOrEnd:
      return "PageStartOrEndBoundary";
    case AXTextBoundary::kParagraphEnd:
      return "ParagraphEndBoundary";
    case AXTextBoundary::kParagraphStart:
      return "ParagraphStartBoundary";
    case AXTextBoundary::kParagraphStartOrEnd:
      return "ParagraphStartOrEndBoundary";
    case AXTextBoundary::kSentenceEnd:
      return "SentenceEndBoundary";
    case AXTextBoundary::kSentenceStart:
      return "SentenceStartBoundary";
    case AXTextBoundary::kSentenceStartOrEnd:
      return "SentenceStartOrEndBoundary";
    case AXTextBoundary::kWebPage:
      return "WebPageBoundary";
    case AXTextBoundary::kWordEnd:
      return "WordEndBoundary";
    case AXTextBoundary::kWordStart:
      return "WordStartBoundary";
    case AXTextBoundary::kWordStartOrEnd:
      return "WordStartOrEndBoundary";
  }
}

std::ostream& operator<<(std::ostream& stream, const AXTextBoundary& boundary) {
  return stream << ToString(boundary);
}

}  // namespace ui
