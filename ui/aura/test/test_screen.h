// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_SCREEN_H_
#define UI_AURA_TEST_TEST_SCREEN_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
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
  ~TestScreen() override;

  WindowTreeHost* CreateHostForPrimaryDisplay();

  void SetDeviceScaleFactor(float device_scale_fator);
  void SetColorSpace(
      const gfx::ColorSpace& color_space,
      float sdr_white_level = gfx::ColorSpace::kDefaultSDRWhiteLevel);
  void SetDisplayRotation(display::Display::Rotation rotation);
  void SetUIScale(float ui_scale);
  void SetWorkAreaInsets(const gfx::Insets& insets);

 protected:
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
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override;

 private:
  explicit TestScreen(const gfx::Rect& screen_bounds);

  aura::WindowTreeHost* host_ = nullptr;

  float ui_scale_ = 1.0f;

  DISALLOW_COPY_AND_ASSIGN(TestScreen);
};

}  // namespace aura

#endif  // UI_AURA_TEST_TEST_SCREEN_H_
