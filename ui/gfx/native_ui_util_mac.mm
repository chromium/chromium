// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/native_ui_util.h"

#import <Cocoa/Cocoa.h>

#include "base/check.h"
#include "build/build_config.h"

namespace gfx {

gfx::NativeView GetViewForWindow(gfx::NativeWindow native_window) {
  NSWindow* window = native_window.GetNativeNSWindow();
  DCHECK(window);
  DCHECK([window contentView]);
  return gfx::NativeView([window contentView]);
}

}  // namespace gfx
