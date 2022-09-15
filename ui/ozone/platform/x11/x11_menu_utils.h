// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_MENU_UTILS_H_
#define UI_OZONE_PLATFORM_X11_X11_MENU_UTILS_H_

#include "ui/ozone/public/platform_menu_utils.h"

namespace ui {

class X11MenuUtils : public PlatformMenuUtils {
 public:
  X11MenuUtils();
  X11MenuUtils(const X11MenuUtils&) = delete;
  X11MenuUtils& operator=(const X11MenuUtils&) = delete;
  ~X11MenuUtils() override;

  int GetCurrentKeyModifiers() const override;

  std::string ToDBusKeySym(KeyboardCode code) const override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_MENU_UTILS_H_
