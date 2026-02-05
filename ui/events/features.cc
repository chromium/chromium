// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/features.h"

#include "build/build_config.h"

namespace ui {

// TODO(https://crbug.com/475611763): Cleanup feature flag.
BASE_FEATURE(kCompensateGestureDetectorTimeouts,
             base::FEATURE_ENABLED_BY_DEFAULT
);

const base::FeatureParam<bool>
    kCompensateGestureTimeoutsForLongDelayedSequences{
        &kCompensateGestureDetectorTimeouts,
        /*name=*/"compensate_gesture_timeouts_for_long_delayed_sequences",
        /*default_value=*/false};

BASE_FEATURE(kLegacyKeyRepeatSynthesis, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFixDoubleClickNotWorking, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace ui
