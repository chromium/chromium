// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_WINDOW_ANIMATIONS_H_
#define UI_WM_CORE_WINDOW_ANIMATIONS_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/wm/core/window_properties.h"

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
COMPONENT_EXPORT(UI_WM)
void SetWindowVisibilityAnimationType(aura::Window* window, int type);
COMPONENT_EXPORT(UI_WM)
int GetWindowVisibilityAnimationType(aura::Window* window);

COMPONENT_EXPORT(UI_WM)
void SetWindowVisibilityAnimationTransition(
    aura::Window* window,
    WindowVisibilityAnimationTransition transition);

COMPONENT_EXPORT(UI_WM)
bool HasWindowVisibilityAnimationTransition(
    aura::Window* window,
    WindowVisibilityAnimationTransition transition);

COMPONENT_EXPORT(UI_WM)
void SetWindowVisibilityAnimationDuration(aura::Window* window,
                                          const base::TimeDelta& duration);

COMPONENT_EXPORT(UI_WM)
base::TimeDelta GetWindowVisibilityAnimationDuration(
    const aura::Window& window);

COMPONENT_EXPORT(UI_WM)
void SetWindowVisibilityAnimationVerticalPosition(aura::Window* window,
                                                  float position);

class ImplicitHidingWindowAnimationObserver;
// A wrapper of ui::ScopedLayerAnimationSettings for implicit hiding animations.
// Use this to ensure that the hiding animation is visible even after
// the window is deleted or deactivated, instead of using
// ui::ScopedLayerAnimationSettings directly.
class COMPONENT_EXPORT(UI_WM) ScopedHidingAnimationSettings {
 public:
  explicit ScopedHidingAnimationSettings(aura::Window* window);

  ScopedHidingAnimationSettings(const ScopedHidingAnimationSettings&) = delete;
  ScopedHidingAnimationSettings& operator=(
      const ScopedHidingAnimationSettings&) = delete;

  ~ScopedHidingAnimationSettings();

  // Returns the wrapped ScopedLayeAnimationSettings instance.
  ui::ScopedLayerAnimationSettings* layer_animation_settings() {
    return &layer_animation_settings_;
  }

 private:
  ui::ScopedLayerAnimationSettings layer_animation_settings_;
  raw_ptr<ImplicitHidingWindowAnimationObserver> observer_;
};

// Returns false if the |window| didn't animate.
COMPONENT_EXPORT(UI_WM)
bool AnimateOnChildWindowVisibilityChanged(aura::Window* window, bool visible);
COMPONENT_EXPORT(UI_WM)
bool AnimateWindow(aura::Window* window, WindowAnimationType type);

// Returns true if window animations are disabled for |window|. Window
// animations are enabled by default. If |window| is nullptr, this just checks
// if the global flag disabling window animations is present.
COMPONENT_EXPORT(UI_WM) bool WindowAnimationsDisabled(aura::Window* window);

}  // namespace wm

#endif  // UI_WM_CORE_WINDOW_ANIMATIONS_H_
