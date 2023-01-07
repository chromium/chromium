// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_LINUX_UI_DELEGATE_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_LINUX_UI_DELEGATE_WAYLAND_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/linux/linux_ui_delegate.h"

namespace ui {

class WaylandConnection;

class LinuxUiDelegateWayland : public LinuxUiDelegate {
 public:
  explicit LinuxUiDelegateWayland(WaylandConnection* connection);
  ~LinuxUiDelegateWayland() override;

  // LinuxUiDelegate:
  LinuxUiBackend GetBackend() const override;
  bool ExportWindowHandle(
      gfx::AcceleratedWidget parent,
      base::OnceCallback<void(const std::string&)> callback) override;
  bool ExportWindowHandle(
      gfx::AcceleratedWidget window_id,
      base::OnceCallback<void(std::string)> callback) override;

 private:
  void OnHandleForward(base::OnceCallback<void(std::string)> callback,
                       const std::string& handle);

  const raw_ptr<WaylandConnection> connection_;
  base::WeakPtrFactory<LinuxUiDelegateWayland> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_LINUX_UI_DELEGATE_WAYLAND_H_
