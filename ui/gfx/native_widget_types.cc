// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/native_widget_types.h"

// TODO(https://crbug.com/1443009): ui::PlatformEvent has its own version of
// this function. When unifying, remove one of these copies.

namespace gfx {

GFX_EXPORT bool IsNativeEventValid(const NativeEvent& event) {
#if BUILDFLAG(IS_APPLE)
  return event.IsValid();
#else
  return !!event;
#endif
}

}  // namespace gfx
