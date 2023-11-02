// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_LINUX_UI_DELEGATE_X11_H_
#define UI_OZONE_PLATFORM_X11_LINUX_UI_DELEGATE_X11_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/linux/linux_ui_delegate.h"

namespace ui {

class LinuxUiDelegateX11 : public LinuxUiDelegate {
 public:
  LinuxUiDelegateX11();
  ~LinuxUiDelegateX11() override;

  // LinuxUiDelegate:
  LinuxUiBackend GetBackend() const override;
  void SetTransientWindowForParent(gfx::AcceleratedWidget parent,
                                   gfx::AcceleratedWidget transient) override;
  bool ExportWindowHandle(
      gfx::AcceleratedWidget window_id,
      base::OnceCallback<void(std::string)> callback) override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_LINUX_UI_DELEGATE_X11_H_
