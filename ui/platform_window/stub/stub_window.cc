// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/stub/stub_window.h"

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/display/types/display_constants.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

StubWindow::StubWindow(PlatformWindowDelegate* delegate,
                       bool use_default_accelerated_widget,
                       const gfx::Rect& bounds)
    : bounds_(bounds) {
  DCHECK(delegate);
  InitDelegate(delegate, use_default_accelerated_widget);
}

StubWindow::StubWindow(const gfx::Rect& bounds) : bounds_(bounds) {}

StubWindow::~StubWindow() = default;

void StubWindow::InitDelegate(PlatformWindowDelegate* delegate,
                              bool use_default_accelerated_widget) {
  DCHECK(delegate);
  delegate_ = delegate;
  if (use_default_accelerated_widget)
    delegate_->OnAcceleratedWidgetAvailable(gfx::kNullAcceleratedWidget);
}

void StubWindow::InitDelegateWithWidget(PlatformWindowDelegate* delegate,
                                        gfx::AcceleratedWidget widget) {
  DCHECK(delegate);
  delegate_ = delegate;
  delegate_->OnAcceleratedWidgetAvailable(widget);
}

void StubWindow::Show(bool inactive) {}

void StubWindow::Hide() {}

void StubWindow::Close() {
  delegate_->OnClosed();
}

bool StubWindow::IsVisible() const {
  return true;
}

void StubWindow::PrepareForShutdown() {}

void StubWindow::SetBoundsInPixels(const gfx::Rect& bounds) {
  // Even if the pixel bounds didn't change this call to the delegate should
  // still happen. The device scale factor may have changed which effectively
  // changes the bounds.
  bool origin_changed = bounds_.origin() != bounds.origin();
  bounds_ = bounds;
  delegate_->OnBoundsChanged({origin_changed});
}

gfx::Rect StubWindow::GetBoundsInPixels() const {
  return bounds_;
}

void StubWindow::SetBoundsInDIP(const gfx::Rect& bounds) {
  SetBoundsInPixels(delegate_->ConvertRectToPixels(bounds));
}

gfx::Rect StubWindow::GetBoundsInDIP() const {
  return delegate_->ConvertRectToDIP(bounds_);
}

void StubWindow::SetTitle(const std::u16string& title) {}

void StubWindow::SetCapture() {}

void StubWindow::ReleaseCapture() {}

bool StubWindow::HasCapture() const {
  return false;
}

void StubWindow::SetFullscreen(bool fullscreen, int64_t target_display_id) {
  DCHECK_EQ(target_display_id, display::kInvalidDisplayId);
  window_state_ = fullscreen ? ui::PlatformWindowState::kFullScreen
                             : ui::PlatformWindowState::kUnknown;
}

void StubWindow::Maximize() {}

void StubWindow::Minimize() {}

void StubWindow::Restore() {}

PlatformWindowState StubWindow::GetPlatformWindowState() const {
  return window_state_;
}

void StubWindow::Activate() {
  if (activation_state_ != ActivationState::kActive) {
    activation_state_ = ActivationState::kActive;
    delegate_->OnActivationChanged(/*active=*/true);
  }
}

void StubWindow::Deactivate() {
  if (activation_state_ != ActivationState::kInactive) {
    activation_state_ = ActivationState::kInactive;
    delegate_->OnActivationChanged(/*active=*/false);
  }
}

void StubWindow::SetUseNativeFrame(bool use_native_frame) {}

bool StubWindow::ShouldUseNativeFrame() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void StubWindow::SetCursor(scoped_refptr<PlatformCursor> cursor) {}

void StubWindow::MoveCursorTo(const gfx::Point& location) {}

void StubWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {}

void StubWindow::SetRestoredBoundsInDIP(const gfx::Rect& bounds) {}

gfx::Rect StubWindow::GetRestoredBoundsInDIP() const {
  return gfx::Rect();
}

void StubWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                const gfx::ImageSkia& app_icon) {}

void StubWindow::SizeConstraintsChanged() {}

}  // namespace ui
