// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SELECTION_MODEL_H_
#define UI_GFX_SELECTION_MODEL_H_

#include <stddef.h>
#include <vector>

#include <iosfwd>
#include <string>

#include "ui/gfx/gfx_export.h"
#include "ui/gfx/range/range.h"

namespace gfx {

// VisualCursorDirection and LogicalCursorDirection represent directions of
// motion of the cursor in BiDi text. The combinations that make sense are:
//
//  base::i18n::TextDirection  VisualCursorDirection  LogicalCursorDirection
//       LEFT_TO_RIGHT             CURSOR_LEFT           CURSOR_BACKWARD
//       LEFT_TO_RIGHT             CURSOR_RIGHT          CURSOR_FORWARD
//       RIGHT_TO_LEFT             CURSOR_RIGHT          CURSOR_BACKWARD
//       RIGHT_TO_LEFT             CURSOR_LEFT           CURSOR_FORWARD
enum VisualCursorDirection {
  CURSOR_LEFT,
  CURSOR_RIGHT,
  CURSOR_UP,
  CURSOR_DOWN
};
enum LogicalCursorDirection {
  CURSOR_BACKWARD,
  CURSOR_FORWARD
};

// TODO(xji): publish bidi-editing guide line and replace the place holder.
// SelectionModel is used to represent the logical selection and visual
// position of cursor.
//
// For bi-directional text, the mapping between visual position and logical
// position is not one-to-one. For example, logical text "abcDEF" where capital
// letters stand for Hebrew, the visual display is "abcFED". According to the
// bidi editing guide (http://bidi-editing-guideline):
// 1. If pointing to the right half of the cell of a LTR character, the current
// position must be set after this character and the caret must be displayed
// after this character.
// 2. If pointing to the right half of the cell of a RTL character, the current
// position must be set before this character and the caret must be displayed
// before this character.
//
// Pointing to the right half of 'c' and pointing to the right half of 'D' both
// set the logical cursor position to 3. But the cursor displayed visually at
// different places:
// Pointing to the right half of 'c' displays the cursor right of 'c' as
// "abc|FED".
// Pointing to the right half of 'D' displays the cursor right of 'D' as
// "abcFED|".
// So, besides the logical selection start point and end point, we need extra
// information to specify to which character the visual cursor is bound. This
// is given by a "caret affinity" which is either CURSOR_BACKWARD (indicating
// the trailing half of the 'c' in this case) or CURSOR_FORWARD (indicating
// the leading half of the 'D').
class GFX_EXPORT SelectionModel {
 public:
  // Create a default SelectionModel to be overwritten later.
  SelectionModel();
  // Create a SelectionModel representing a caret |position| without a
  // selection. The |affinity| is meaningful only when the caret is positioned
  // between bidi runs that are not visually contiguous: in that case, it
  // indicates the run to which the caret is attached for display purposes.
  SelectionModel(size_t position, LogicalCursorDirection affinity);
  // Create a SelectionModel representing a selection (which may be empty).
  // The caret position is the end of the range.
  SelectionModel(const Range& selection, LogicalCursorDirection affinity);
  // Create a SelectionModel representing multiple selections (which may be
  // empty but not overlapping). The end of the first range determines the caret
  // position.
  SelectionModel(const std::vector<Range>& selections,
                 LogicalCursorDirection affinity);
  SelectionModel(const SelectionModel& selection_model);
  ~SelectionModel();

  // |selection| should overlap with neither |selection_| nor
  // |secondary_selections_|.
  void AddSecondarySelection(const Range& selection);

  const Range& selection() const { return selection_; }
  size_t caret_pos() const { return selection_.end(); }
  LogicalCursorDirection caret_affinity() const { return caret_affinity_; }
  const std::vector<Range>& secondary_selections() const {
    return secondary_selections_;
  }
  std::vector<Range> GetAllSelections() const;

  // WARNING: Generally the selection start should not be changed without
  // considering the effect on the caret affinity.
  void set_selection_start(uint32_t pos) { selection_.set_start(pos); }

  bool operator==(const SelectionModel& sel) const;
  bool operator!=(const SelectionModel& sel) const { return !(*this == sel); }

  std::string ToString() const;

 private:
  // Logical selection. The logical caret position is the end of the selection.
  Range selection_;
  // Secondary selections not associated with the cursor. Do not overlap.
  std::vector<Range> secondary_selections_;

  // The logical direction from the caret position (selection_.end()) to the
  // character it is attached to for display purposes. This matters only when
  // the surrounding characters are not visually contiguous, which happens only
  // in bidi text (and only at bidi run boundaries). The text is treated as
  // though it was surrounded on both sides by runs in the dominant text
  // direction. For example, supposing the dominant direction is LTR and the
  // logical text is "abcDEF", where DEF is right-to-left text, the visual
  // cursor will display as follows:
  //    caret position    CURSOR_BACKWARD affinity    CURSOR_FORWARD affinity
  //          0                  |abcFED                     |abcFED
  //          1                  a|bcFED                     a|bcFED
  //          2                  ab|cFED                     ab|cFED
  //          3                  abc|FED                     abcFED|
  //          4                  abcFE|D                     abcFE|D
  //          5                  abcF|ED                     abcF|ED
  //          6                  abc|FED                     abcFED|
  LogicalCursorDirection caret_affinity_;
};

GFX_EXPORT std::ostream& operator<<(std::ostream& out,
                                    const SelectionModel& model);

}  // namespace gfx

#endif  // UI_GFX_SELECTION_MODEL_H_
