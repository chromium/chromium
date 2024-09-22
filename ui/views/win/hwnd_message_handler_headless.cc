// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/hwnd_message_handler_headless.h"

#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/win/hwnd_message_handler_delegate.h"
#include "ui/views/win/hwnd_util.h"

namespace views {
namespace {

// Default headless window size used when no window size was provided in
// creation params.
constexpr gfx::Rect kDefaultHeadlessWindowSize(800, 600);

// In headless mode there is no screen size that would define maximized window
// dimensions. So just double the current window size assuming the user will
// expect it to increase.
constexpr int kZoomedWindowSizeScaleFactor = 2;

// In headless mode where we have to manually scale window bounds because we
// cannot rely on the platform window size since it gets clamped to the monitor
// work area.
gfx::Rect ScaleWindowBoundsMaybe(HWND hwnd, const gfx::Rect& bounds) {
  const float scale = display::win::ScreenWin::GetScaleFactorForHWND(hwnd);
  if (scale > 1.0) {
    gfx::RectF scaled_bounds(bounds);
    scaled_bounds.Scale(scale);
    return gfx::ToEnclosingRect(scaled_bounds);
  }

  return bounds;
}

gfx::Rect GetZoomedWindowBounds(const gfx::Rect& bounds) {
  gfx::Rect zoomed_bounds = bounds;
  zoomed_bounds.set_width(bounds.width() * kZoomedWindowSizeScaleFactor);
  zoomed_bounds.set_height(bounds.height() * kZoomedWindowSizeScaleFactor);

  return zoomed_bounds;
}

}  // namespace

HWNDMessageHandlerHeadless::HWNDMessageHandlerHeadless(
    HWNDMessageHandlerDelegate* delegate,
    const std::string& debugging_id)
    : HWNDMessageHandler(delegate, debugging_id) {}

HWNDMessageHandlerHeadless::~HWNDMessageHandlerHeadless() = default;

void HWNDMessageHandlerHeadless::Init(HWND parent, const gfx::Rect& bounds) {
  TRACE_EVENT0("views", "HWNDMessageHandlerHeadless::Init");
  GetMonitorAndRects(bounds.ToRECT(), &last_monitor_, &last_monitor_rect_,
                     &last_work_area_);

  initial_bounds_valid_ = !bounds.IsEmpty();

  WindowImpl::Init(parent, bounds);

  // In headless mode remember the expected window bounds possibly adjusted
  // according to the scale factor.
  if (initial_bounds_valid_) {
    SetHeadlessWindowBounds(bounds);
  } else {
    // If initial window bounds were not provided, use the newly created
    // platform window size or fall back to the default headless window size
    // as the last resort.
    RECT window_rect;
    if (::GetWindowRect(hwnd(), &window_rect)) {
      SetHeadlessWindowBounds(gfx::Rect(window_rect));
    } else {
      // Even if the window rectangle cannot be retrieved, there is still a
      // chance that ScreenWin::GetScaleFactorForHWND() will be able to figure
      // out the scale factor.
      SetHeadlessWindowBounds(
          ScaleWindowBoundsMaybe(hwnd(), kDefaultHeadlessWindowSize));
    }
  }

  InitExtras();
}

gfx::Rect HWNDMessageHandlerHeadless::GetWindowBoundsInScreen() const {
  // Return the headless window rectangle set in Init() and updated in
  // SetBounds() and SetSize().
  return bounds_;
}

gfx::Rect HWNDMessageHandlerHeadless::GetClientAreaBoundsInScreen() const {
  gfx::Insets client_insets;
  if (!GetClientAreaInsets(&client_insets, last_monitor_)) {
    // If client area insets were not provided, calculate headless client
    // rectangle using the difference between platform window and client
    // rectangles.
    RECT window_rect;
    if (!::GetWindowRect(hwnd(), &window_rect)) {
      return gfx::Rect();
    }

    RECT client_rect;
    if (!::GetClientRect(hwnd(), &client_rect)) {
      return gfx::Rect(window_rect);
    }

    client_insets.set_left(client_rect.left - window_rect.left);
    client_insets.set_right(window_rect.right - client_rect.right);
    client_insets.set_top(client_rect.top - window_rect.top);
    client_insets.set_bottom(window_rect.bottom - client_rect.bottom);
  }

  gfx::Rect bounds = bounds_;
  bounds.Inset(client_insets);
  if (bounds.IsEmpty()) {
    return bounds_;
  }

  return bounds;
}

gfx::Rect HWNDMessageHandlerHeadless::GetRestoredBounds() const {
  return restored_bounds_.value_or(bounds_);
}

void HWNDMessageHandlerHeadless::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) const {
  if (bounds) {
    if (window_state_ == WindowState::kNormal) {
      *bounds = bounds_;
    } else {
      *bounds = restored_bounds_.value_or(bounds_);
    }
  }

  if (show_state) {
    switch (window_state_) {
      case WindowState::kNormal:
        *show_state = ui::mojom::WindowShowState::kNormal;
        break;
      case WindowState::kMinimized:
        *show_state = ui::mojom::WindowShowState::kMinimized;
        break;
      case WindowState::kMaximized:
        *show_state = ui::mojom::WindowShowState::kMaximized;
        break;
      case WindowState::kFullscreen:
        *show_state = ui::mojom::WindowShowState::kFullscreen;
        break;
    }
  }
}

