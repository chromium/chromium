// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_ALERT_WINDOW_H_
#define UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_ALERT_WINDOW_H_

#include <memory>
#include <string>

#include "base/scoped_observer.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}

namespace views {
class AXAuraObjCache;

// This class provides a caller a way to alert an accessibility client such as
// ChromeVox with a text string without a backing visible window or view.
class VIEWS_EXPORT AccessibilityAlertWindow : public aura::EnvObserver {
 public:
  // |parent| is the window where a child alert window will be added.
  AccessibilityAlertWindow(aura::Window* parent, views::AXAuraObjCache* cache);
  AccessibilityAlertWindow(const AccessibilityAlertWindow&) = delete;
  AccessibilityAlertWindow& operator=(const AccessibilityAlertWindow&) = delete;
  ~AccessibilityAlertWindow() override;

  // Triggers an alert with the text |alert_string| to be sent to an
  // accessibility client.
  void HandleAlert(const std::string& alert_string);

 private:
  // aura::EnvObserver:
  void OnWillDestroyEnv() override;

  // The child alert window.
  std::unique_ptr<aura::Window> alert_window_;

  // The accessibility cache associated with |alert_window_|.
  views::AXAuraObjCache* cache_;

  ScopedObserver<aura::Env, aura::EnvObserver> observer_{this};
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_ACCESSIBILITY_ALERT_WINDOW_H_
