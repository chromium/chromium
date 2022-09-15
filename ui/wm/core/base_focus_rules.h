// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_BASE_FOCUS_RULES_H_
#define UI_WM_CORE_BASE_FOCUS_RULES_H_

#include "base/component_export.h"
#include "ui/wm/core/focus_rules.h"

namespace wm {

// A set of basic focus and activation rules. Specializations should most likely
// subclass this and call up to these methods rather than reimplementing them.
class COMPONENT_EXPORT(UI_WM) BaseFocusRules : public FocusRules {
 public:
  BaseFocusRules(const BaseFocusRules&) = delete;
  BaseFocusRules& operator=(const BaseFocusRules&) = delete;

 protected:
  BaseFocusRules();
  ~BaseFocusRules() override;

  // Returns true if the children of |window| can be activated.
  virtual bool SupportsChildActivation(const aura::Window* window) const = 0;

  // Returns true if |window| is considered visible for activation purposes.
  virtual bool IsWindowConsideredVisibleForActivation(
      const aura::Window* window) const;

  // Overridden from FocusRules:
  bool IsToplevelWindow(const aura::Window* window) const override;
  bool CanActivateWindow(const aura::Window* window) const override;
  bool CanFocusWindow(const aura::Window* window,
                      const ui::Event* event) const override;
  const aura::Window* GetToplevelWindow(
      const aura::Window* window) const override;
  aura::Window* GetActivatableWindow(aura::Window* window) const override;
  aura::Window* GetFocusableWindow(aura::Window* window) const override;
  aura::Window* GetNextActivatableWindow(aura::Window* ignore) const override;

 private:
  aura::Window* GetToplevelWindow(aura::Window* window) const {
    return const_cast<aura::Window*>(
        GetToplevelWindow(const_cast<const aura::Window*>(window)));
  }
  const aura::Window* GetActivatableWindow(const aura::Window* window) const;
};

}  // namespace wm

#endif  // UI_WM_CORE_BASE_FOCUS_RULES_H_
