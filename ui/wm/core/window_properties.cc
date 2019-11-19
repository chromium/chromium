// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/window_properties.h"

#include "ui/wm/core/window_animations.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(WM_CORE_EXPORT,
                                       wm::WindowVisibilityAnimationTransition)

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(WM_CORE_EXPORT, float)

namespace wm {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kUsesScreenCoordinatesKey, false)
DEFINE_UI_CLASS_PROPERTY_KEY(base::TimeDelta,
                             kWindowVisibilityAnimationDurationKey,
                             base::TimeDelta())
DEFINE_UI_CLASS_PROPERTY_KEY(WindowVisibilityAnimationTransition,
                             kWindowVisibilityAnimationTransitionKey,
                             ANIMATE_BOTH)
DEFINE_UI_CLASS_PROPERTY_KEY(int,
                             kWindowVisibilityAnimationTypeKey,
                             WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT)
DEFINE_UI_CLASS_PROPERTY_KEY(float,
                             kWindowVisibilityAnimationVerticalPositionKey,
                             15.f)

}  // namespace wm