void HWNDMessageHandlerHeadless::SetSize(const gfx::Size& size) {
  // Update the headless window size and pretend the platform window size has
  // updated.
  bool size_changed = bounds_.size() != size;
  gfx::Rect bounds = bounds_;
  bounds.set_size(size);
  SetHeadlessWindowBounds(bounds);
  if (size_changed) {
    delegate_->HandleClientSizeChanged(GetClientAreaBounds().size());
  }
}

void HWNDMessageHandlerHeadless::CenterWindow(const gfx::Size& size) {
  HWND parent = ::GetParent(hwnd());
  if (!::IsWindow(parent)) {
    parent = ::GetWindow(hwnd(), GW_OWNER);
  }

  gfx::Rect center_bounds = views::GetHeadlessWindowBounds(parent);
  if (center_bounds.IsEmpty()) {
    // No parent or no parent rect. Since in headless mode there is no monitor
    // to center the window over, just assume the window size and leave.
    SetSize(size);
    return;
  }

  // This mimics the code in gfx::CenterAndSizeWindow() which we cannot use
  // in headless mode because it operates on HWNDs.
  int x = center_bounds.x();
  if (center_bounds.width() > size.width()) {
    x += (center_bounds.width() - size.width()) / 2;
  }

  int y = center_bounds.y();
  if (center_bounds.height() > size.height()) {
    y += (center_bounds.height() - size.height()) / 2;
  }

  gfx::Rect window_bounds(gfx::Point(x, y), size);
  SetBoundsInternal(window_bounds, /*force_size_changed=*/false);
}

void HWNDMessageHandlerHeadless::SetRegion(HRGN region) {}

void HWNDMessageHandlerHeadless::StackAbove(HWND other_hwnd) {}

void HWNDMessageHandlerHeadless::StackAtTop() {}

void HWNDMessageHandlerHeadless::Show(ui::mojom::WindowShowState show_state,
                                      const gfx::Rect& pixel_restore_bounds) {
  TRACE_EVENT0("views", "HWNDMessageHandlerHeadless::Show");

  bool activate = true;

  switch (show_state) {
    case ui::mojom::WindowShowState::kMinimized:
      Minimize();
      activate = false;
      break;
    case ui::mojom::WindowShowState::kMaximized:
      if (window_state_ != WindowState::kMaximized) {
        if (!pixel_restore_bounds.IsEmpty()) {
          bounds_ = pixel_restore_bounds;
        }
        Maximize();
      }
      break;
    case ui::mojom::WindowShowState::kFullscreen:
      SetFullscreen(true, display::kInvalidDisplayId);
      break;
    case ui::mojom::WindowShowState::kInactive:
      activate = false;
      break;
    default:
      break;
  }

  // In headless mode the platform window is always hidden, so instead of
  // showing it just maintain a local flag to track the expected headless
  // window visibility state and explicitly activate window just like
  // platform window manager would do.
  if (!is_visible_) {
    is_visible_ = true;
    delegate_->HandleVisibilityChanged(/*visible=*/true);
  }

  if (activate) {
    Activate();
  }
}

void HWNDMessageHandlerHeadless::Hide() {
  // In headless mode the platform window is always hidden, so instead of
  // hiding it just maintain a local flag to track the expected headless
  // window visibility state.
  if (is_visible_) {
    is_visible_ = false;
    delegate_->HandleVisibilityChanged(/*visible=*/false);
  }
}

void HWNDMessageHandlerHeadless::Maximize() {
  if (window_state_ == WindowState::kMaximized) {
    return;
  }

  restored_bounds_ = bounds_;
  window_state_ = WindowState::kMaximized;

  gfx::Rect bounds = GetZoomedWindowBounds(bounds_);
  SetBoundsInternal(bounds, /*force_size_changed=*/false);
  delegate_->HandleCommand(static_cast<int>(SC_MAXIMIZE));
}

void HWNDMessageHandlerHeadless::Minimize() {
  if (window_state_ == WindowState::kMinimized) {
    return;
  }

  window_state_ = WindowState::kMinimized;

  // Windows automatiaclly deactivates minimized windows, so we need to
  // replicate this behavior to prevent focus not being restored, see
  // https://crbug.com/358998544.
  was_active_before_minimize_ = is_active_;
  if (is_active_) {
    Deactivate();
  }

  delegate_->HandleWindowMinimizedOrRestored(/*restored=*/false);
  delegate_->HandleCommand(static_cast<int>(SC_MINIMIZE));
  delegate_->HandleNativeBlur(nullptr);
}

