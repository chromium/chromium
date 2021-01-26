// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_COCOA_WATCHER_MAC_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_COCOA_WATCHER_MAC_H_

#include <objc/objc.h>

#include "base/callback.h"
#include "base/macros.h"
#include "ui/views/views_export.h"

namespace views {

// This class executes a callback when a native menu begins tracking, or when a
// new window takes focus. With native menus, each one automatically closes when
// a new one begins tracking, and MenuPreTargetHandlerAura::OnWindowActivated()
// closes menus when new windows take focus. This allows Views menus to have the
// correct behavior.
class VIEWS_EXPORT MenuCocoaWatcherMac {
 public:
  explicit MenuCocoaWatcherMac(base::OnceClosure callback);
  ~MenuCocoaWatcherMac();

 private:
  void ExecuteCallback();

  // The closure to call when the notification comes in.
  base::OnceClosure callback_;

  // Tokens representing the notification observers.
  id observer_token_other_menu_;
  id observer_token_new_window_focus_;
  id observer_token_app_change_;

  DISALLOW_COPY_AND_ASSIGN(MenuCocoaWatcherMac);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_COCOA_WATCHER_MAC_H_
