// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/easy_resize_window_targeter.h"

#include <algorithm>

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"

namespace wm {

EasyResizeWindowTargeter::EasyResizeWindowTargeter(
    const gfx::Insets& mouse_extend,
    const gfx::Insets& touch_extend) {
  SetInsets(mouse_extend, touch_extend);
}

EasyResizeWindowTargeter::~EasyResizeWindowTargeter() {}

bool EasyResizeWindowTargeter::EventLocationInsideBounds(
    aura::Window* target,
    const ui::LocatedEvent& event) const {
  return WindowTargeter::EventLocationInsideBounds(target, event);
}

bool EasyResizeWindowTargeter::ShouldUseExtendedBounds(
    const aura::Window* w) const {
  DCHECK(window());
  // Use the extended bounds only for immediate child windows of window().
  // Use the default targeter otherwise.
  if (w->parent() != window())
    return false;

  // Only resizable windows benefit from the extended hit-test region.
  if ((w->GetProperty(aura::client::kResizeBehaviorKey) &
       aura::client::kResizeBehaviorCanResize) == 0) {
    return false;
  }

  // For transient children use extended bounds if a transient parent or if
  // transient parent's parent is a top level window in window().
  aura::client::TransientWindowClient* transient_window_client =
      aura::client::GetTransientWindowClient();
  const aura::Window* transient_parent =
      transient_window_client ? transient_window_client->GetTransientParent(w)
                              : nullptr;
  return !transient_parent || transient_parent == window() ||
         transient_parent->parent() == window();
}

}  // namespace wm
