// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_xrandr_interval_only_vsync_provider.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/x/x11_display_util.h"

namespace ui {

XrandrIntervalOnlyVSyncProvider::XrandrIntervalOnlyVSyncProvider()
    : interval_(base::TimeDelta::FromSeconds(1 / 60.)) {}

void XrandrIntervalOnlyVSyncProvider::GetVSyncParameters(
    UpdateVSyncCallback callback) {
  if (++calls_since_last_update_ >= kCallsBetweenUpdates) {
    calls_since_last_update_ = 0;
    interval_ = GetPrimaryDisplayRefreshIntervalFromXrandr();
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::TimeTicks(), interval_));
}

bool XrandrIntervalOnlyVSyncProvider::GetVSyncParametersIfAvailable(
    base::TimeTicks* timebase,
    base::TimeDelta* interval) {
  return false;
}

bool XrandrIntervalOnlyVSyncProvider::SupportGetVSyncParametersIfAvailable()
    const {
  return false;
}

bool XrandrIntervalOnlyVSyncProvider::IsHWClock() const {
  return false;
}

}  // namespace ui
