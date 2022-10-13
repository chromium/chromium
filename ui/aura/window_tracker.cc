// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tracker.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "ui/aura/window.h"

namespace aura {

WindowTracker::WindowTracker(const WindowList& windows) {
  for (Window* window : windows)
    Add(window);
}

WindowTracker::WindowTracker() = default;

WindowTracker::~WindowTracker() {
  RemoveAll();
}

void WindowTracker::Add(Window* window) {
  if (base::Contains(windows_, window))
    return;

  window->AddObserver(this);
  windows_.push_back(window);
}

void WindowTracker::RemoveAll() {
  for (Window* window : windows_)
    window->RemoveObserver(this);
  windows_.clear();
}

void WindowTracker::Remove(Window* window) {
  auto iter = base::ranges::find(windows_, window);
  if (iter != windows_.end()) {
    window->RemoveObserver(this);
    windows_.erase(iter);
  }
}

Window* WindowTracker::Pop() {
  DCHECK(!windows_.empty());
  Window* result = windows_[0];
  Remove(result);
  return result;
}

bool WindowTracker::Contains(Window* window) const {
  return base::Contains(windows_, window);
}

void WindowTracker::OnWindowDestroying(Window* window) {
  DCHECK(Contains(window));
  Remove(window);
}

}  // namespace aura
