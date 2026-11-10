// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/appkit_utils.h"

#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <cmath>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/mac/mac_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Values of com.apple.trackpad.forceClick corresponding to "Look up & data
// detectors" in System Preferences -> Trackpad -> Point & Click.
enum class ForceTouchAction {
  NONE = 0,        // Unchecked or set to "Tap with three fingers".
  QUICK_LOOK = 1,  // Set to "Force Click with one finger".
};

// The global preference backing "Keyboard navigation" in System Settings, and
// the bit in it that indicates that full keyboard access is enabled. This
// matches what HIToolbox reads to implement
// -[NSApplication isFullKeyboardAccessEnabled].
const CFStringRef kKeyboardUIModeKey = CFSTR("AppleKeyboardUIMode");
constexpr int kKeyboardUIModeFullKeyboardAccessBit = 1 << 1;

ui::FullKeyboardAccessGetter g_full_keyboard_access_getter_override = nullptr;

}  // namespace

namespace ui {

bool ForceClickInvokesQuickLook() {
  return [NSUserDefaults.standardUserDefaults
             integerForKey:@"com.apple.trackpad.forceClick"] ==
         static_cast<NSInteger>(ForceTouchAction::QUICK_LOOK);
}

bool IsCGFloatEqual(CGFloat a, CGFloat b) {
  return std::fabs(a - b) <= std::numeric_limits<CGFloat>::epsilon();
}

bool IsActiveApplication() {
  return NSRunningApplication.currentApplication.active;
}

bool IsFullKeyboardAccessEnabled() {
  if (g_full_keyboard_access_getter_override) {
    return g_full_keyboard_access_getter_override();
  }

  // Synchronize first so that a change just made by another process (e.g.
  // System Settings) is observed.
  CFPreferencesSynchronize(kCFPreferencesAnyApplication,
                           kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
  base::apple::ScopedCFTypeRef<CFPropertyListRef> value(
      CFPreferencesCopyValue(kKeyboardUIModeKey, kCFPreferencesAnyApplication,
                             kCFPreferencesCurrentUser, kCFPreferencesAnyHost));
  CFNumberRef number = base::apple::CFCast<CFNumberRef>(value.get());
  int mode = 0;
  if (!number || !CFNumberGetValue(number, kCFNumberIntType, &mode)) {
    return false;
  }
  return mode & kKeyboardUIModeFullKeyboardAccessBit;
}

void SetFullKeyboardAccessGetterForTesting(FullKeyboardAccessGetter getter) {
  g_full_keyboard_access_getter_override = getter;
}

bool PasteMightBlockWithPrivacyAlert() {
  if (@available(macOS 15.4, *)) {
    NSPasteboardAccessBehavior behavior =
        NSPasteboard.generalPasteboard.accessBehavior;
    return behavior == NSPasteboardAccessBehaviorDefault ||
           behavior == NSPasteboardAccessBehaviorAsk;
  } else {
    return false;
  }
}

UTType* UTTypeForServicesType(NSString* type) {
  if (!type) {
    return nil;
  }

  // `type` is either a modern UTType identifier, or an obsolete Pboard type.
  // Try one, then the other.
  UTType* uttype = [UTType typeWithIdentifier:type];
  if (uttype) {
    return uttype;
  }

  // The UTType API provides two UTTagClass values: UTTagClassFilenameExtension
  // and UTTagClassMIMEType, but there are other internal ones. This is one of
  // the private internal tag classes, the tag class that corresponds to the
  // deprecated kUTTagClassNSPboardType.
  return [UTType typeWithTag:type
                    tagClass:@"com.apple.nspboard-type"
            conformingToType:nil];
}

NSSet<UTType*>* UTTypesForServicesTypeArray(NSArray* types) {
  NSMutableSet* result = [NSMutableSet set];
  for (NSString* type in types) {
    if (UTType* uttype = UTTypeForServicesType(type)) {
      [result addObject:uttype];
    }
  }
  return result;
}

}  // namespace ui
