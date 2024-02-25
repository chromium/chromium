// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_STYLE_PLATFORM_STYLE_H_
#define UI_VIEWS_STYLE_PLATFORM_STYLE_H_

#include <memory>

#include "ui/views/controls/button/button.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace gfx {
class Range;
}  // namespace gfx

namespace views {

// Cross-platform API for providing platform-specific styling for toolkit-views.
class VIEWS_EXPORT PlatformStyle {
 public:
  PlatformStyle() = delete;
  PlatformStyle(const PlatformStyle&) = delete;
  PlatformStyle& operator=(const PlatformStyle&) = delete;

  // Whether the ok button is in the leading position (left in LTR) in a
  // typical Cancel/OK button group.
  static const bool kIsOkButtonLeading;

  // Minimum size for platform-styled buttons.
  static const int kMinLabelButtonWidth;
  static const int kMinLabelButtonHeight;

  // Whether the default button for a dialog can be the Cancel button.
  static const bool kDialogDefaultButtonCanBeCancel;

  // Whether right clicking on text, selects the word under cursor.
  static const bool kSelectWordOnRightClick;

  // Whether right clicking inside an unfocused text view selects all the text.
  static const bool kSelectAllOnRightClickWhenUnfocused;

  // Whether the Space key clicks a button on key press or key release.
  static const Button::KeyClickAction kKeyClickActionOnSpace;

  // Whether the Return key clicks the focused control (on key press).
  // Otherwise, Return does nothing unless it is handled by an accelerator.
  static const bool kReturnClicksFocusedControl;

  // Whether cursor left and right can be used in a TableView to select and
  // resize columns and whether a focus ring should be shown around the active
  // cell.
  static const bool kTableViewSupportsKeyboardNavigationByCell;

  // Whether selecting a row in a TreeView selects the entire row or only the
  // label for that row.
  static const bool kTreeViewSelectionPaintsEntireRow;

  // Whether ripples should be used for visual feedback on control activation.
  static const bool kUseRipples;

  // Whether text fields should use a "drag" cursor when not actually
  // dragging but available to do so.
  static const bool kTextfieldUsesDragCursorWhenDraggable;

  // Whether controls in inactive widgets appear disabled.
  static const bool kInactiveWidgetControlsAppearDisabled;

  // Default setting at bubble creation time for whether arrow will be adjusted
  // for bubbles going off-screen to bring more bubble area into view.
  static const bool kAdjustBubbleIfOffscreen;

  // Default focus behavior on the platform.
  static const View::FocusBehavior kDefaultFocusBehavior;

  // Creates the default scrollbar for the given orientation.
  static std::unique_ptr<ScrollBar> CreateScrollBar(
      ScrollBar::Orientation orientation);

  // Called whenever a textfield edit fails. Gives visual/audio feedback about
  // the failed edit if platform-appropriate.
  static void OnTextfieldEditFailed();

  // When deleting backwards in |string| with the cursor at index
  // |cursor_position|, return the range of UTF-16 words to be deleted.
  // This is to support deleting entire graphemes instead of individual
  // characters when necessary on Mac, and code points made from surrogate
  // pairs on other platforms.
  static gfx::Range RangeToDeleteBackwards(const std::u16string& text,
                                           size_t cursor_position);
};

}  // namespace views

#endif  // UI_VIEWS_STYLE_PLATFORM_STYLE_H_
