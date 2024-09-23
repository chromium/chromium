// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_screen.h"

#include <stdint.h>

#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "build/build_config.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display_transform.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace aura {

namespace {

bool IsRotationPortrait(display::Display::Rotation rotation) {
  return rotation == display::Display::ROTATE_90 ||
         rotation == display::Display::ROTATE_270;
}

}  // namespace

// static
TestScreen* TestScreen::Create(const gfx::Size& size) {
  const gfx::Size kDefaultSize(800, 600);
  // Use (0,0) because the desktop aura tests are executed in
  // native environment where the display's origin is (0,0).
  return new TestScreen(gfx::Rect(size.IsEmpty() ? kDefaultSize : size));
}

TestScreen::~TestScreen() {
  delete host_;
}

WindowTreeHost* TestScreen::CreateHostForPrimaryDisplay() {
  DCHECK(!host_);
  ui::PlatformWindowInitProperties properties(
      gfx::Rect(GetPrimaryDisplay().GetSizeInPixel()));
  host_ = WindowTreeHost::Create(std::move(properties)).release();
  // Some tests don't correctly manage window focus/activation states.
  // Makes sure InputMethod is default focused so that IME basics can work.
  host_->GetInputMethod()->OnFocus();
  host_->window()->AddObserver(this);
  // Other test code may have already initialized the compositor.
  if (!host_->compositor()->root_layer())
    host_->InitHost();
  host_->window()->Show();
  return host_;
}

void TestScreen::SetDeviceScaleFactor(float device_scale_factor,
                                      bool resize_host) {
  display::Display display(GetPrimaryDisplay());
  gfx::Rect bounds_in_pixel(display.GetSizeInPixel());
  display.SetScaleAndBounds(device_scale_factor, bounds_in_pixel);
  display_list().UpdateDisplay(display);
  if (resize_host)
    host_->OnHostResizedInPixels(bounds_in_pixel.size());
}

void TestScreen::SetColorSpace(const gfx::ColorSpace& color_space,
                               float sdr_white_level) {
  display::Display display(GetPrimaryDisplay());
  gfx::DisplayColorSpaces display_color_spaces(color_space,
                                               gfx::BufferFormat::RGBA_8888);
  display_color_spaces.SetSDRMaxLuminanceNits(sdr_white_level);
  display.SetColorSpaces(display_color_spaces);
  display_list().UpdateDisplay(display);
}

void TestScreen::SetDisplayRotation(display::Display::Rotation rotation) {
  display::Display display(GetPrimaryDisplay());
  gfx::Rect bounds_in_pixel(display.GetSizeInPixel());
  gfx::Rect new_bounds(bounds_in_pixel);
  if (IsRotationPortrait(rotation) != IsRotationPortrait(display.rotation())) {
    new_bounds.set_width(bounds_in_pixel.height());
    new_bounds.set_height(bounds_in_pixel.width());
  }
  display.set_rotation(rotation);
  display.SetScaleAndBounds(display.device_scale_factor(), new_bounds);
  display_list().UpdateDisplay(display);
  host_->SetRootTransform(GetUIScaleTransform() * GetRotationTransform());
}

void TestScreen::SetUIScale(float ui_scale) {
  ui_scale_ = ui_scale;
  display::Display display(GetPrimaryDisplay());
  gfx::Rect bounds_in_pixel(display.GetSizeInPixel());
  gfx::Rect new_bounds = gfx::ToNearestRect(
      gfx::ScaleRect(gfx::RectF(bounds_in_pixel), 1.0f / ui_scale));
  display.SetScaleAndBounds(display.device_scale_factor(), new_bounds);
  display_list().UpdateDisplay(display);
  host_->SetRootTransform(GetUIScaleTransform() * GetRotationTransform());
}

void TestScreen::SetWorkAreaInsets(const gfx::Insets& insets) {
  display::Display display(GetPrimaryDisplay());
  display.UpdateWorkAreaFromInsets(insets);
  display_list().UpdateDisplay(display);
}

void TestScreen::SetPreferredScaleFactorForWindow(gfx::NativeWindow window,
                                                  float scale_factor) {
  preferred_scale_factors_[window] = scale_factor;
}

gfx::Transform TestScreen::GetRotationTransform() const {
  display::Display display = GetPrimaryDisplay();
  return display::CreateRotationTransform(display.rotation(),
                                          gfx::SizeF(display.size()));
}

gfx::Transform TestScreen::GetUIScaleTransform() const {
  gfx::Transform ui_scale;
  ui_scale.Scale(1.0f / ui_scale_, 1.0f / ui_scale_);
  return ui_scale;
}

void TestScreen::OnWindowBoundsChanged(Window* window,
                                       const gfx::Rect& old_bounds,
                                       const gfx::Rect& new_bounds,
                                       ui::PropertyChangeReason reason) {
  DCHECK_EQ(host_->window(), window);
  display::Display display(GetPrimaryDisplay());
  display.SetSize(gfx::ScaleToFlooredSize(new_bounds.size(),
                                          display.device_scale_factor()));
  display_list().UpdateDisplay(display);
}

void TestScreen::OnWindowDestroying(Window* window) {
  if (host_->window() == window) {
    host_->window()->RemoveObserver(this);
    host_ = nullptr;
  }
}

gfx::Point TestScreen::GetCursorScreenPoint() {
  return Env::GetInstance()->last_mouse_location();
}

bool TestScreen::IsWindowUnderCursor(gfx::NativeWindow window) {
  return GetWindowAtScreenPoint(GetCursorScreenPoint()) == window;
}

gfx::NativeWindow TestScreen::GetWindowForPoint(Window* window,
                                                const gfx::Point& local_point) {
  DCHECK(window);
  if (!window->IsVisible()) {
    return nullptr;
  }

  if (!window->HitTest(local_point)) {
    return nullptr;
  }

  for (Window* child : base::Reversed(window->children())) {
    gfx::Point point_in_child_coords(local_point);
    Window::ConvertPointToTarget(window, child, &point_in_child_coords);
    Window* match = GetWindowForPoint(child, point_in_child_coords);
    if (match) {
      return match;
    }
  }
  return window;
}

gfx::NativeWindow TestScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  if (!host_ || !host_->window())
    return nullptr;

  // GetWindowAtScreenPoint() is designed to return a visible window that
  // contains the given point within its bounds. Using GetEventHandlerForPoint()
  // can lead to null returns for windows that don't have an event handler, such
  // as content_window_ in DesktopNativeWidgetAura.
  return GetWindowForPoint(host_->window(), point);
}

gfx::NativeWindow TestScreen::GetLocalProcessWindowAtPoint(
    const gfx::Point& point,
    const std::set<gfx::NativeWindow>& ignore) {
  return nullptr;
}

display::Display TestScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return GetPrimaryDisplay();
}

std::string TestScreen::GetCurrentWorkspace() {
  return {};
}

std::optional<float> TestScreen::GetPreferredScaleFactorForWindow(
    gfx::NativeWindow window) const {
  if (auto it = preferred_scale_factors_.find(window);
      it != preferred_scale_factors_.end()) {
    return it->second;
  }
  return Screen::GetPreferredScaleFactorForWindow(window);
}

TestScreen::TestScreen(const gfx::Rect& screen_bounds) {
  static int64_t synthesized_display_id = 2000;
  display::Display display(synthesized_display_id++);
  display.SetScaleAndBounds(1.0f, screen_bounds);
  ProcessDisplayChanged(display, true /* is_primary */);
}

}  // namespace aura
