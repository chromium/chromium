// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define all the base::Features used by ui/accessibility.
#ifndef UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_
#define UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_export.h"

namespace features {

AX_EXPORT extern const base::Feature kEnableAccessibilityExposeARIAAnnotations;

// Returns true if ARIA annotations should be exposed to the browser AX Tree.
AX_EXPORT bool IsAccessibilityExposeARIAAnnotationsEnabled();

AX_EXPORT extern const base::Feature kEnableAccessibilityExposeDisplayNone;

// Returns true if "display: none" nodes should be exposed to the
// browser process AXTree.
AX_EXPORT bool IsAccessibilityExposeDisplayNoneEnabled();

}  // namespace features

#endif  // UI_ACCESSIBILITY_ACCESSIBILITY_FEATURES_H_
