// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/test/scoped_fake_full_keyboard_access.h"

#import <Cocoa/Cocoa.h>

#include <ostream>

#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/check_op.h"
#import "ui/base/cocoa/appkit_utils.h"

namespace {

ui::test::ScopedFakeFullKeyboardAccess* g_instance = nullptr;

bool FakeIsFullKeyboardAccessEnabled() {
  CHECK(g_instance, base::NotFatalUntil::M163);
  return g_instance->full_keyboard_access_state();
}

}  // namespace

// Donates a testing implementation of [NSApp isFullKeyboardAccessEnabled].
@interface FakeNSAppKeyboardAccessDonor : NSObject
@end

@implementation FakeNSAppKeyboardAccessDonor

- (BOOL)isFullKeyboardAccessEnabled {
  return FakeIsFullKeyboardAccessEnabled();
}

@end

namespace ui::test {

ScopedFakeFullKeyboardAccess::ScopedFakeFullKeyboardAccess()
    : full_keyboard_access_state_(true),
      swizzler_(new base::apple::ScopedObjCClassSwizzler(
          [NSApplication class],
          [FakeNSAppKeyboardAccessDonor class],
          @selector(isFullKeyboardAccessEnabled))) {
  DCHECK(!g_instance)
      << "Cannot initialize ScopedFakeFullKeyboardAccess twice\n";
  g_instance = this;
  ui::SetFullKeyboardAccessGetterForTesting(&FakeIsFullKeyboardAccessEnabled);
}

ScopedFakeFullKeyboardAccess::~ScopedFakeFullKeyboardAccess() {
  CHECK_EQ(g_instance, this, base::NotFatalUntil::M163);
  ui::SetFullKeyboardAccessGetterForTesting(nullptr);
  g_instance = nullptr;
}

// static
ScopedFakeFullKeyboardAccess* ScopedFakeFullKeyboardAccess::GetInstance() {
  return g_instance;
}

}  // namespace ui::test
