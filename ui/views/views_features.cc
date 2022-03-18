// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/views_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace views {
namespace features {

// Please keep alphabetized.

// Use a high-contrast style for ink drops when in platform high-contrast mode,
// including full opacity and a high-contrast color
const base::Feature kEnablePlatformHighContrastInkDrop{
    "EnablePlatformHighContrastInkDrop", base::FEATURE_DISABLED_BY_DEFAULT};

// Only paint views that are invalidated/dirty (i.e. a paint was directly
// scheduled on those views) as opposed to painting all views that intersect
// an invalid rectangle on the layer.
const base::Feature kEnableViewPaintOptimization{
    "EnableViewPaintOptimization", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_MAC)
// Pushing of fullscreen control from the cross-platform Widget level down to
// the native NativeWidgetMac level and below. Once this lands and all bugs and
// flakes are mopped up, this feature will be removed.
// https://crbug.com/1302857
const base::Feature kFullscreenControllerMac{"FullscreenControllerMac",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace features
}  // namespace views
