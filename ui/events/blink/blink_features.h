// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_BLINK_FEATURES_H_
#define UI_EVENTS_BLINK_BLINK_FEATURES_H_

#include "base/feature_list.h"

namespace features {

extern const base::Feature kVsyncAlignedInputEvents;

// Enables resampling GestureScroll events on compositor thread.
extern const base::Feature kResamplingScrollEvents;

// This flag is used to set field parameters to choose predictor we use when
// resampling is disabled. It's used for gatherig accuracy metrics on finch
// without enabling resampling. It does not have any effect when the resampling
// flag is enabled.
extern const base::Feature kScrollPredictorTypeChoice;

// This feature allows native ET_MOUSE_EXIT events to be passed
// through to blink as mouse leave events. Traditionally these events were
// converted to mouse move events due to a number of inconsistencies on
// the native platforms. crbug.com/450631
extern const base::Feature kSendMouseLeaveEvents;

// When enabled, this feature prevents Blink from changing the hover state and
// dispatching mouse enter/exit events for elements under the mouse after the
// layout under the mouse cursor is changed.
extern const base::Feature kNoHoverAfterLayoutChange;

// When enabled, this feature prevents Blink from changing the hover state and
// dispatching mouse enter/exit events for elements under the mouse as the page
// is scrolled.
extern const base::Feature kNoHoverDuringScroll;
}

#endif  // UI_EVENTS_BLINK_BLINK_FEATURES_H_
