// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_WINDOW_ANIMATIONS_H_
#define UI_WM_CORE_WINDOW_ANIMATIONS_H_

#include <vector>

#include "base/macros.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/wm/core/window_properties.h"
#include "ui/wm/core/wm_core_export.h"

namespace aura {
class Window;
}
namespace base {
class TimeDelta;
}

namespace wm {

// A variety of canned animations for window transitions.
enum WindowVisibilityAnimationType {
  WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT = 0,     // Default. Lets the system
                                                    // decide based on window
                                                    // type.
  WINDOW_VISIBILITY_ANIMATION_TYPE_DROP,            // Window shrinks in.
  WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL,        // Vertical Glenimation.
  WINDOW_VISIBILITY_ANIMATION_TYPE_FADE,            // Fades in/out.
  WINDOW_VISIBILITY_ANIMATION_TYPE_ROTATE,          // Window rotates in.

  // Downstream library animations start above this point.
  WINDOW_VISIBILITY_ANIMATION_MAX
};

// Canned animations that take effect once but don't have a symmetric pair as
// visibility animations do.
enum WindowAnimationType {
  WINDOW_ANIMATION_TYPE_BOUNCE = 0,  // Window scales up and down.
};

// These two methods use int for type rather than WindowVisibilityAnimationType
// since downstream libraries can extend the set of animations.
WM_CORE_EXPORT void SetWindowVisibilityAnimationType(aura::Window* window,
                                                     int type);
WM_CORE_EXPORT int GetWindowVisibilityAnimationType(aura::Window* window);

WM_CORE_EXPORT void SetWindowVisibilityAnimationTransition(
    aura::Window* window,
    WindowVisibilityAnimationTransition transition);

WM_CORE_EXPORT bool HasWindowVisibilityAnimationTransition(
    aura::Window* window,
    WindowVisibilityAnimationTransition transition);

WM_CORE_EXPORT void SetWindowVisibilityAnimationDuration(
    aura::Window* window,
    const base::TimeDelta& duration);

WM_CORE_EXPORT base::TimeDelta GetWindowVisibilityAnimationDuration(
    const aura::Window& window);

WM_CORE_EXPORT void SetWindowVisibilityAnimationVerticalPosition(
    aura::Window* window,
    float position);

class ImplicitHidingWindowAnimationObserver;
// A wrapper of ui::ScopedLayerAnimationSettings for implicit hiding animations.
// Use this to ensure that the hiding animation is visible even after
// the window is deleted or deactivated, instead of using
// ui::ScopedLayerAnimationSettings directly.
class WM_CORE_EXPORT ScopedHidingAnimationSettings {
 public:
  explicit ScopedHidingAnimationSettings(aura::Window* window);
  ~ScopedHidingAnimationSettings();

  // Returns the wrapped ScopedLayeAnimationSettings instance.
  ui::ScopedLayerAnimationSettings* layer_animation_settings() {
    return &layer_animation_settings_;
  }

 private:
  ui::ScopedLayerAnimationSettings layer_animation_settings_;
  ImplicitHidingWindowAnimationObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedHidingAnimationSettings);
};

// Returns false if the |window| didn't animate.
WM_CORE_EXPORT bool AnimateOnChildWindowVisibilityChanged(aura::Window* window,
                                                          bool visible);
WM_CORE_EXPORT bool AnimateWindow(aura::Window* window,
                                  WindowAnimationType type);

// Returns true if window animations are disabled for |window|. Window
// animations are enabled by default. If |window| is nullptr, this just checks
// if the global flag disabling window animations is present.
WM_CORE_EXPORT bool WindowAnimationsDisabled(aura::Window* window);

}  // namespace wm

#endif  // UI_WM_CORE_WINDOW_ANIMATIONS_H_
