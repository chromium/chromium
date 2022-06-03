// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_focus_rules.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/wm/core/window_util.h"

namespace views {

DesktopFocusRules::DesktopFocusRules(aura::Window* content_window)
    : content_window_(content_window) {}

DesktopFocusRules::~DesktopFocusRules() = default;

bool DesktopFocusRules::CanActivateWindow(const aura::Window* window) const {
  // The RootWindow is not activatable, only |content_window_| and children of
  // the RootWindow are considered activatable.
  if (window && window->IsRootWindow())
    return false;
  if (window && IsToplevelWindow(window) &&
      content_window_->GetRootWindow()->Contains(window) &&
      wm::WindowStateIs(window->GetRootWindow(), ui::SHOW_STATE_MINIMIZED)) {
    return true;
  }
  if (!BaseFocusRules::CanActivateWindow(window))
    return false;
  // Never activate a window that is not a child of the root window. Transients
  // spanning different DesktopNativeWidgetAuras may trigger this.
  return !window || content_window_->GetRootWindow()->Contains(window);
}

bool DesktopFocusRules::CanFocusWindow(const aura::Window* window,
                                       const ui::Event* event) const {
  return BaseFocusRules::CanFocusWindow(window, event) ||
         wm::WindowStateIs(window->GetRootWindow(), ui::SHOW_STATE_MINIMIZED);
}

bool DesktopFocusRules::SupportsChildActivation(
    const aura::Window* window) const {
  // In Desktop-Aura, only the content_window or children of the RootWindow are
  // activatable.
  return window->IsRootWindow();
}

bool DesktopFocusRules::IsWindowConsideredVisibleForActivation(
    const aura::Window* window) const {
  // |content_window_| is initially hidden and made visible from Show(). Even in
  // this state we still want it to be activatable.
  return BaseFocusRules::IsWindowConsideredVisibleForActivation(window) ||
         (window == content_window_);
}

const aura::Window* DesktopFocusRules::GetToplevelWindow(
    const aura::Window* window) const {
  const aura::Window* top_level_window =
      wm::BaseFocusRules::GetToplevelWindow(window);
  // In Desktop-Aura, only the content_window or children of the RootWindow are
  // considered as top level windows.
  if (top_level_window == content_window_->parent())
    return content_window_;
  return top_level_window;
}

aura::Window* DesktopFocusRules::GetNextActivatableWindow(
    aura::Window* window) const {
  aura::Window* next_activatable_window =
      wm::BaseFocusRules::GetNextActivatableWindow(window);
  // In Desktop-Aura the content_window_'s parent is a dummy window and thus
  // should never be activated. We should return the content_window_ if it
  // can be activated in this case.
  if (next_activatable_window == content_window_->parent() &&
      CanActivateWindow(content_window_))
    return content_window_;
  return next_activatable_window;
}

}  // namespace views
