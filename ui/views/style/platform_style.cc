// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/platform_style.h"

#include "build/build_config.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/utf16_indexing.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/scrollbar/scroll_bar_views.h"

#if defined(OS_CHROMEOS)
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#elif defined(OS_LINUX)
#define DESKTOP_LINUX
#endif

namespace views {
namespace {

#if !defined(DESKTOP_LINUX)
// Default text and shadow colors for STYLE_BUTTON.
const SkColor kStyleButtonTextColor = SK_ColorBLACK;
const SkColor kStyleButtonShadowColor = SK_ColorWHITE;
#endif

}  // namespace

#if defined(OS_WIN) || defined(OS_CHROMEOS)
const bool PlatformStyle::kIsOkButtonLeading = true;
#else
const bool PlatformStyle::kIsOkButtonLeading = false;
#endif

// Set kFocusHaloInset to negative half of kFocusHaloThickness to draw half of
// the focus ring inside and half outside the parent elmeent
const float PlatformStyle::kFocusHaloThickness = 2.f;
const float PlatformStyle::kFocusHaloInset = -1.f;

#if !defined(OS_MACOSX)

const int PlatformStyle::kMinLabelButtonWidth = 70;
const int PlatformStyle::kMinLabelButtonHeight = 33;
const bool PlatformStyle::kDialogDefaultButtonCanBeCancel = true;
const bool PlatformStyle::kSelectWordOnRightClick = false;
const bool PlatformStyle::kSelectAllOnRightClickWhenUnfocused = false;
const Button::NotifyAction PlatformStyle::kMenuNotifyActivationAction =
    Button::NOTIFY_ON_RELEASE;
const Button::KeyClickAction PlatformStyle::kKeyClickActionOnSpace =
    Button::CLICK_ON_KEY_RELEASE;
const bool PlatformStyle::kReturnClicksFocusedControl = true;
const bool PlatformStyle::kTreeViewSelectionPaintsEntireRow = false;
const bool PlatformStyle::kUseRipples = true;
const bool PlatformStyle::kTextfieldScrollsToStartOnFocusChange = false;
const bool PlatformStyle::kTextfieldUsesDragCursorWhenDraggable = true;
const bool PlatformStyle::kPreferFocusRings = false;
const bool PlatformStyle::kInactiveWidgetControlsAppearDisabled = false;

// static
std::unique_ptr<ScrollBar> PlatformStyle::CreateScrollBar(bool is_horizontal) {
#if defined(OS_CHROMEOS)
  return std::make_unique<OverlayScrollBar>(is_horizontal);
#else
  return std::make_unique<ScrollBarViews>(is_horizontal);
#endif
}

// static
void PlatformStyle::OnTextfieldEditFailed() {}

// static
gfx::Range PlatformStyle::RangeToDeleteBackwards(const base::string16& text,
                                                 size_t cursor_position) {
  // Delete one code point, which may be two UTF-16 words.
  size_t previous_grapheme_index =
      gfx::UTF16OffsetToIndex(text, cursor_position, -1);
  return gfx::Range(cursor_position, previous_grapheme_index);
}

#endif  // OS_MACOSX

#if !defined(DESKTOP_LINUX)
// static
void PlatformStyle::ApplyLabelButtonTextStyle(
    Label* label,
    ButtonColorByState* color_by_state) {
  ButtonColorByState& colors = *color_by_state;
  colors[Button::STATE_NORMAL] = kStyleButtonTextColor;
  colors[Button::STATE_HOVERED] = kStyleButtonTextColor;
  colors[Button::STATE_PRESSED] = kStyleButtonTextColor;

  label->SetShadows(gfx::ShadowValues(
      1, gfx::ShadowValue(gfx::Vector2d(0, 1), 0, kStyleButtonShadowColor)));
}
#endif

#if !defined(DESKTOP_LINUX)
// static
std::unique_ptr<Border> PlatformStyle::CreateThemedLabelButtonBorder(
    LabelButton* button) {
  return button->CreateDefaultBorder();
}
#endif

}  // namespace views
