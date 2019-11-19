// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/accessibility_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace features {

// Allow use of ARIA roles from https://github.com/w3c/annotation-aria draft.
const base::Feature kEnableAccessibilityExposeARIAAnnotations{
    "AccessibilityExposeARIAAnnotations", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAccessibilityExposeARIAAnnotationsEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityExposeARIAAnnotations);
}

// Enable exposing "display: none" nodes to the browser process AXTree
const base::Feature kEnableAccessibilityExposeDisplayNone{
    "AccessibilityExposeDisplayNone", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAccessibilityExposeDisplayNoneEnabled() {
  return base::FeatureList::IsEnabled(
      ::features::kEnableAccessibilityExposeDisplayNone);
}

}  // namespace features
