// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_cocoa_watcher_mac.h"

#import <Cocoa/Cocoa.h>
#include <dispatch/dispatch.h>

#import <utility>

namespace views {

MenuCocoaWatcherMac::MenuCocoaWatcherMac(base::OnceClosure callback)
    : callback_(std::move(callback)) {
  observer_token_other_menu_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:NSMenuDidBeginTrackingNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                ExecuteCallback();
              }];
  observer_token_new_window_focus_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:NSWindowDidBecomeKeyNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                ExecuteCallback();
              }];
  observer_token_app_change_ =
      [[[NSWorkspace sharedWorkspace] notificationCenter]
          addObserverForName:NSWorkspaceDidActivateApplicationNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                    // Only destroy menus if the browser is losing focus, not if
                    // it's gaining focus. This is to ensure that we can invoke
                    // a context menu while focused on another app, and still be
                    // able to click on menu items without dismissing the menu.
                    if (![[NSRunningApplication currentApplication] isActive]) {
                      ExecuteCallback();
                    }
                  }];
}

MenuCocoaWatcherMac::~MenuCocoaWatcherMac() {
  [[NSNotificationCenter defaultCenter]
      removeObserver:observer_token_other_menu_];
  [[NSNotificationCenter defaultCenter]
      removeObserver:observer_token_new_window_focus_];
  [[[NSWorkspace sharedWorkspace] notificationCenter]
      removeObserver:observer_token_app_change_];
}

void MenuCocoaWatcherMac::ExecuteCallback() {
  __block base::OnceClosure callback = std::move(callback_);
  dispatch_async(dispatch_get_main_queue(), ^{
    if (callback) {
      std::move(callback).Run();
    }
  });
}

}  // namespace views
