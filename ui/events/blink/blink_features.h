// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_BLINK_FEATURES_H_
#define UI_EVENTS_BLINK_BLINK_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace features {

// Enables resampling GestureScroll events on compositor thread.
COMPONENT_EXPORT(BLINK_FEATURES)
extern const base::Feature kResamplingScrollEvents;

// Enables filtering of predicted scroll events on compositor thread.
COMPONENT_EXPORT(BLINK_FEATURES)
extern const base::Feature kFilteringScrollPrediction;

// Enables changing the influence of acceleration based on change of direction.
COMPONENT_EXPORT(BLINK_FEATURES)
extern const base::Feature kKalmanHeuristics;

// Enables discarding the prediction if the predicted direction is opposite from
// the current direction.
COMPONENT_EXPORT(BLINK_FEATURES)
extern const base::Feature kKalmanDirectionCutOff;

// This feature allows native ET_MOUSE_EXIT events to be passed
// through to blink as mouse leave events. Traditionally these events were
// converted to mouse move events due to a number of inconsistencies on
// the native platforms. crbug.com/450631
COMPONENT_EXPORT(BLINK_FEATURES)
extern const base::Feature kSendMouseLeaveEvents;

// When enabled, this feature prevents Blink from changing the hover state and
// dispatching mouse enter/exit events for elements under the mouse after the
// layout under the mouse cursor is changed or the page is scrolled.
COMPONENT_EXPORT(BLINK_FEATURES)
extern const base::Feature kUpdateHoverAtBeginFrame;

// Enables handling touch events in compositor using impl side touch action
// knowledge.
COMPONENT_EXPORT(BLINK_FEATURES)
extern const base::Feature kCompositorTouchAction;

// Enables fallback cursor mode for dpad devices.
COMPONENT_EXPORT(BLINK_FEATURES)
extern const base::Feature kFallbackCursorMode;

// When enabled, this feature prevent blink sending key event to web unless it
// is on installed PWA.
COMPONENT_EXPORT(BLINK_FEATURES)
extern const base::Feature kDontSendKeyEventsToJavascript;

// Skips the browser touch event filter, ensuring that events that reach the
// queue and would otherwise be filtered out will instead be passed onto the
// renderer compositor process as long as the page hasn't timed out. If
// skip_filtering_process is browser_and_renderer, also skip the renderer cc
// touch event filter, ensuring that events will be passed onto the renderer
// main thread. Which event types will be always forwarded is controlled by the
// "type" FeatureParam,
// which can be either "discrete" (default) or "all".
COMPONENT_EXPORT(BLINK_FEATURES)
extern const base::Feature kSkipTouchEventFilter;
COMPONENT_EXPORT(BLINK_FEATURES)
extern const char kSkipTouchEventFilterTypeParamName[];
COMPONENT_EXPORT(BLINK_FEATURES)
extern const char kSkipTouchEventFilterTypeParamValueDiscrete[];
COMPONENT_EXPORT(BLINK_FEATURES)
extern const char kSkipTouchEventFilterTypeParamValueAll[];
COMPONENT_EXPORT(BLINK_FEATURES)
extern const char kSkipTouchEventFilterFilteringProcessParamName[];
COMPONENT_EXPORT(BLINK_FEATURES)
extern const char kSkipTouchEventFilterFilteringProcessParamValueBrowser[];
COMPONENT_EXPORT(BLINK_FEATURES)
extern const char
    kSkipTouchEventFilterFilteringProcessParamValueBrowserAndRenderer[];
}

#endif  // UI_EVENTS_BLINK_BLINK_FEATURES_H_
