// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_UTILS_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_UTILS_H_

#include "ui/ozone/public/platform_utils.h"

namespace ui {

class WaylandConnection;

class WaylandUtils : public PlatformUtils {
 public:
  explicit WaylandUtils(WaylandConnection* connection);
  WaylandUtils(const WaylandUtils&) = delete;
  WaylandUtils& operator=(const WaylandUtils&) = delete;
  ~WaylandUtils() override;

  gfx::ImageSkia GetNativeWindowIcon(intptr_t target_window_id) override;
  std::string GetWmWindowClass(const std::string& desktop_base_name) override;
  std::unique_ptr<PlatformUtils::ScopedDisableClientSideDecorationsForTest>
  DisableClientSideDecorationsForTest() override;
  void OnUnhandledKeyEvent(const KeyEvent& key_event) override;

 private:
  WaylandConnection* const connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_UTILS_H_
