// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEWS_FEATURES_H_
#define UI_VIEWS_VIEWS_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/views/views_export.h"

namespace views {
namespace features {

// Please keep alphabetized.

VIEWS_EXPORT extern const base::Feature kEnablePlatformHighContrastInkDrop;
VIEWS_EXPORT extern const base::Feature kEnableViewPaintOptimization;
VIEWS_EXPORT extern const base::Feature kTextfieldFocusOnTapUp;
VIEWS_EXPORT extern const base::Feature kEnableNewBadgeOnMenuItems;

}  // namespace features
}  // namespace views

#endif  // UI_VIEWS_VIEWS_FEATURES_H_
