// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_extra/window_position_in_root_monitor.h"

#include "ui/aura/window.h"

namespace aura_extra {

WindowPositionInRootMonitor::WindowPositionInRootMonitor(
    aura::Window* window,
    base::RepeatingClosure callback)
    : callback_(std::move(callback)) {
  DCHECK(window);
  AddAncestors(window);
}

WindowPositionInRootMonitor::~WindowPositionInRootMonitor() {
  for (aura::Window* ancestor : ancestors_)
    ancestor->RemoveObserver(this);
}

void WindowPositionInRootMonitor::AddAncestors(aura::Window* window) {
  while (window) {
    ancestors_.push_back(window);
    window->AddObserver(this);
    window = window->parent();
  }
}

void WindowPositionInRootMonitor::OnWindowDestroyed(aura::Window* window) {
  // This should only be hit when window has no ancestors (because destroying
  // a window implicitly removes children).
  DCHECK_EQ(1u, ancestors_.size());
  DCHECK_EQ(window, ancestors_[0]);
  window->RemoveObserver(this);
  ancestors_.clear();
}

void WindowPositionInRootMonitor::OnWindowParentChanged(aura::Window* window,
                                                        aura::Window* parent) {
  // |window|'s parent is now |parent|. Iterate through the list backwards,
  // removing windows until |window| is found. Then add all the new ancestors
  // of |window|.
  while (!ancestors_.empty()) {
    if (ancestors_.back() == window) {
      AddAncestors(parent);
      // When adding to a root, notify the callback.
      if (window->GetRootWindow())
        callback_.Run();
      return;
    }
    ancestors_.back()->RemoveObserver(this);
    ancestors_.pop_back();
  }
  NOTREACHED_IN_MIGRATION();
}

void WindowPositionInRootMonitor::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (old_bounds.origin() != new_bounds.origin() &&
      ancestors_.back()->GetRootWindow()) {
    callback_.Run();
  }
}

}  // namespace aura_extra
