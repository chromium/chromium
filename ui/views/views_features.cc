// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/views_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace views::features {

// Please keep alphabetized.

// Use a high-contrast style for ink drops when in platform high-contrast mode,
// including full opacity and a high-contrast color
BASE_FEATURE(kEnablePlatformHighContrastInkDrop,
             "EnablePlatformHighContrastInkDrop",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Only paint views that are invalidated/dirty (i.e. a paint was directly
// scheduled on those views) as opposed to painting all views that intersect
// an invalid rectangle on the layer.
BASE_FEATURE(kEnableViewPaintOptimization,
             "EnableViewPaintOptimization",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, widgets will be shown based on their z-order level
BASE_FEATURE(kWidgetLayering,
             "WidgetLayering",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace views::features
