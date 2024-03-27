// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_UTILS_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "ui/ozone/public/platform_utils.h"

namespace ui {

#define PARAM_TO_FLOAT(x) (x / 10000.f)
#define FLOAT_TO_PARAM(x) static_cast<uint32_t>(x * 10000)

class WaylandConnection;

class WaylandUtils : public PlatformUtils {
 public:
  explicit WaylandUtils(WaylandConnection* connection);
  WaylandUtils(const WaylandUtils&) = delete;
  WaylandUtils& operator=(const WaylandUtils&) = delete;
  ~WaylandUtils() override;

  gfx::ImageSkia GetNativeWindowIcon(intptr_t target_window_id) override;
  std::string GetWmWindowClass(const std::string& desktop_base_name) override;
  void OnUnhandledKeyEvent(const KeyEvent& key_event) override;

 private:
  const raw_ptr<WaylandConnection, LeakedDanglingUntriaged> connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_UTILS_H_
