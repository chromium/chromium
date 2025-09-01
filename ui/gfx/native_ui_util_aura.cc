// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ui/gfx/native_ui_util.h"

#if !defined(USE_AURA)
#error "This file can only build on platforms that use Aura."
#endif

namespace gfx {

gfx::NativeView GetViewForWindow(gfx::NativeWindow window) {
  return window;
}

}  // namespace gfx
