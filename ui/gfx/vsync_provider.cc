// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/vsync_provider.h"

namespace gfx {

void FixedVSyncProvider::GetVSyncParameters(UpdateVSyncCallback callback) {
  std::move(callback).Run(timebase_, interval_);
}

bool FixedVSyncProvider::GetVSyncParametersIfAvailable(
    base::TimeTicks* timebase,
    base::TimeDelta* interval) {
  *timebase = timebase_;
  *interval = interval_;
  return true;
}

bool FixedVSyncProvider::SupportGetVSyncParametersIfAvailable() const {
  return true;
}

bool FixedVSyncProvider::IsHWClock() const {
  return false;
}

}  // namespace gfx
