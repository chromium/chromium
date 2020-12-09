// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/platform_style.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/utf16_indexing.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/buildflags.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/scrollbar/scroll_bar_views.h"

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#endif

namespace views {

#if defined(OS_WIN)
const bool PlatformStyle::kIsOkButtonLeading = true;
#else
const bool PlatformStyle::kIsOkButtonLeading = false;
#endif

// Set kFocusHaloInset to negative half of kFocusHaloThickness to draw half of
// the focus ring inside and half outside the parent elmeent
const float PlatformStyle::kFocusHaloThickness = 2.f;
const float PlatformStyle::kFocusHaloInset = -1.f;

#if !defined(OS_APPLE)

const int PlatformStyle::kMinLabelButtonWidth = 70;
const int PlatformStyle::kMinLabelButtonHeight = 33;
const bool PlatformStyle::kDialogDefaultButtonCanBeCancel = true;
const bool PlatformStyle::kSelectWordOnRightClick = false;
const bool PlatformStyle::kSelectAllOnRightClickWhenUnfocused = false;
const Button::KeyClickAction PlatformStyle::kKeyClickActionOnSpace =
    Button::KeyClickAction::kOnKeyRelease;
const bool PlatformStyle::kReturnClicksFocusedControl = true;
const bool PlatformStyle::kTableViewSupportsKeyboardNavigationByCell = true;
const bool PlatformStyle::kTreeViewSelectionPaintsEntireRow = false;
const bool PlatformStyle::kUseRipples = true;
const bool PlatformStyle::kTextfieldScrollsToStartOnFocusChange = false;
const bool PlatformStyle::kTextfieldUsesDragCursorWhenDraggable = true;
const bool PlatformStyle::kInactiveWidgetControlsAppearDisabled = false;
const View::FocusBehavior PlatformStyle::kDefaultFocusBehavior =
    View::FocusBehavior::ALWAYS;

// Linux clips bubble windows that extend outside their parent window
// bounds.
const bool PlatformStyle::kAdjustBubbleIfOffscreen =
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
    false;
#else
    true;
#endif

// static
std::unique_ptr<ScrollBar> PlatformStyle::CreateScrollBar(bool is_horizontal) {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
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

#endif  // OS_APPLE

#if !BUILDFLAG(ENABLE_DESKTOP_AURA) || \
    (!defined(OS_LINUX) && !defined(OS_CHROMEOS))
// static
std::unique_ptr<Border> PlatformStyle::CreateThemedLabelButtonBorder(
    LabelButton* button) {
  return button->CreateDefaultBorder();
}
#endif

}  // namespace views
