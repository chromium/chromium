// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/blink_features.h"

namespace features {

const base::Feature kResamplingScrollEvents{"ResamplingScrollEvents",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kFilteringScrollPrediction{
    "FilteringScrollPrediction", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kKalmanHeuristics{"KalmanHeuristics",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kKalmanDirectionCutOff{"KalmanDirectionCutOff",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSendMouseLeaveEvents{"SendMouseLeaveEvents",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kUpdateHoverAtBeginFrame{"UpdateHoverAtBeginFrame",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCompositorTouchAction{"CompositorTouchAction",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kFallbackCursorMode{"FallbackCursorMode",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDontSendKeyEventsToJavascript{
    "DontSendKeyEventsToJavascript", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSkipTouchEventFilter{"SkipTouchEventFilter",
                                          base::FEATURE_ENABLED_BY_DEFAULT};
const char kSkipTouchEventFilterTypeParamName[] = "type";
const char kSkipTouchEventFilterTypeParamValueDiscrete[] = "discrete";
const char kSkipTouchEventFilterTypeParamValueAll[] = "all";
const char kSkipTouchEventFilterFilteringProcessParamName[] =
    "skip_filtering_process";
const char kSkipTouchEventFilterFilteringProcessParamValueBrowser[] = "browser";
const char kSkipTouchEventFilterFilteringProcessParamValueBrowserAndRenderer[] =
    "browser_and_renderer";
}  // namespace features
