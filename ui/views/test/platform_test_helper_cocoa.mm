// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/platform_test_helper.h"

#include <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/macros.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget.h"
#import "ui/views_bridge_mac/bridged_native_widget_impl.h"

namespace views {

void PlatformTestHelper::SimulateNativeDestroy(Widget* widget) {
  // Retain the window while closing it, otherwise the window may lose its
  // last owner before -[NSWindow close] completes (this offends AppKit).
  // Usually this reference will exist on an event delivered to the runloop.
  base::scoped_nsobject<NSWindow> window(
      [widget->GetNativeWindow().GetNativeNSWindow() retain]);
  [window close];
}

}  // namespace views
