// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_TOUCH_SELECTION_MAGNIFIER_RUNNER_H_
#define UI_TOUCH_SELECTION_TOUCH_SELECTION_MAGNIFIER_RUNNER_H_

#include "ui/touch_selection/ui_touch_selection_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class PointF;
}  // namespace gfx

namespace ui {

// An interface for the singleton object responsible for running the touch
// selection magnifier.
class UI_TOUCH_SELECTION_EXPORT TouchSelectionMagnifierRunner {
 public:
  TouchSelectionMagnifierRunner(const TouchSelectionMagnifierRunner&) = delete;
  TouchSelectionMagnifierRunner& operator=(
      const TouchSelectionMagnifierRunner&) = delete;

  virtual ~TouchSelectionMagnifierRunner();

  static TouchSelectionMagnifierRunner* GetInstance();

  // Creates and shows a magnifier, or updates the running magnifier's position
  // if the runner is already showing a magnifier. `position` specifies
  // coordinates in the context window.
  virtual void ShowMagnifier(aura::Window* context,
                             const gfx::PointF& position) = 0;

  // Closes and stops showing the running magnifier.
  virtual void CloseMagnifier() = 0;

  // Returns true if the runner is currently showing a magnifier.
  virtual bool IsRunning() const = 0;

 protected:
  TouchSelectionMagnifierRunner();
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_SELECTION_MAGNIFIER_RUNNER_H_
