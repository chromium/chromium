// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_EXTRA_WINDOW_POSITION_IN_ROOT_MONITOR_H_
#define UI_AURA_EXTRA_WINDOW_POSITION_IN_ROOT_MONITOR_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/aura_extra/aura_extra_export.h"

namespace aura_extra {

// WindowPositionInRootMonitor notifies a callback any time the position of a
// window, relative to the root, changes. Changes are only sent when attached
// to a valid root.
class AURA_EXTRA_EXPORT WindowPositionInRootMonitor
    : public aura::WindowObserver {
 public:
  WindowPositionInRootMonitor(aura::Window* window,
                              base::RepeatingClosure callback);

  WindowPositionInRootMonitor(const WindowPositionInRootMonitor&) = delete;
  WindowPositionInRootMonitor& operator=(const WindowPositionInRootMonitor&) =
      delete;

  ~WindowPositionInRootMonitor() override;

 private:
  // Adds |window| and all its ancestors to |ancestors_|.
  void AddAncestors(aura::Window* window);

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  base::RepeatingClosure callback_;

  // The windows being watched. This contains the window supplied to the
  // constructor and all it's ancestors. This is empty if the window is deleted.
  std::vector<raw_ptr<aura::Window, VectorExperimental>> ancestors_;
};

}  // namespace aura_extra

#endif  // UI_AURA_EXTRA_WINDOW_POSITION_IN_ROOT_MONITOR_H_
