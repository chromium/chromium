// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_WINDOW_PROPERTIES_H_
#define UI_WM_CORE_WINDOW_PROPERTIES_H_

#include "ui/base/class_property.h"
#include "ui/wm/core/wm_core_export.h"

namespace wm {

// Type of visibility change transition that a window should animate.
// Default behavior is to animate both show and hide.
enum WindowVisibilityAnimationTransition {
  ANIMATE_SHOW = 0x1,
  ANIMATE_HIDE = 0x2,
  ANIMATE_BOTH = ANIMATE_SHOW | ANIMATE_HIDE,
  ANIMATE_NONE = 0x4,
};

// Alphabetical sort.

// Property to tell if the container uses screen coordinates for the child
// windows.
WM_CORE_EXPORT extern const ui::ClassProperty<bool>* const
    kUsesScreenCoordinatesKey;

WM_CORE_EXPORT extern const ui::ClassProperty<base::TimeDelta>* const
    kWindowVisibilityAnimationDurationKey;

WM_CORE_EXPORT extern const ui::ClassProperty<
    WindowVisibilityAnimationTransition>* const
    kWindowVisibilityAnimationTransitionKey;

WM_CORE_EXPORT extern const ui::ClassProperty<int>* const
    kWindowVisibilityAnimationTypeKey;

// Used if the animation-type is WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL.
WM_CORE_EXPORT extern const ui::ClassProperty<float>* const
    kWindowVisibilityAnimationVerticalPositionKey;

}  // namespace wm

// These need to be declared here for jumbo builds.
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(WM_CORE_EXPORT,
                                        wm::WindowVisibilityAnimationTransition)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(WM_CORE_EXPORT, float)

#endif  // UI_WM_CORE_WINDOW_PROPERTIES_H_
