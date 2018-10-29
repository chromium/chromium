// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views_bridge_mac/browser_native_widget_window_mac.h"

#import <AppKit/AppKit.h>

#include "ui/views_bridge_mac/bridged_native_widget_impl.h"
#include "ui/views_bridge_mac/mojo/bridged_native_widget_host.mojom.h"

@interface NSWindow (PrivateBrowserNativeWidgetAPI)
+ (Class)frameViewClassForStyleMask:(NSUInteger)windowStyle;
@end

@interface NSThemeFrame (PrivateBrowserNativeWidgetAPI)
- (CGFloat)_titlebarHeight;
- (void)setStyleMask:(NSUInteger)styleMask;
@end

@interface BrowserWindowFrame : NativeWidgetMacNSWindowTitledFrame
@end

@implementation BrowserWindowFrame {
  BOOL _inFullScreen;
}

// NSThemeFrame overrides.

- (CGFloat)_titlebarHeight {
  bool overrideTitlebarHeight = false;
  float titlebarHeight = 0;

  if (!_inFullScreen) {
    auto* window = base::mac::ObjCCast<NativeWidgetMacNSWindow>([self window]);
    views::BridgedNativeWidgetImpl* bridgeImpl = [window bridgeImpl];
    if (bridgeImpl) {
      bridgeImpl->host()->GetWindowFrameTitlebarHeight(&overrideTitlebarHeight,
                                                       &titlebarHeight);
    }
  }
  if (overrideTitlebarHeight)
    return titlebarHeight;
  return [super _titlebarHeight];
}

- (void)setStyleMask:(NSUInteger)styleMask {
  _inFullScreen = (styleMask & NSWindowStyleMaskFullScreen) != 0;
  [super setStyleMask:styleMask];
}

- (BOOL)_shouldCenterTrafficLights {
  return YES;
}

// The base implementation justs tests [self class] == [NSThemeFrame class].
- (BOOL)_shouldFlipTrafficLightsForRTL API_AVAILABLE(macos(10.12)) {
  return [[self window] windowTitlebarLayoutDirection] ==
         NSUserInterfaceLayoutDirectionRightToLeft;
}

// On 10.10, this prevents the window server from treating the title bar as an
// unconditionally-draggable region, and allows -[BridgedContentView hitTest:]
// to choose case-by-case whether to take a mouse event or let it turn into a
// window drag. Not needed for newer macOS. See r549802 for details.
- (NSRect)_draggableFrame NS_DEPRECATED_MAC(10_10, 10_11) {
  return NSZeroRect;
}

@end

@implementation BrowserNativeWidgetWindow

// NSWindow (PrivateAPI) overrides.

+ (Class)frameViewClassForStyleMask:(NSUInteger)windowStyle {
  // - NSThemeFrame and its subclasses will be nil if it's missing at runtime.
  if ([BrowserWindowFrame class])
    return [BrowserWindowFrame class];
  return [super frameViewClassForStyleMask:windowStyle];
}

// The base implementation returns YES if the window's frame view is a custom
// class, which causes undesirable changes in behavior. AppKit NSWindow
// subclasses are known to override it and return NO.
- (BOOL)_usesCustomDrawing {
  return NO;
}

// Handle "Move focus to the window toolbar" configured in System Preferences ->
// Keyboard -> Shortcuts -> Keyboard. Usually Ctrl+F5. The argument (|unknown|)
// tends to just be nil.
- (void)_handleFocusToolbarHotKey:(id)unknown {
  views::BridgedNativeWidgetImpl* bridgeImpl = [self bridgeImpl];
  if (bridgeImpl)
    bridgeImpl->host()->OnFocusWindowToolbar();
}

@end
