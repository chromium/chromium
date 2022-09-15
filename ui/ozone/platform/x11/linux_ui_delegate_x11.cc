// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/linux_ui_delegate_x11.h"

#include "ui/gfx/x/xproto.h"
#include "ui/ozone/platform/x11/x11_window.h"
#include "ui/ozone/platform/x11/x11_window_manager.h"

namespace ui {

LinuxUiDelegateX11::LinuxUiDelegateX11() = default;

LinuxUiDelegateX11::~LinuxUiDelegateX11() = default;

LinuxUiBackend LinuxUiDelegateX11::GetBackend() const {
  return LinuxUiBackend::kX11;
}

void LinuxUiDelegateX11::SetTransientWindowForParent(
    gfx::AcceleratedWidget parent,
    gfx::AcceleratedWidget transient) {
  X11Window* parent_window = X11WindowManager::GetInstance()->GetWindow(parent);
  // parent_window might be dead if there was a top-down window close
  if (parent_window)
    parent_window->SetTransientWindow(static_cast<x11::Window>(transient));
}

bool LinuxUiDelegateX11::ExportWindowHandle(
    gfx::AcceleratedWidget window_id,
    base::OnceCallback<void(std::string)> callback) {
  std::move(callback).Run(base::StringPrintf("x11:%#x", window_id));
  return true;
}

}  // namespace ui
