// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_BROWSER_CONTROLS_OFFSET_TAG_CONSTRAINTS_H_
#define UI_ANDROID_BROWSER_CONTROLS_OFFSET_TAG_CONSTRAINTS_H_

#include "components/viz/common/quads/offset_tag.h"
#include "ui/android/ui_android_export.h"

namespace ui {

// All OffsetTagConstraints for browser controls. These constraints define how
// far they can be moved by their OffsetTagsValues during a scroll/animation.
struct UI_ANDROID_EXPORT BrowserControlsOffsetTagConstraints {
  viz::OffsetTagConstraints top_controls_constraints;
  viz::OffsetTagConstraints content_constraints;
  viz::OffsetTagConstraints bottom_controls_constraints;
};

}  // namespace ui

#endif  // UI_ANDROID_BROWSER_CONTROLS_OFFSET_TAG_CONSTRAINTS_H_
