// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_MENU_UTILS_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_MENU_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "ui/ozone/public/platform_menu_utils.h"

namespace ui {

class WaylandConnection;

class WaylandMenuUtils : public PlatformMenuUtils {
 public:
  explicit WaylandMenuUtils(WaylandConnection* connection);
  WaylandMenuUtils(const WaylandMenuUtils&) = delete;
  WaylandMenuUtils& operator=(const WaylandMenuUtils&) = delete;
  ~WaylandMenuUtils() override;

  int GetCurrentKeyModifiers() const override;
  std::string ToDBusKeySym(KeyboardCode code) const override;

 private:
  const raw_ptr<WaylandConnection, LeakedDanglingUntriaged> connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_MENU_UTILS_H_
