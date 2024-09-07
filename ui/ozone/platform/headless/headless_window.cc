// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/headless_window.h"

#include <string>

#include "base/notreached.h"
#include "build/build_config.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/headless/headless_window_manager.h"

namespace ui {

HeadlessWindow::HeadlessWindow(PlatformWindowDelegate* delegate,
                               HeadlessWindowManager* manager,
                               const gfx::Rect& bounds)
    : delegate_(delegate), manager_(manager), bounds_(bounds) {
  widget_ = manager_->AddWindow(this);
  delegate->OnAcceleratedWidgetAvailable(widget_);
}

HeadlessWindow::~HeadlessWindow() {
  manager_->RemoveWindow(widget_, this);
}

void HeadlessWindow::Show(bool inactive) {
  visible_ = true;
}

void HeadlessWindow::Hide() {
  visible_ = false;
}

void HeadlessWindow::Close() {
  delegate_->OnClosed();
}

bool HeadlessWindow::IsVisible() const {
  return visible_;
}

void HeadlessWindow::PrepareForShutdown() {}

void HeadlessWindow::SetBoundsInPixels(const gfx::Rect& bounds) {
  UpdateBounds(bounds);
}

gfx::Rect HeadlessWindow::GetBoundsInPixels() const {
  return bounds_;
}

void HeadlessWindow::SetBoundsInDIP(const gfx::Rect& bounds) {
  SetBoundsInPixels(delegate_->ConvertRectToPixels(bounds));
}

gfx::Rect HeadlessWindow::GetBoundsInDIP() const {
  return delegate_->ConvertRectToDIP(bounds_);
}

void HeadlessWindow::SetTitle(const std::u16string& title) {}

void HeadlessWindow::SetCapture() {}

void HeadlessWindow::ReleaseCapture() {}

bool HeadlessWindow::HasCapture() const {
  return false;
}

void HeadlessWindow::SetFullscreen(bool fullscreen, int64_t target_display_id) {
  DCHECK_EQ(target_display_id, display::kInvalidDisplayId);
  if (!delegate_->CanFullscreen()) {
    return;
  }

  if (fullscreen) {
    if (window_state_ != PlatformWindowState::kMaximized &&
        window_state_ != PlatformWindowState::kFullScreen) {
      restored_bounds_ = bounds_;
    }
    ZoomWindowBounds();
    UpdateWindowState(PlatformWindowState::kFullScreen);
  } else {
    if (window_state_ != PlatformWindowState::kFullScreen) {
      return;
    }
    RestoreWindowBounds();
    UpdateWindowState(PlatformWindowState::kNormal);
  }
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Setting kImmersive when it's fullscreen on headless window since the
  // immersive fullscreen is default for fullscreen on Lacros.
  delegate_->OnFullscreenTypeChanged(
      window_state_ == PlatformWindowState::kFullScreen
          ? PlatformFullscreenType::kImmersive
          : PlatformFullscreenType::kNone,
      fullscreen ? PlatformFullscreenType::kImmersive
                 : PlatformFullscreenType::kNone);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

void HeadlessWindow::Maximize() {
  if (!delegate_->CanMaximize()) {
    return;
  }

  if (window_state_ != PlatformWindowState::kMaximized &&
      window_state_ != PlatformWindowState::kFullScreen) {
    restored_bounds_ = bounds_;
    ZoomWindowBounds();
    UpdateWindowState(PlatformWindowState::kMaximized);
  }
}

void HeadlessWindow::Minimize() {
  if (window_state_ != PlatformWindowState::kMinimized) {
    // Minimized window retains its size and position, however, it's made
    // hidden by Aura.
    if (window_state_ == PlatformWindowState::kMaximized ||
        window_state_ == PlatformWindowState::kFullScreen) {
      RestoreWindowBounds();
    }
    UpdateWindowState(PlatformWindowState::kMinimized);
    // Minimized windows are inactive. Aura activates minimized windows
    // when restoring. If we don't deactivate the window here, the subsequent
    // activation will be optimized away, causing https://crbug.com/358998544.
    Deactivate();
  }
}

void HeadlessWindow::Restore() {
  if (window_state_ != PlatformWindowState::kNormal) {
    RestoreWindowBounds();
    UpdateWindowState(PlatformWindowState::kNormal);
  }
}

PlatformWindowState HeadlessWindow::GetPlatformWindowState() const {
  return window_state_;
}

void HeadlessWindow::Activate() {
  if (activation_state_ != ActivationState::kActive) {
    activation_state_ = ActivationState::kActive;
    delegate_->OnActivationChanged(/*active=*/true);
  }
}

void HeadlessWindow::Deactivate() {
  if (activation_state_ != ActivationState::kInactive) {
    activation_state_ = ActivationState::kInactive;
    delegate_->OnActivationChanged(/*active=*/false);
  }
}

void HeadlessWindow::SetUseNativeFrame(bool use_native_frame) {}

bool HeadlessWindow::ShouldUseNativeFrame() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void HeadlessWindow::SetCursor(scoped_refptr<PlatformCursor> cursor) {}

void HeadlessWindow::MoveCursorTo(const gfx::Point& location) {}

void HeadlessWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {}

void HeadlessWindow::SetRestoredBoundsInDIP(const gfx::Rect& bounds) {
  restored_bounds_ = delegate_->ConvertRectToPixels(bounds);
}

gfx::Rect HeadlessWindow::GetRestoredBoundsInDIP() const {
  return delegate_->ConvertRectToDIP(restored_bounds_.value_or(bounds_));
}

void HeadlessWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                    const gfx::ImageSkia& app_icon) {}

void HeadlessWindow::SizeConstraintsChanged() {}

void HeadlessWindow::ZoomWindowBounds() {
  gfx::Rect new_bounds = bounds_;
  new_bounds.set_width(bounds_.width() * 2);
  new_bounds.set_height(bounds_.height() * 2);
  UpdateBounds(new_bounds);
}

void HeadlessWindow::RestoreWindowBounds() {
  if (restored_bounds_) {
    gfx::Rect restored_bounds = *restored_bounds_;
    restored_bounds_.reset();
    UpdateBounds(restored_bounds);
  }
}

void HeadlessWindow::UpdateBounds(const gfx::Rect& bounds) {
  // Even if the pixel bounds didn't change this call to the delegate should
  // still happen. The device scale factor may have changed which effectively
  // changes the bounds.
  bool origin_changed = bounds_.origin() != bounds.origin();
  bounds_ = bounds;
  delegate_->OnBoundsChanged({origin_changed});
}

void HeadlessWindow::UpdateWindowState(PlatformWindowState new_window_state) {
  DCHECK_NE(window_state_, new_window_state);

  auto old_window_state = window_state_;
  window_state_ = new_window_state;
  delegate_->OnWindowStateChanged(old_window_state, new_window_state);
}

}  // namespace ui
