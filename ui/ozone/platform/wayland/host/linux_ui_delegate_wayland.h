// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_LINUX_UI_DELEGATE_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_LINUX_UI_DELEGATE_WAYLAND_H_

#include "ui/base/linux/linux_ui_delegate.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class WaylandConnection;

class LinuxUiDelegateWayland : public LinuxUiDelegate {
 public:
  explicit LinuxUiDelegateWayland(WaylandConnection* connection);
  ~LinuxUiDelegateWayland() override;

  // LinuxUiDelegate:
  LinuxUiBackend GetBackend() const override;
  bool SetWidgetTransientFor(
      gfx::AcceleratedWidget parent,
      base::OnceCallback<void(const std::string&)> callback) override;
  int GetKeyState() override;

 private:
  WaylandConnection* const connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_LINUX_UI_DELEGATE_WAYLAND_H_
