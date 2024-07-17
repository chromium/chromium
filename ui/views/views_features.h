// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEWS_FEATURES_H_
#define UI_VIEWS_VIEWS_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/views/views_export.h"

namespace views::features {

// Please keep alphabetized.
VIEWS_EXPORT BASE_DECLARE_FEATURE(kEnablePlatformHighContrastInkDrop);
VIEWS_EXPORT BASE_DECLARE_FEATURE(kEnableViewPaintOptimization);
VIEWS_EXPORT BASE_DECLARE_FEATURE(kKeyboardAccessibleTooltipInViews);
VIEWS_EXPORT BASE_DECLARE_FEATURE(kAnnounceTextAdditionalAttributes);

}  // namespace views::features

#endif  // UI_VIEWS_VIEWS_FEATURES_H_