void HWNDMessageHandlerHeadless::Restore() {
  if (window_state_ == WindowState::kNormal) {
    return;
  }

  auto prev_state = window_state_;
  window_state_ = WindowState::kNormal;

  RestoreBounds();

  if (prev_state == WindowState::kMinimized) {
    delegate_->HandleWindowMinimizedOrRestored(/*restored=*/true);
    if (was_active_before_minimize_) {
      Activate();
    }
  }

  delegate_->HandleCommand(static_cast<int>(SC_RESTORE));
}

void HWNDMessageHandlerHeadless::Activate() {
  if (!is_active_ && delegate_->CanActivate() && IsTopLevelWindow(hwnd())) {
    is_active_ = true;
    delegate_->HandleActivationChanged(/*active=*/true);
  }
}

void HWNDMessageHandlerHeadless::Deactivate() {
  if (is_active_) {
    is_active_ = false;
    delegate_->HandleActivationChanged(/*active=*/false);
  }
}

void HWNDMessageHandlerHeadless::SetAlwaysOnTop(bool on_top) {
  is_always_on_top_ = on_top;
}

bool HWNDMessageHandlerHeadless::IsVisible() const {
  return is_visible_;
}

bool HWNDMessageHandlerHeadless::IsActive() const {
  return is_active_;
}

bool HWNDMessageHandlerHeadless::IsMinimized() const {
  return window_state_ == WindowState::kMinimized;
}

bool HWNDMessageHandlerHeadless::IsMaximized() const {
  return window_state_ == WindowState::kMaximized;
}

bool HWNDMessageHandlerHeadless::IsFullscreen() const {
  return window_state_ == WindowState::kFullscreen;
}

bool HWNDMessageHandlerHeadless::IsAlwaysOnTop() const {
  return is_always_on_top_;
}

bool HWNDMessageHandlerHeadless::IsHeadless() const {
  return true;
}

void HWNDMessageHandlerHeadless::FlashFrame(bool flash) {}

void HWNDMessageHandlerHeadless::ClearNativeFocus() {
  // Headless windows don't get native focus, so just pretend it got one.
  delegate_->HandleNativeFocus(/*last_focused_window=*/0);
}

// Headless window don't capture mouse.
void HWNDMessageHandlerHeadless::SetCapture() {}
void HWNDMessageHandlerHeadless::ReleaseCapture() {}
bool HWNDMessageHandlerHeadless::HasCapture() const {
  return false;
}

FullscreenHandler* HWNDMessageHandlerHeadless::fullscreen_handler() {
  // Headless windows don't use the fullscreen handler.
  return nullptr;
}

void HWNDMessageHandlerHeadless::SetFullscreen(bool fullscreen,
                                               int64_t target_display_id) {
  if (fullscreen) {
    if (window_state_ == WindowState::kFullscreen) {
      return;
    }

    if (window_state_ != WindowState::kMaximized) {
      restored_bounds_ = bounds_;
    }

    window_state_ = WindowState::kFullscreen;

    gfx::Rect bounds = GetZoomedWindowBounds(bounds_);
    SetBoundsInternal(bounds, /*force_size_changed=*/false);

  } else {
    if (window_state_ != WindowState::kFullscreen) {
      return;
    }

    window_state_ = WindowState::kNormal;

    RestoreBounds();
  }
}

void HWNDMessageHandlerHeadless::SizeConstraintsChanged() {
  // Base class method updates platform window style bits WS_THICKFRAME and
  // WS_MIN/MAXIMIZEBOX according to the delegate's window sizing expectations.
  // Ignored in headless mode since we don't touch underlying platform window.
}

void HWNDMessageHandlerHeadless::SetHeadlessWindowBounds(
    const gfx::Rect& bounds) {
  if (bounds_ != bounds) {
    bounds_ = bounds;
    delegate_->HandleHeadlessWindowBoundsChanged(bounds);
  }
}

void HWNDMessageHandlerHeadless::SetBoundsInternal(
    const gfx::Rect& bounds_in_pixels,
    bool force_size_changed) {
  gfx::Size old_size = GetClientAreaBounds().size();

  // Update the headless window bounds and notify the delegate pretending the
  // platform window size has been changed.
  SetHeadlessWindowBounds(bounds_in_pixels);
  if (old_size != bounds_in_pixels.size() || force_size_changed) {
    delegate_->HandleClientSizeChanged(GetClientAreaBounds().size());
  }
}

void HWNDMessageHandlerHeadless::RestoreBounds() {
  if (restored_bounds_) {
    gfx::Rect bounds = restored_bounds_.value();
    restored_bounds_.reset();
    SetBoundsInternal(bounds, /*force_size_changed=*/false);
  }
}

}  // namespace views
