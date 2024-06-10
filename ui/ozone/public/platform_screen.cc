// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/platform_screen.h"

#include <optional>

#include "base/notreached.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

PlatformScreen::PlatformScreen() = default;
PlatformScreen::~PlatformScreen() = default;

gfx::AcceleratedWidget PlatformScreen::GetLocalProcessWidgetAtPoint(
    const gfx::Point& point_in_dip,
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

PlatformScreen::PlatformScreenSaverSuspender::~PlatformScreenSaverSuspender() =
    default;

std::unique_ptr<PlatformScreen::PlatformScreenSaverSuspender>
PlatformScreen::SuspendScreenSaver() {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

bool PlatformScreen::IsScreenSaverActive() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

base::TimeDelta PlatformScreen::CalculateIdleTime() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return base::Seconds(0);
}

base::Value::List PlatformScreen::GetGpuExtraInfo(
    const gfx::GpuExtraInfo& gpu_extra_info) {
  return base::Value::List();
}

std::optional<float>
PlatformScreen::GetPreferredScaleFactorForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  return std::nullopt;
}

void PlatformScreen::StorePlatformNameIntoListOfValues(
    base::Value::List& values,
    const std::string& platform_name) {
  base::Value::Dict dict;
  dict.Set("description", "Ozone platform");
  dict.Set("value", platform_name);
  values.Append(std::move(dict));
}

}  // namespace ui
