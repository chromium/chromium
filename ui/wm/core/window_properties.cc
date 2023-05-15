// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/window_properties.h"

#include "ui/wm/core/window_animations.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(UI_WM),
                                       wm::WindowVisibilityAnimationTransition)

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(UI_WM), float)

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
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kWindowHidingAnimationCountKey, 0)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kPersistableKey, true)

}  // namespace wm
