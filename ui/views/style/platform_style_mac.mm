// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/platform_style.h"

#import <Cocoa/Cocoa.h>

#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/base/buildflags.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/button/label_button.h"
#import "ui/views/controls/scrollbar/cocoa_scroll_bar.h"

extern "C" {
// From CFString private headers.
typedef CF_ENUM(CFIndex, CFStringCharacterClusterType) {
  kCFStringGraphemeCluster = 1, /* Unicode Grapheme Cluster */
  kCFStringComposedCharacterCluster =
      2, /* Compose all non-base (including spacing marks) */
  kCFStringCursorMovementCluster =
      3, /* Cluster suitable for cursor movements */
  kCFStringBackwardDeletionCluster =
      4 /* Cluster suitable for backward deletion */
};

CFRange CFStringGetRangeOfCharacterClusterAtIndex(
    CFStringRef string,
    CFIndex index,
    CFStringCharacterClusterType type);
}

namespace views {

const int PlatformStyle::kMinLabelButtonWidth = 32;
const int PlatformStyle::kMinLabelButtonHeight = 30;
const bool PlatformStyle::kDialogDefaultButtonCanBeCancel = false;
const bool PlatformStyle::kSelectWordOnRightClick = true;
const bool PlatformStyle::kSelectAllOnRightClickWhenUnfocused = true;
const bool PlatformStyle::kTextfieldUsesDragCursorWhenDraggable = false;
const bool PlatformStyle::kTableViewSupportsKeyboardNavigationByCell = false;
const bool PlatformStyle::kTreeViewSelectionPaintsEntireRow = true;
const bool PlatformStyle::kUseRipples = false;
const bool PlatformStyle::kInactiveWidgetControlsAppearDisabled = true;
const bool PlatformStyle::kAdjustBubbleIfOffscreen = true;
const View::FocusBehavior PlatformStyle::kDefaultFocusBehavior =
    View::FocusBehavior::ACCESSIBLE_ONLY;

const Button::KeyClickAction PlatformStyle::kKeyClickActionOnSpace =
    Button::KeyClickAction::kOnKeyPress;

// On Mac, the Return key is used to perform the default action even when a
// control is focused.
const bool PlatformStyle::kReturnClicksFocusedControl = false;

// static
std::unique_ptr<ScrollBar> PlatformStyle::CreateScrollBar(
    ScrollBar::Orientation orientation) {
  return std::make_unique<CocoaScrollBar>(orientation);
}

// static
void PlatformStyle::OnTextfieldEditFailed() {
  NSBeep();
}

// static
gfx::Range PlatformStyle::RangeToDeleteBackwards(const std::u16string& text,
                                                 size_t cursor_position) {
  if (cursor_position == 0)
    return gfx::Range();

  base::apple::ScopedCFTypeRef<CFStringRef> cf_string(
      CFStringCreateWithCharacters(
          kCFAllocatorDefault, reinterpret_cast<const UniChar*>(text.data()),
          base::checked_cast<CFIndex>(text.size())));
  CFRange range_to_delete = CFStringGetRangeOfCharacterClusterAtIndex(
      cf_string.get(), base::checked_cast<CFIndex>(cursor_position - 1),
      kCFStringBackwardDeletionCluster);

  if (range_to_delete.location == NSNotFound)
    return gfx::Range();

  // The range needs to be reversed to undo correctly.
  return gfx::Range(base::checked_cast<size_t>(range_to_delete.location +
                                               range_to_delete.length),
                    base::checked_cast<size_t>(range_to_delete.location));
}

}  // namespace views
