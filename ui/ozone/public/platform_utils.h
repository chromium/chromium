// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_UTILS_H_
#define UI_OZONE_PUBLIC_PLATFORM_UTILS_H_

#include <cstdint>

#include "base/component_export.h"

namespace gfx {
class ImageSkia;
}

namespace ui {

// Platform-specific general util functions that didn't find their way to any
// other existing utilities, but they are required to be accessed outside
// Ozone.
class COMPONENT_EXPORT(OZONE_BASE) PlatformUtils {
 public:
  virtual ~PlatformUtils() = default;

  // Returns an icon for a native window referred by |target_window_id|. Can be
  // any window on screen.
  virtual gfx::ImageSkia GetNativeWindowIcon(intptr_t target_window_id) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_PLATFORM_UTILS_H_
