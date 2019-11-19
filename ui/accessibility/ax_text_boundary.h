// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TEXT_BOUNDARY_H_
#define UI_ACCESSIBILITY_AX_TEXT_BOUNDARY_H_

#include <ostream>
#include <string>

#include "ui/accessibility/ax_export.h"

namespace ui {

// Defines a set of text boundaries that can be used to find, e.g. the beginning
// of the next word, or the end of the current line, from any position in the
// accessibility tree.
//
// Most boundaries come in three flavors: A "WordStartOrEnd" boundary for
// example differs from a "WordStart" or a "WordEnd" boundary in that the first
// would consider both the start and the end of the word to be boundaries, based
// on whether we are navigating in the forward or backward direction, while the
// other two would consider only the start or the end respectively. This allows
// us, for example, to implement word navigation on Windows vs. Mac, where
// navigating to the previous / next word on Windows always jumps to the start
// of the word, while on Mac and Linux it jumps to the start / end of the
// current word. A "...StartOrEnd" boundary, e.g. "WordStartOrEnd", would also
// allow us to implement different behavior when announcing a text unit, vs.
// while navigating by it. Announcing a word for example should not include any
// whitespace after the word.
//
// An "Object" boundary would limit an operation to the current object, e.g. it
// could enable us to retrieve all the text inside a particular text field.
enum class AXTextBoundary {
  kCharacter,
  kFormatChange,
  kLineEnd,
  kLineStart,
  kLineStartOrEnd,
  kObject,
  kPageEnd,
  kPageStart,
  kPageStartOrEnd,
  kParagraphEnd,
  kParagraphStart,
  kParagraphStartOrEnd,
  kSentenceEnd,
  kSentenceStart,
  kSentenceStartOrEnd,
  kWebPage,
  kWordEnd,
  kWordStart,
  kWordStartOrEnd,
};

// Specifies the direction to search for a text boundary.
enum class AXTextBoundaryDirection {
  // Search forward for the next boundary past a given position.
  kForwards,
  // Search backward for the previous boundary before a given position.
  kBackwards
};

// Produces a string representation of AXTextBoundary.
AX_EXPORT std::string ToString(const AXTextBoundary boundary);
AX_EXPORT std::ostream& operator<<(std::ostream& stream,
                                   const AXTextBoundary& boundary);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TEXT_BOUNDARY_H_
