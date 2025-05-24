// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_MENU_UTILS_H_
#define UI_OZONE_PUBLIC_PLATFORM_MENU_UTILS_H_

#include <stdint.h>

#include "base/component_export.h"

namespace ui {

enum class DomCode : uint32_t;

// Platform-specific functions related to menus.
class COMPONENT_EXPORT(OZONE_BASE) PlatformMenuUtils {
 public:
  PlatformMenuUtils();
  PlatformMenuUtils(const PlatformMenuUtils&) = delete;
  PlatformMenuUtils& operator=(const PlatformMenuUtils&) = delete;
  virtual ~PlatformMenuUtils();

  // Returns a bitmask of EventFlags showing the state of Alt, Shift and Ctrl
  // keys that came with the most recent UI event.
  virtual int GetCurrentKeyModifiers() const;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_PLATFORM_MENU_UTILS_H_
