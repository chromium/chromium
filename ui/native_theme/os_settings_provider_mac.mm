// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/os_settings_provider.h"

#import <Accessibility/Accessibility.h>
#import <Cocoa/Cocoa.h>

#include <optional>

#include "ui/base/cocoa/defaults_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/os_settings_provider_mac.h"

// Helper object to respond to light mode/dark mode changeovers.
@interface EffectiveAppearanceObserver : NSObject
@end

@implementation EffectiveAppearanceObserver {
  void (^_handler)() __strong;
}

- (instancetype)initWithHandler:(void (^)())handler {
  self = [super init];
  if (self) {
    _handler = handler;
    [NSApp addObserver:self
            forKeyPath:@"effectiveAppearance"
               options:0
               context:nullptr];
  }
  return self;
}

- (void)dealloc {
  [NSApp removeObserver:self forKeyPath:@"effectiveAppearance"];
}

- (void)observeValueForKeyPath:(NSString*)forKeyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  _handler();
}

@end

namespace ui {

struct OsSettingsProviderMac::ObjCMembers {
  id __strong non_blinking_cursor_token;
  id __strong display_accessibility_notification_token;
  EffectiveAppearanceObserver* __strong appearance_observer;
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

  objc_members_->appearance_observer =
      [[EffectiveAppearanceObserver alloc] initWithHandler:^{
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

NativeTheme::PreferredColorScheme OsSettingsProviderMac::PreferredColorScheme()
    const {
  NSAppearanceName appearance =
      [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:@[
        NSAppearanceNameAqua, NSAppearanceNameDarkAqua
      ]];
  return [appearance isEqual:NSAppearanceNameDarkAqua]
             ? NativeTheme::PreferredColorScheme::kDark
             : NativeTheme::PreferredColorScheme::kLight;
}

NativeTheme::PreferredContrast OsSettingsProviderMac::PreferredContrast()
    const {
  return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldIncreaseContrast
             ? NativeTheme::PreferredContrast::kMore
             : OsSettingsProvider::PreferredContrast();
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
  return TextInsertionCaretBlinkPeriodFromDefaults().value_or(
      OsSettingsProvider::CaretBlinkInterval());
}

}  // namespace ui
