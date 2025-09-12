// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/os_settings_provider.h"

#import <Accessibility/Accessibility.h>
#import <Cocoa/Cocoa.h>

#include <optional>

#include "base/no_destructor.h"
#include "ui/base/cocoa/defaults_utils.h"
#include "ui/native_theme/os_settings_provider_mac.h"

namespace ui {

struct OsSettingsProviderMac::ObjCMembers {
  id __strong non_blinking_cursor_token;
  id __strong display_accessibility_notification_token;
};

OsSettingsProviderMac::OsSettingsProviderMac()
    : OsSettingsProvider(PriorityLevel::kProduction) {
  objc_members_ = std::make_unique<ObjCMembers>();

  __block auto provider = this;
  if (@available(macOS 15.0, *)) {
    objc_members_->non_blinking_cursor_token =
        [[NSNotificationCenter defaultCenter]
            addObserverForName:
                AXPrefersNonBlinkingTextInsertionIndicatorDidChangeNotification
                        object:nil
                         queue:nil
                    usingBlock:^(NSNotification* notification) {
                      provider->NotifyOnSettingsChanged();
                    }];
  }

  objc_members_->display_accessibility_notification_token =
      [NSWorkspace.sharedWorkspace.notificationCenter
          addObserverForName:
              NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                    provider->NotifyOnSettingsChanged();
                  }];
}

OsSettingsProviderMac::~OsSettingsProviderMac() {
  [NSNotificationCenter.defaultCenter
      removeObserver:objc_members_->display_accessibility_notification_token];
  if (@available(macOS 15.0, *)) {
    [NSNotificationCenter.defaultCenter
        removeObserver:objc_members_->non_blinking_cursor_token];
  }
}

bool OsSettingsProviderMac::PrefersReducedTransparency() const {
  return NSWorkspace.sharedWorkspace
      .accessibilityDisplayShouldReduceTransparency;
}

bool OsSettingsProviderMac::PrefersInvertedColors() const {
  return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldInvertColors;
}

base::TimeDelta OsSettingsProviderMac::CaretBlinkInterval() const {
  if (@available(macOS 15.0, *)) {
    if (AXPrefersNonBlinkingTextInsertionIndicator()) {
      return base::TimeDelta();
    }
  }

  // If there's insertion point flash rate info in NSUserDefaults, use the
  // blink period derived from that.
  return ui::TextInsertionCaretBlinkPeriodFromDefaults().value_or(
      OsSettingsProvider::CaretBlinkInterval());
}

}  // namespace ui
