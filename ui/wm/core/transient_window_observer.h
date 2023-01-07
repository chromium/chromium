// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_TRANSIENT_WINDOW_OBSERVER_H_
#define UI_WM_CORE_TRANSIENT_WINDOW_OBSERVER_H_

#include "base/component_export.h"

namespace aura {
class Window;
}

namespace wm {

class COMPONENT_EXPORT(UI_WM) TransientWindowObserver {
 public:
  // Called when a transient child is added to |window|.
  virtual void OnTransientChildAdded(aura::Window* window,
                                     aura::Window* transient) {}

  // Called when a transient child is removed from |window|.
  virtual void OnTransientChildRemoved(aura::Window* window,
                                       aura::Window* transient) {}

  virtual void OnTransientParentChanged(aura::Window* new_parent) {}

 protected:
  virtual ~TransientWindowObserver() {}
};

}  // namespace wm

#endif  // UI_WM_CORE_TRANSIENT_WINDOW_OBSERVER_H_
