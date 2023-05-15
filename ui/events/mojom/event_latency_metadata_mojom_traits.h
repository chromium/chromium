// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_MOJOM_EVENT_LATENCY_METADATA_MOJOM_TRAITS_H_
#define UI_EVENTS_MOJOM_EVENT_LATENCY_METADATA_MOJOM_TRAITS_H_

#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/events/event_latency_metadata.h"
#include "ui/events/mojom/event_latency_metadata.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<ui::mojom::EventLatencyMetadataDataView,
                    ui::EventLatencyMetadata> {
  static base::TimeTicks arrived_in_browser_main_timestamp(
      const ui::EventLatencyMetadata& event_latency_metadata) {
    return event_latency_metadata.arrived_in_browser_main_timestamp;
  }

  static base::TimeTicks scrolls_blocking_touch_dispatched_to_renderer(
      const ui::EventLatencyMetadata& event_latency_metadata) {
    return event_latency_metadata.scrolls_blocking_touch_dispatched_to_renderer;
  }

  static base::TimeTicks dispatched_to_renderer(
      const ui::EventLatencyMetadata& event_latency_metadata) {
    return event_latency_metadata.dispatched_to_renderer;
  }
  static bool Read(ui::mojom::EventLatencyMetadataDataView in,
                   ui::EventLatencyMetadata* out);
};

}  // namespace mojo

#endif  // UI_EVENTS_MOJOM_EVENT_LATENCY_METADATA_MOJOM_TRAITS_H_