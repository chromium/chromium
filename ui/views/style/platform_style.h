// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_STYLE_PLATFORM_STYLE_H_
#define UI_VIEWS_STYLE_PLATFORM_STYLE_H_

#include <stddef.h>

#include <memory>
#include <string_view>

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
  static constexpr bool kIsOkButtonLeading = BUILDFLAG(IS_WIN);

  // Whether the default button for a dialog can be the Cancel button.
  static constexpr bool kDialogDefaultButtonCanBeCancel = !BUILDFLAG(IS_MAC);

  // Whether right clicking on text, selects the word under cursor.
  static constexpr bool kSelectWordOnRightClick = BUILDFLAG(IS_MAC);

  // Whether right clicking inside an unfocused text view selects all the text.
  static constexpr bool kSelectAllOnRightClickWhenUnfocused = BUILDFLAG(IS_MAC);

  // Whether the Space key clicks a button on key press or key release.
  static constexpr Button::KeyClickAction kKeyClickActionOnSpace =
#if BUILDFLAG(IS_MAC)
      Button::KeyClickAction::kOnKeyPress;
#else
      Button::KeyClickAction::kOnKeyRelease;
#endif

  // Whether the Return key clicks the focused control (on key press).
  // Otherwise, Return does nothing unless it is handled by an accelerator.
  // On Mac, the Return key is used to perform the default action even when a
  // control is focused.
  static constexpr bool kReturnClicksFocusedControl = !BUILDFLAG(IS_MAC);

  // Whether cursor left and right can be used in a TableView to select and
  // resize columns and whether a focus ring should be shown around the active
  // cell.
  static constexpr bool kTableViewSupportsKeyboardNavigationByCell =
      !BUILDFLAG(IS_MAC);

  // Whether TableView will show rows with alternating colors.
  static constexpr bool kTableViewSupportsAlternatingRowColors =
      BUILDFLAG(IS_MAC);

  // Whether selecting a row in a TreeView selects the entire row or only the
  // label for that row.
  static constexpr bool kTreeViewSelectionPaintsEntireRow = BUILDFLAG(IS_MAC);

  // Whether ripples should be used for visual feedback on control activation.
  static constexpr bool kUseRipples = !BUILDFLAG(IS_MAC);

  // Whether text fields should use a "drag" cursor when not actually
  // dragging but available to do so.
  static constexpr bool kTextfieldUsesDragCursorWhenDraggable =
      !BUILDFLAG(IS_MAC);

  // Whether controls in inactive widgets appear disabled.
  static constexpr bool kInactiveWidgetControlsAppearDisabled =
      BUILDFLAG(IS_MAC);

  // Default setting at bubble creation time for whether arrow will be adjusted
  // for bubbles going off-screen to bring more bubble area into view. Linux
  // clips bubble windows that extend outside their parent window bounds.
  static constexpr bool kAdjustBubbleIfOffscreen = !BUILDFLAG(IS_LINUX);

  // Default focus behavior on the platform.
  static constexpr View::FocusBehavior kDefaultFocusBehavior =
#if BUILDFLAG(IS_MAC)
      View::FocusBehavior::ACCESSIBLE_ONLY;
#else
      View::FocusBehavior::ALWAYS;
#endif

  // On Windows, the first menu item is automatically selected when a menu
  // is opened with the keyboard.
  static constexpr bool kAutoSelectFirstMenuItemFromKeyboard =
      BUILDFLAG(IS_WIN);

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
  static gfx::Range RangeToDeleteBackwards(std::u16string_view text,
                                           size_t cursor_position);
};

}  // namespace views

#endif  // UI_VIEWS_STYLE_PLATFORM_STYLE_H_
