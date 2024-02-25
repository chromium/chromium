// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TRACKER_H_
#define UI_AURA_WINDOW_TRACKER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_observer.h"

namespace aura {

// This class is used to track an ordered list of Windows. When a Window is
// destroyed it is removed from the list of Windows.
class AURA_EXPORT WindowTracker : public WindowObserver {
 public:
  // A vector is used for tracking the windows (instead of a set) as some places
  // care about ordering.
  using WindowList = std::vector<raw_ptr<Window, VectorExperimental>>;

  explicit WindowTracker(const WindowList& windows);
  WindowTracker();

  WindowTracker(const WindowTracker&) = delete;
  WindowTracker& operator=(const WindowTracker&) = delete;

  ~WindowTracker() override;

  // Returns the set of windows being observed.
  const WindowList& windows() const { return windows_; }

  // Adds |window| to the set of Windows being tracked.
  void Add(Window* window);

  void RemoveAll();

  // Removes |window| from the set of windows being tracked.
  void Remove(Window* window);

  Window* Pop();

  // Returns true if |window| was previously added and has not been removed or
  // deleted.
  bool Contains(Window* window) const;

  // WindowObserver overrides:
  void OnWindowDestroying(Window* window) override;

 private:
  WindowList windows_;
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TRACKER_H_
