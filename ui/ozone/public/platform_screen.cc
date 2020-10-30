// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/platform_screen.h"

#include "base/notreached.h"
#include "base/time/time.h"

namespace ui {

PlatformScreen::PlatformScreen() = default;
PlatformScreen::~PlatformScreen() = default;

gfx::AcceleratedWidget PlatformScreen::GetLocalProcessWidgetAtPoint(
    const gfx::Point& point,
    const std::set<gfx::AcceleratedWidget>& ignore) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::kNullAcceleratedWidget;
}

std::string PlatformScreen::GetCurrentWorkspace() {
  NOTIMPLEMENTED_LOG_ONCE();
  return {};
}

void PlatformScreen::SetScreenSaverSuspended(bool suspend) {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool PlatformScreen::IsScreenSaverActive() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

base::TimeDelta PlatformScreen::CalculateIdleTime() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return base::TimeDelta::FromSeconds(0);
}

base::Value PlatformScreen::GetGpuExtraInfoAsListValue(
    const gfx::GpuExtraInfo& gpu_extra_info) {
  return base::Value(base::Value::Type::LIST);
}

}  // namespace ui
