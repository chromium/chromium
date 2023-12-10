// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/android/ui_accessibility_features.h"
#include "base/feature_list.h"
namespace ui {
BASE_FEATURE(kStartSurfaceAccessibilityCheck,
             "StartSurfaceAccessibilityCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace ui
