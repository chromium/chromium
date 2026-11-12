// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_DECORATION_CONTROLLER_DELEGATE_H_
#define UI_WM_CORE_DECORATION_CONTROLLER_DELEGATE_H_

#include <cstdint>

#include "base/component_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace wm {

// DecorationControllerDelegate allows a user to modify a decoration on certain
// windows differently from the normal use case.
class COMPONENT_EXPORT(UI_WM) DecorationControllerDelegate {
 public:
  DecorationControllerDelegate() = default;
  virtual ~DecorationControllerDelegate() = default;

  virtual bool ShouldObserveWindow(const aura::Window* window) = 0;

  // Invoked when the shadow on |window| is to be modified, either normally from
  // activation change or manually.
  virtual bool ShouldShowShadowForWindow(const aura::Window* window) = 0;

  // Invoked to determine whether `window` should display a highlight border.
  virtual bool ShouldShowHighlightBorderForWindow(
      const aura::Window* window) = 0;

  // Invoked when the `window` property changes.
  virtual bool ShouldUpdateDecorationOnWindowPropertyChange(
      const aura::Window* window,
      const void* key,
      intptr_t old) = 0;

  // Invoked when the decoration on `window` is created to apply the window
  // color theme to its decoration.
  virtual void ApplyColorThemeToWindowDecoration(aura::Window* window) = 0;

  // Invoked to determine if the decoration should consider the curvature of the
  // `window`.
  virtual bool ShouldRoundDecorationForWindow(const aura::Window* window) = 0;
};

}  // namespace wm

#endif  // UI_WM_CORE_DECORATION_CONTROLLER_DELEGATE_H_
