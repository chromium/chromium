// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation.h"

#include "ui/accessibility/android/accessibility_state.h"

namespace gfx {

// static
void Animation::UpdatePrefersReducedMotion() {
  // prefers_reduced_motion_ should only be modified on the UI thread.
  // TODO(crbug.com/40611878): DCHECK this assertion once tests are
  // well-behaved.
  prefers_reduced_motion_ = ui::AccessibilityState::PrefersReducedMotion();
}

}  // namespace gfx
