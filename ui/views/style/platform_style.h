// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_STYLE_PLATFORM_STYLE_H_
#define UI_VIEWS_STYLE_PLATFORM_STYLE_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/views_export.h"

namespace gfx {
class Range;
}  // namespace gfx

namespace views {

class Border;
class Label;
class LabelButton;
class ScrollBar;

// Cross-platform API for providing platform-specific styling for toolkit-views.
class VIEWS_EXPORT PlatformStyle {
 public:
  // Type used by LabelButton to map button states to text colors.
  using ButtonColorByState = SkColor[Button::STATE_COUNT];

  // Whether the ok button is in the leading position (left in LTR) in a
  // typical Cancel/OK button group.
  static const bool kIsOkButtonLeading;

  // Minimum size for platform-styled buttons (Button::STYLE_BUTTON).
  static const int kMinLabelButtonWidth;
  static const int kMinLabelButtonHeight;

  // Whether the default button for a dialog can be the Cancel button.
  static const bool kDialogDefaultButtonCanBeCancel;

  // Whether right clicking on text, selects the word under cursor.
  static const bool kSelectWordOnRightClick;

  // Whether right clicking inside an unfocused text view selects all the text.
  static const bool kSelectAllOnRightClickWhenUnfocused;

  // The menu button's action to show the menu.
  static const Button::NotifyAction kMenuNotifyActivationAction;

  // Whether the Space key clicks a button on key press or key release.
  static const Button::KeyClickAction kKeyClickActionOnSpace;

  // Whether the Return key clicks the focused control (on key press).
  // Otherwise, Return does nothing unless it is handled by an accelerator.
  static const bool kReturnClicksFocusedControl;

  // Whether selecting a row in a TreeView selects the entire row or only the
  // label for that row.
  static const bool kTreeViewSelectionPaintsEntireRow;

  // Whether ripples should be used for visual feedback on control activation.
  static const bool kUseRipples;

  // Whether to scroll text fields to the beginning when they gain or lose
  // focus.
  static const bool kTextfieldScrollsToStartOnFocusChange;

  // Whether text fields should use a "drag" cursor when not actually
  // dragging but available to do so.
  static const bool kTextfieldUsesDragCursorWhenDraggable;

  // The thickness and inset amount of focus ring halos.
  static const float kFocusHaloThickness;
  static const float kFocusHaloInset;

  // Whether "button-like" (for example, buttons in the top chrome or Omnibox
  // decorations) UI elements should use a focus ring, rather than show
  // hover state on focus.
  static const bool kPreferFocusRings;

  // Whether controls in inactive widgets appear disabled.
  static const bool kInactiveWidgetControlsAppearDisabled;

  // Creates the default scrollbar for the given orientation.
  static std::unique_ptr<ScrollBar> CreateScrollBar(bool is_horizontal);

  // Applies platform styles to |label| and fills |color_by_state| with the text
  // colors for normal, pressed, hovered, and disabled states, if the colors for
  // Button::STYLE_BUTTON buttons differ from those provided by ui::NativeTheme.
  static void ApplyLabelButtonTextStyle(Label* label,
                                        ButtonColorByState* color_by_state);

  // Applies the current system theme to the default border created by |button|.
  static std::unique_ptr<Border> CreateThemedLabelButtonBorder(
      LabelButton* button);

  // Called whenever a textfield edit fails. Gives visual/audio feedback about
  // the failed edit if platform-appropriate.
  static void OnTextfieldEditFailed();

  // When deleting backwards in |string| with the cursor at index
  // |cursor_position|, return the range of UTF-16 words to be deleted.
  // This is to support deleting entire graphemes instead of individual
  // characters when necessary on Mac, and code points made from surrogate
  // pairs on other platforms.
  static gfx::Range RangeToDeleteBackwards(const base::string16& text,
                                           size_t cursor_position);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PlatformStyle);
};

}  // namespace views

#endif  // UI_VIEWS_STYLE_PLATFORM_STYLE_H_
