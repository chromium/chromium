// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/views_switches.h"

#include "build/build_config.h"

namespace views::switches {

// Please keep alphabetized.

// Disables the disregarding of potentially unintended input events such as
// button clicks that happen instantly after the button is shown. Use this for
// integration tests that do automated clicks etc.
const char kDisableInputEventActivationProtectionForTesting[] =
    "disable-input-event-activation-protection";

// Draws a semitransparent rect to indicate the bounds of each view.
const char kDrawViewBoundsRects[] = "draw-view-bounds-rects";

// Captures stack traces on View construction to provide better debug info.
const char kViewStackTraces[] = "view-stack-traces";

}  // namespace views::switches
