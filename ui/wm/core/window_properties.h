// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_WINDOW_PROPERTIES_H_
#define UI_WM_CORE_WINDOW_PROPERTIES_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "ui/base/class_property.h"

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
COMPONENT_EXPORT(UI_WM)
extern const ui::ClassProperty<bool>* const kUsesScreenCoordinatesKey;

COMPONENT_EXPORT(UI_WM)
extern const ui::ClassProperty<base::TimeDelta>* const
    kWindowVisibilityAnimationDurationKey;

COMPONENT_EXPORT(UI_WM)
extern const ui::ClassProperty<WindowVisibilityAnimationTransition>* const
    kWindowVisibilityAnimationTransitionKey;

COMPONENT_EXPORT(UI_WM)
extern const ui::ClassProperty<int>* const kWindowVisibilityAnimationTypeKey;

// Used if the animation-type is WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL.
COMPONENT_EXPORT(UI_WM)
extern const ui::ClassProperty<float>* const
    kWindowVisibilityAnimationVerticalPositionKey;

// The number of hiding animations in progress on the window.
COMPONENT_EXPORT(UI_WM)
extern const ui::ClassProperty<int32_t>* const kWindowHidingAnimationCountKey;

COMPONENT_EXPORT(UI_WM)
extern const ui::ClassProperty<bool>* const kPersistableKey;

}  // namespace wm

// These need to be declared here for jumbo builds.
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(UI_WM),
                                        wm::WindowVisibilityAnimationTransition)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(UI_WM), float)

#endif  // UI_WM_CORE_WINDOW_PROPERTIES_H_
