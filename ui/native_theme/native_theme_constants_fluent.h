// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_CONSTANTS_FLUENT_H_
#define UI_NATIVE_THEME_NATIVE_THEME_CONSTANTS_FLUENT_H_

namespace ui {

constexpr int kFluentScrollbarThickness = 15;
constexpr int kFluentScrollbarThumbThickness = 9;

// A sufficiently large value ensures the most round curve for the corners of
// the scrollbar thumb and overlay buttons.
constexpr int kFluentScrollbarPartsRadius = 999;

// The value specifies the minimum length the scrollbar thumb can have*.
// We choose 17px for compatibility reasons with the default scrollbar on the
// Windows platform.
//
// *Please note that when the scrollbar height for the vertical scrollbar
// (or width for horizontal) is less than [1], the thumb does get smaller
// until it disappears when the scrollbar size is less than [2].
//
// [1] 2 * kFluentScrollbarButtonSideLength + kFluentScrollbarMinimalThumbLength
// [2] 2 * kFluentScrollbarButtonSideLength + 1
constexpr int kFluentScrollbarMinimalThumbLength = 17;

// The value represents button height for the vertical scrollbar and width for
// the horizontal. Another side for the corresponding orientation is the same as
// the track thickness.
constexpr int kFluentScrollbarButtonSideLength = 18;

// Arrow rect side length. The height and width of the rect are equal.
constexpr int kFluentScrollbarArrowRectLength = 9;

// Arrow rect side length when the button is pressed. The height and width of
// the rect are equal.
constexpr int kFluentScrollbarPressedArrowRectLength = 8;

// Use this length only when the font that contains arrow icons is not present
// on the device and the default arrows are painted using SkPath. Since the
// scrollbar thickness is an even number, we shift the odd rect from the
// button's center. Also, we can avoid the usage of anti-aliasing, which tends
// to produce visual defects on specific scales.
constexpr int kFluentScrollbarPressedArrowRectFallbackLength = 7;

// Offset the arrow icon by this amount off-center, away from the thumb.
constexpr int kFluentScrollbarArrowOffset = 1;

// The outline width used to paint track and buttons in High Contrast mode.
constexpr float kFluentScrollbarTrackOutlineWidth = 1.0f;

// The font that supports the drawing of Fluent scrollbar arrow icons.
// Currently, it's only available on Windows 11 by default.
constexpr char kFluentScrollbarFont[] = "Segoe Fluent Icons";

// Fluent scrollbar arrow code points.
constexpr char kFluentScrollbarUpArrow[] = "\uEDDB";
constexpr char kFluentScrollbarDownArrow[] = "\uEDDC";
constexpr char kFluentScrollbarLeftArrow[] = "\uEDD9";
constexpr char kFluentScrollbarRightArrow[] = "\uEDDA";

// Track and button inset to be applied at the time of painting overlay
// scrollbars. This will yield a 1dp border around the track that is
// transparent yet interactive.
constexpr int kFluentPaintedScrollbarTrackInset = 1;

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_CONSTANTS_FLUENT_H_
