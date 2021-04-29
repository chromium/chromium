// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_FOCUS_RULES_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_FOCUS_RULES_H_

#include "base/macros.h"
#include "ui/wm/core/base_focus_rules.h"

namespace views {

class DesktopFocusRules : public wm::BaseFocusRules {
 public:
  explicit DesktopFocusRules(aura::Window* content_window);
  ~DesktopFocusRules() override;

 private:
  // Overridden from wm::BaseFocusRules:
  bool CanActivateWindow(const aura::Window* window) const override;
  bool CanFocusWindow(const aura::Window* window,
                      const ui::Event* event) const override;
  bool SupportsChildActivation(const aura::Window* window) const override;
  bool IsWindowConsideredVisibleForActivation(
      const aura::Window* window) const override;
  const aura::Window* GetToplevelWindow(
      const aura::Window* window) const override;
  aura::Window* GetNextActivatableWindow(aura::Window* window) const override;

  // The content window. This is an activatable window even though it is a
  // child.
  aura::Window* content_window_;

  DISALLOW_COPY_AND_ASSIGN(DesktopFocusRules);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_FOCUS_RULES_H_
