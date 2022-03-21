// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/platform_screen.h"

#include "base/notreached.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

PlatformScreen::PlatformScreen() = default;
PlatformScreen::~PlatformScreen() = default;

gfx::AcceleratedWidget PlatformScreen::GetLocalProcessWidgetAtPoint(
    const gfx::Point& point,
    const std::set<gfx::AcceleratedWidget>& ignore) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::kNullAcceleratedWidget;
}

bool PlatformScreen::IsAcceleratedWidgetUnderCursor(
    gfx::AcceleratedWidget widget) const {
  return GetAcceleratedWidgetAtScreenPoint(GetCursorScreenPoint()) == widget;
}

std::string PlatformScreen::GetCurrentWorkspace() {
  NOTIMPLEMENTED_LOG_ONCE();
  return {};
}

bool PlatformScreen::SetScreenSaverSuspended(bool suspend) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool PlatformScreen::IsScreenSaverActive() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

base::TimeDelta PlatformScreen::CalculateIdleTime() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return base::Seconds(0);
}

std::vector<base::Value> PlatformScreen::GetGpuExtraInfo(
    const gfx::GpuExtraInfo& gpu_extra_info) {
  return std::vector<base::Value>();
}

void PlatformScreen::SetDeviceScaleFactor(float scale) {}

void PlatformScreen::StorePlatformNameIntoListOfValues(
    std::vector<base::Value>& values,
    const std::string& platform_name) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("description", base::Value("Ozone platform"));
  dict.SetKey("value", base::Value(platform_name));
  values.push_back(std::move(dict));
}

}  // namespace ui
