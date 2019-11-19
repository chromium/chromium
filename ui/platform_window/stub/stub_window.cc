// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/stub/stub_window.h"

#include "base/logging.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

StubWindow::StubWindow(PlatformWindowDelegate* delegate,
                       bool use_default_accelerated_widget,
                       const gfx::Rect& bounds)
    : delegate_(delegate), bounds_(bounds) {
  DCHECK(delegate);
  if (use_default_accelerated_widget)
    delegate_->OnAcceleratedWidgetAvailable(gfx::kNullAcceleratedWidget);
}

StubWindow::~StubWindow() {}

void StubWindow::Show(bool inactive) {}

void StubWindow::Hide() {}

void StubWindow::Close() {
  delegate_->OnClosed();
}

bool StubWindow::IsVisible() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return true;
}

void StubWindow::PrepareForShutdown() {}

void StubWindow::SetBounds(const gfx::Rect& bounds) {
  // Even if the pixel bounds didn't change this call to the delegate should
  // still happen. The device scale factor may have changed which effectively
  // changes the bounds.
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
}

gfx::Rect StubWindow::GetBounds() {
  return bounds_;
}

void StubWindow::SetTitle(const base::string16& title) {}

void StubWindow::SetCapture() {}

void StubWindow::ReleaseCapture() {}

bool StubWindow::HasCapture() const {
  return false;
}

void StubWindow::ToggleFullscreen() {}

void StubWindow::Maximize() {}

void StubWindow::Minimize() {}

void StubWindow::Restore() {}

PlatformWindowState StubWindow::GetPlatformWindowState() const {
  return PlatformWindowState::kUnknown;
}

void StubWindow::Activate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void StubWindow::Deactivate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void StubWindow::SetUseNativeFrame(bool use_native_frame) {}

bool StubWindow::ShouldUseNativeFrame() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void StubWindow::SetCursor(PlatformCursor cursor) {}

void StubWindow::MoveCursorTo(const gfx::Point& location) {}

void StubWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {}

void StubWindow::SetRestoredBoundsInPixels(const gfx::Rect& bounds) {}

gfx::Rect StubWindow::GetRestoredBoundsInPixels() const {
  return gfx::Rect();
}

void StubWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                const gfx::ImageSkia& app_icon) {}

void StubWindow::SizeConstraintsChanged() {}

}  // namespace ui
