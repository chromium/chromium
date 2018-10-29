// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_SCREEN_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_SCREEN_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_screen.h"

namespace ui {

// A PlatformScreen implementation for Scenic.
class ScenicScreen : public PlatformScreen {
 public:
  ScenicScreen();
  ~ScenicScreen() override;

  // Processes window state change events for the ScenicWindow |window_id_|.
  void OnWindowAdded(int32_t window_id);
  void OnWindowRemoved(int32_t window_id);
  void OnWindowMetrics(int32_t window_id, float device_pixel_ratio);
  void OnWindowBoundsChanged(int32_t window_id, gfx::Rect bounds);

  base::WeakPtr<ScenicScreen> GetWeakPtr();

  // display::Screen implementation.
  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetPrimaryDisplay() const override;
  display::Display GetDisplayForAcceleratedWidget(
      gfx::AcceleratedWidget widget) const override;
  gfx::Point GetCursorScreenPoint() const override;
  gfx::AcceleratedWidget GetAcceleratedWidgetAtScreenPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

 private:
  using DisplayVector = std::vector<display::Display>;

  DisplayVector displays_;

  base::ObserverList<display::DisplayObserver> observers_;

  base::WeakPtrFactory<ScenicScreen> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScenicScreen);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_SCREEN_H_
