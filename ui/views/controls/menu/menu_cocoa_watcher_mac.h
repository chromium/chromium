// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_COCOA_WATCHER_MAC_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_COCOA_WATCHER_MAC_H_

#include <memory>

#include "base/functional/callback.h"
#include "ui/views/views_export.h"

namespace views {

enum class MacNotificationFilter {
  DontIgnoreNotifications,
  IgnoreWorkspaceNotifications,
  IgnoreAllNotifications
};

// This class executes a callback when a native menu begins tracking, or when a
// new window takes focus. With native menus, each one automatically closes when
// a new one begins tracking, and MenuPreTargetHandlerAura::OnWindowActivated()
// closes menus when new windows take focus. This allows Views menus to have the
// correct behavior.
class VIEWS_EXPORT MenuCocoaWatcherMac {
 public:
  // Forces all MenuCocoaWatcherMac instances to ignore certain NSNotifications.
  static void SetNotificationFilterForTesting(MacNotificationFilter filter);

  explicit MenuCocoaWatcherMac(base::OnceClosure callback);

  MenuCocoaWatcherMac(const MenuCocoaWatcherMac&) = delete;
  MenuCocoaWatcherMac& operator=(const MenuCocoaWatcherMac&) = delete;

  ~MenuCocoaWatcherMac();

 private:
  void ExecuteCallback();

  // The closure to call when the notification comes in.
  base::OnceClosure callback_;

  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_COCOA_WATCHER_MAC_H_
