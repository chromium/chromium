// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_SCREEN_H_
#define UI_AURA_TEST_TEST_SCREEN_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display.h"
#include "ui/display/screen_base.h"

namespace gfx {
class ColorSpace;
class Insets;
class Rect;
class Transform;
}

namespace aura {
class Window;
class WindowTreeHost;

// A minimal, testing Aura implementation of display::Screen.
// TODO(bruthig): Consider extending gfx::test::TestScreen.
class TestScreen : public display::ScreenBase, public WindowObserver {
 public:
  // Creates a display::Screen of the specified size. If no size is specified,
  // then creates a 800x600 screen. |size| is in physical pixels.
  static TestScreen* Create(const gfx::Size& size);

  TestScreen(const TestScreen&) = delete;
  TestScreen& operator=(const TestScreen&) = delete;

  ~TestScreen() override;

  WindowTreeHost* CreateHostForPrimaryDisplay();

  void SetDeviceScaleFactor(float device_scale_factor, bool resize_host = true);
  void SetColorSpace(
      const gfx::ColorSpace& color_space,
      float sdr_white_level = gfx::ColorSpace::kDefaultSDRWhiteLevel);
  void SetDisplayRotation(display::Display::Rotation rotation);
  void SetUIScale(float ui_scale);
  void SetWorkAreaInsets(const gfx::Insets& insets);
  void SetPreferredScaleFactorForWindow(gfx::NativeWindow window,
                                        float scale_factor);

 protected:
  static gfx::NativeWindow GetWindowForPoint(Window* window,
                                             const gfx::Point& local_point);
  gfx::Transform GetRotationTransform() const;
  gfx::Transform GetUIScaleTransform() const;

  // WindowObserver overrides:
  void OnWindowBoundsChanged(Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(Window* window) override;

  // display::Screen overrides:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) override;
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override;
  std::string GetCurrentWorkspace() override;
  std::optional<float> GetPreferredScaleFactorForWindow(
      gfx::NativeWindow window) const override;

 private:
  explicit TestScreen(const gfx::Rect& screen_bounds);

  raw_ptr<aura::WindowTreeHost> host_ = nullptr;
  std::unordered_map<gfx::NativeWindow, float> preferred_scale_factors_;

  float ui_scale_ = 1.0f;
};

}  // namespace aura

#endif  // UI_AURA_TEST_TEST_SCREEN_H_
