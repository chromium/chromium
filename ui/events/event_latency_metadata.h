// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_LATENCY_METADATA_H_
#define UI_EVENTS_EVENT_LATENCY_METADATA_H_

#include "base/time/time.h"

namespace ui {

// The struct contains metadata about EventLatency events.
// There should only be POD classes in this struct to keep the metadata to a
// minimum.
struct EventLatencyMetadata {
  // Time when event arrived in the BrowserMain thread.
  base::TimeTicks arrived_in_browser_main_timestamp;

  // This field is used only by scroll events to understand when the related
  // blocking touch move was dispatched to Renderer. If the related touch move
  // wasn't blocking, this field is not set.
  base::TimeTicks scrolls_blocking_touch_dispatched_to_renderer;

  // Time when event was disppatched to the Renderer from the Browser.
  base::TimeTicks dispatched_to_renderer;
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_LATENCY_METADATA_H_