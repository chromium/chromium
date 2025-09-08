// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_UI_UTIL_H_
#define UI_GFX_NATIVE_UI_UTIL_H_

#include "base/component_export.h"
#include "ui/gfx/native_ui_types.h"

namespace gfx {

// TODO(kerenzhu): NativeWindow and NativeView utility functions
// should be moved from chrome/browser/platform_util.h to here.

// Returns a NativeView handle for parenting dialogs off `window`. This can be
// used to position a dialog using a NativeWindow, when a NativeView (e.g.
// browser tab) isn't available.
COMPONENT_EXPORT(GFX)
gfx::NativeView GetViewForWindow(gfx::NativeWindow window);

}  // namespace gfx

#endif  // UI_GFX_NATIVE_UI_UTIL_H_
