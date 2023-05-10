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
class SelectionBound;
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
  // if one is already being shown. `focus_bound` specifies the selection bound
  // that the magnifier should zoom in on. Roughly, this is a line segment
  // representing a selection endpoint or caret position and is generally
  // vertical or horizontal (depending on text orientation). `focus_bound` is
  // specified in coordinates of the context window.
  virtual void ShowMagnifier(aura::Window* context,
                             const gfx::SelectionBound& focus_bound) = 0;

  // Closes and stops showing the running magnifier.
  virtual void CloseMagnifier() = 0;

  // Returns true if the runner is currently showing a magnifier.
  virtual bool IsRunning() const = 0;

 protected:
  TouchSelectionMagnifierRunner();
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_SELECTION_MAGNIFIER_RUNNER_H_
