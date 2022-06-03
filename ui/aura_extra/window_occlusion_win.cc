// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_extra/window_occlusion_win.h"

#include "ui/aura_extra/window_occlusion_impl_win.h"

namespace aura_extra {

namespace {

// Default implementation of WindowBoundsDelegate using GetWindowRect().
class WindowBoundsDelegateImpl : public WindowBoundsDelegate {
 public:
  WindowBoundsDelegateImpl();

  WindowBoundsDelegateImpl(const WindowBoundsDelegateImpl&) = delete;
  WindowBoundsDelegateImpl& operator=(const WindowBoundsDelegateImpl&) = delete;

  ~WindowBoundsDelegateImpl() override {}

  // WindowBoundsDelegate:
  gfx::Rect GetBoundsInPixels(aura::WindowTreeHost* window) override;
};

WindowBoundsDelegateImpl::WindowBoundsDelegateImpl() = default;

gfx::Rect WindowBoundsDelegateImpl::GetBoundsInPixels(
    aura::WindowTreeHost* window) {
  HWND hwnd = window->GetAcceleratedWidget();
  RECT window_rect_in_pixels;

  bool success = GetWindowRect(hwnd, &window_rect_in_pixels);
  DCHECK(success);

  return gfx::Rect(window_rect_in_pixels);
}

}  // namespace

base::flat_map<aura::WindowTreeHost*, aura::Window::OcclusionState>
ComputeNativeWindowOcclusionStatus(
    const std::vector<aura::WindowTreeHost*>& windows) {
  return ComputeNativeWindowOcclusionStatusImpl(
      windows, std::make_unique<WindowsDesktopWindowIterator>(),
      std::make_unique<WindowBoundsDelegateImpl>());
}

}  // namespace aura_extra
