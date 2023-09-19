// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TEXT_CONSTANTS_H_
#define UI_GFX_TEXT_CONSTANTS_H_

namespace gfx {

// TODO(msw): Distinguish between logical character stops and glyph stops?
// TODO(msw): Merge with base::i18n::BreakIterator::BreakType.
enum BreakType {
  CHARACTER_BREAK = 0,  // Stop cursor movement on neighboring characters.
  WORD_BREAK,           // Stop cursor movement on nearest word boundaries.
  LINE_BREAK,           // Stop cursor movement on line ends as shown on screen.
  FIELD_BREAK,          // Stop cursor movement on text ends.
};

// Specifies the selection behavior for a move/move-and-select command. For
// example consider the state "ab|cd|e", i.e. cd is selected. Assume the
// selection direction is from left to right. If we move to the beginning of the
// line (LINE_BREAK, CURSOR_LEFT), the resultant state is:
// "|ab|cde" for SELECTION_RETAIN, selection direction from right to left.
// "|abcd|e" for SELECTION_EXTEND, selection direction from right to left.
// "ab|cde" for SELECTION_CARET.
// "|abcde" for SELECTION_NONE.
enum SelectionBehavior {
  // Default behavior for a move-and-select command. The selection start point
  // remains the same. For example, this is the behavior of textfields on Mac
  // for the command moveUpAndModifySelection (Shift + Up).
  SELECTION_RETAIN,

  // Use for move-and-select commands that want the existing selection to be
  // extended in the opposite direction, when the selection direction is
  // reversed. For example, this is the behavior for textfields on Mac for the
  // command moveToLeftEndOfLineAndModifySelection (Command + Shift + Left).
  SELECTION_EXTEND,

  // Use for move-and-select commands that want the existing selection to reduce
  // to a caret, when the selection direction is reversed. For example, this is
  // the behavior for textfields on Mac for the command
  // moveWordLeftAndModifySelection (Alt + Shift + Left).
  SELECTION_CARET,

  // No selection. To be used for move commands that don't want to cause a
  // selection, and that want to collapse any pre-existing selection.
  SELECTION_NONE,
};

// Specifies the word wrapping behavior when a word would exceed the available
// display width. All words that are too wide will be put on a new line, and
// then:
enum WordWrapBehavior {
  IGNORE_LONG_WORDS,   // Overflowing word text is left on that line.
  TRUNCATE_LONG_WORDS, // Overflowing word text is truncated.
  ELIDE_LONG_WORDS,    // Overflowing word text is elided at the ellipsis.
  WRAP_LONG_WORDS,     // Overflowing word text is wrapped over multiple lines.
};

// Horizontal text alignment modes.
enum HorizontalAlignment {
  ALIGN_LEFT = 0, // Align the text's left edge with that of its display area.
  ALIGN_CENTER,   // Align the text's center with that of its display area.
  ALIGN_RIGHT,    // Align the text's right edge with that of its display area.
  ALIGN_TO_HEAD,  // Align the text to its first strong character's direction.
};

// Vertical text alignment modes for multiline text.
enum VerticalAlignment {
  ALIGN_TOP = 0,  // Align the text's top edge with that of its display area.
  ALIGN_MIDDLE,   // Align the text's center with that of its display area.
  ALIGN_BOTTOM,   // Align the text's bottom edge with that of its display area.
};

// The directionality modes used to determine the base text direction.
enum DirectionalityMode {
  DIRECTIONALITY_FROM_TEXT = 0,  // Use the first strong character's direction.
  DIRECTIONALITY_FORCE_LTR,      // Use LTR regardless of content or UI locale.
  DIRECTIONALITY_FORCE_RTL,      // Use RTL regardless of content or UI locale.
  // Note: Unless the experimental feature LeftToRightUrls is enabled,
  // DIRECTIONALITY_AS_URL is the same as DIRECTIONALITY_FORCE_LTR.
  DIRECTIONALITY_AS_URL,  // FORCE_LTR with additional rules for URLs.
};

// Text styles and adornments.
// TODO(msw): Merge with gfx::Font::FontStyle.
enum TextStyle {
  TEXT_STYLE_ITALIC = 0,
  TEXT_STYLE_STRIKE,
  TEXT_STYLE_UNDERLINE,
  TEXT_STYLE_HEAVY_UNDERLINE,

  TEXT_STYLE_COUNT,
};

// Text baseline offset types.
// Figure of font metrics:
//   +--------+--------+------------------------+-------------+
//   |        |        | internal leading       | SUPERSCRIPT |
//   |        |        +------------+-----------|             |
//   |        | ascent |            | SUPERIOR  |-------------+
//   | height |        | cap height |-----------|
//   |        |        |            | INFERIOR  |-------------+
//   |        |--------+------------+-----------|             |
//   |        | descent                         | SUBSCRIPT   |
//   +--------+---------------------------------+-------------+
enum class BaselineStyle {
  kNormalBaseline = 0,
  kSuperscript,  // e.g. a mathematical exponent would be superscript.
  kSuperior,     // e.g. 8th, the "th" would be superior script.
  kInferior,     // e.g. 1/2, the "2" would be inferior ("1" is superior).
  kSubscript,    // e.g. H2O, the "2" would be subscript.
};

// Elision behaviors of text that exceeds constrained dimensions.
enum ElideBehavior {
  NO_ELIDE = 0, // Do not modify the text, it may overflow its available bounds.
  TRUNCATE,     // Do not elide or fade, just truncate at the end of the string.
  ELIDE_HEAD,   // Add an ellipsis at the start of the string.
  ELIDE_MIDDLE, // Add an ellipsis in the middle of the string.
  ELIDE_TAIL,   // Add an ellipsis at the end of the string.
  ELIDE_EMAIL,  // Add ellipses to username and domain substrings.
  FADE_TAIL,    // Fade the string's end opposite of its horizontal alignment.
};

}  // namespace gfx

#endif  // UI_GFX_TEXT_CONSTANTS_H_
