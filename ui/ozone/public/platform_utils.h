// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_UTILS_H_
#define UI_OZONE_PUBLIC_PLATFORM_UTILS_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/component_export.h"

namespace gfx {
class ImageSkia;
}

namespace ui {

class KeyEvent;

// Platform-specific general util functions that didn't find their way to any
// other existing utilities, but they are required to be accessed outside
// Ozone.
class COMPONENT_EXPORT(OZONE_BASE) PlatformUtils {
 public:
  virtual ~PlatformUtils() = default;

  // Returns an icon for a native window referred by |target_window_id|. Can be
  // any window on screen.
  virtual gfx::ImageSkia GetNativeWindowIcon(intptr_t target_window_id) = 0;

  // Returns a string that labels Chromium's windows for the window manager.
  // By default, the class name is based on the so called desktop base name (see
  // GetDesktopBaseName() in chrome/browser/shell_integration_linux.cc), which,
  // in its turn, depends on the channel.
  virtual std::string GetWmWindowClass(
      const std::string& desktop_base_name) = 0;

  // Called when it is found that a KeyEvent is not consumed.
  virtual void OnUnhandledKeyEvent(const KeyEvent& key_event) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_PLATFORM_UTILS_H_
