// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SCREEN_OZONE_H_
#define UI_AURA_SCREEN_OZONE_H_

#include <memory>

#include "base/macros.h"
#include "ui/aura/aura_export.h"
#include "ui/display/screen.h"

namespace ui {
class PlatformScreen;
}

namespace aura {

// display::Screen implementation on top of ui::PlatformScreen provided by
// Ozone.
class AURA_EXPORT ScreenOzone : public display::Screen {
 public:
  ScreenOzone();
  ~ScreenOzone() override;

  // display::Screen interface.
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  int GetNumDisplays() const override;
  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override;
  display::Display GetDisplayNearestView(gfx::NativeView view) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  display::Display GetPrimaryDisplay() const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

 private:
  gfx::AcceleratedWidget GetAcceleratedWidgetForWindow(
      aura::Window* window) const;

  std::unique_ptr<ui::PlatformScreen> platform_screen_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOzone);
};

}  // namespace aura

#endif  // UI_AURA_SCREEN_OZONE_H_
