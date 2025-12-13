// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEWS_SWITCHES_H_
#define UI_VIEWS_VIEWS_SWITCHES_H_

namespace views::switches {

// Please keep alphabetized.

// Disables the disregarding of potentially unintended input events such as
// button clicks that happen instantly after the button is shown. Use this for
// integration tests that do automated clicks etc.
inline constexpr char kDisableInputEventActivationProtectionForTesting[] =
    "disable-input-event-activation-protection";

// Draws a semitransparent red rect to indicate the bounds of each view. Also,
// draws a blue semitransparent rect when GetContentBounds() differs from
// GetLocalBounds().
inline constexpr char kDrawViewBoundsRects[] = "draw-view-bounds-rects";

// Captures stack traces on View construction to provide better debug info.
inline constexpr char kViewStackTraces[] = "view-stack-traces";

}  // namespace views::switches

#endif  // UI_VIEWS_VIEWS_SWITCHES_H_
