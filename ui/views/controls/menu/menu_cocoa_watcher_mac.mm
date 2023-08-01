// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_cocoa_watcher_mac.h"

#import <Cocoa/Cocoa.h>
#include <dispatch/dispatch.h>

#include <memory>
#include <utility>

namespace views {
namespace {

// Returns the global notification filter.
MacNotificationFilter& NotificationFilterInternal() {
  static MacNotificationFilter filter =
      MacNotificationFilter::DontIgnoreNotifications;
  return filter;
}

// Returns YES if `notification` should be ignored based on the current value of
// the notification filter.
BOOL ShouldIgnoreNotification(NSNotification* notification) {
  switch (NotificationFilterInternal()) {
    case MacNotificationFilter::DontIgnoreNotifications:
      return NO;
    case MacNotificationFilter::IgnoreWorkspaceNotifications:
      return [notification.name
          isEqualToString:NSWorkspaceDidActivateApplicationNotification];
    case MacNotificationFilter::IgnoreAllNotifications:
      return YES;
  }

  return NO;
}
}  // namespace

struct MenuCocoaWatcherMac::ObjCStorage {
  // Tokens representing the notification observers.
  id __strong observer_token_other_menu;
  id __strong observer_token_new_window_focus;
  id __strong observer_token_app_change;
};

MenuCocoaWatcherMac::MenuCocoaWatcherMac(base::OnceClosure callback)
    : callback_(std::move(callback)),
      objc_storage_(std::make_unique<ObjCStorage>()) {
  objc_storage_->observer_token_other_menu =
      [[NSNotificationCenter defaultCenter]
          addObserverForName:NSMenuDidBeginTrackingNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                    if (ShouldIgnoreNotification(notification)) {
                      return;
                    }

                    ExecuteCallback();
                  }];
  objc_storage_->observer_token_new_window_focus =
      [[NSNotificationCenter defaultCenter]
          addObserverForName:NSWindowDidBecomeKeyNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                    if (ShouldIgnoreNotification(notification)) {
                      return;
                    }

                    ExecuteCallback();
                  }];
  objc_storage_->observer_token_app_change =
      [[[NSWorkspace sharedWorkspace] notificationCenter]
          addObserverForName:NSWorkspaceDidActivateApplicationNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                    if (ShouldIgnoreNotification(notification))
                      return;

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
  [NSNotificationCenter.defaultCenter
      removeObserver:objc_storage_->observer_token_other_menu];
  [NSNotificationCenter.defaultCenter
      removeObserver:objc_storage_->observer_token_new_window_focus];
  [NSWorkspace.sharedWorkspace.notificationCenter
      removeObserver:objc_storage_->observer_token_app_change];
}

void MenuCocoaWatcherMac::SetNotificationFilterForTesting(
    MacNotificationFilter filter) {
  NotificationFilterInternal() = filter;
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
