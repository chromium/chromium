// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/mojom/event_latency_metadata_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    ui::mojom::EventLatencyMetadataDataView,
    ui::EventLatencyMetadata>::Read(ui::mojom::EventLatencyMetadataDataView in,
                                    ui::EventLatencyMetadata* out) {
  DCHECK(out != nullptr);

  if (!in.ReadArrivedInBrowserMainTimestamp(
          &out->arrived_in_browser_main_timestamp) ||
      !in.ReadScrollsBlockingTouchDispatchedToRenderer(
          &out->scrolls_blocking_touch_dispatched_to_renderer) ||
      !in.ReadDispatchedToRenderer(&out->dispatched_to_renderer)) {
    return false;
  }

  return true;
}

}  // namespace mojo
