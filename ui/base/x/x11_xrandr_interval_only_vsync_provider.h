// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_XRANDR_INTERVAL_ONLY_VSYNC_PROVIDER_H_
#define UI_BASE_X_X11_XRANDR_INTERVAL_ONLY_VSYNC_PROVIDER_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "ui/gfx/vsync_provider.h"

namespace ui {

class COMPONENT_EXPORT(UI_BASE_X) XrandrIntervalOnlyVSyncProvider
    : public gfx::VSyncProvider {
 public:
  explicit XrandrIntervalOnlyVSyncProvider();

  // gfx::VSyncProvider:
  void GetVSyncParameters(UpdateVSyncCallback callback) override;
  bool GetVSyncParametersIfAvailable(base::TimeTicks* timebase,
                                     base::TimeDelta* interval) override;
  bool SupportGetVSyncParametersIfAvailable() const override;
  bool IsHWClock() const override;

 private:
  base::TimeDelta interval_;
  static const int kCallsBetweenUpdates = 100;
  int calls_since_last_update_ = kCallsBetweenUpdates;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_XRANDR_INTERVAL_ONLY_VSYNC_PROVIDER_H_
