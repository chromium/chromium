// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/mojom/latency_info_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

namespace {

ui::mojom::SourceEventType UISourceEventTypeToMojo(ui::SourceEventType type) {
  switch (type) {
    case ui::SourceEventType::UNKNOWN:
      return ui::mojom::SourceEventType::UNKNOWN;
    case ui::SourceEventType::WHEEL:
      return ui::mojom::SourceEventType::WHEEL;
    case ui::SourceEventType::MOUSE:
      return ui::mojom::SourceEventType::MOUSE;
    case ui::SourceEventType::TOUCH:
      return ui::mojom::SourceEventType::TOUCH;
    case ui::SourceEventType::INERTIAL:
      return ui::mojom::SourceEventType::INERTIAL;
    case ui::SourceEventType::KEY_PRESS:
      return ui::mojom::SourceEventType::KEY_PRESS;
    case ui::SourceEventType::TOUCHPAD:
      return ui::mojom::SourceEventType::TOUCHPAD;
    case ui::SourceEventType::SCROLLBAR:
      return ui::mojom::SourceEventType::SCROLLBAR;
    case ui::SourceEventType::OTHER:
      return ui::mojom::SourceEventType::OTHER;
  }
  NOTREACHED();
  return ui::mojom::SourceEventType::UNKNOWN;
}

ui::SourceEventType MojoSourceEventTypeToUI(ui::mojom::SourceEventType type) {
  switch (type) {
    case ui::mojom::SourceEventType::UNKNOWN:
      return ui::SourceEventType::UNKNOWN;
    case ui::mojom::SourceEventType::WHEEL:
      return ui::SourceEventType::WHEEL;
    case ui::mojom::SourceEventType::MOUSE:
      return ui::SourceEventType::MOUSE;
    case ui::mojom::SourceEventType::TOUCH:
      return ui::SourceEventType::TOUCH;
    case ui::mojom::SourceEventType::INERTIAL:
      return ui::SourceEventType::INERTIAL;
    case ui::mojom::SourceEventType::KEY_PRESS:
      return ui::SourceEventType::KEY_PRESS;
    case ui::mojom::SourceEventType::TOUCHPAD:
      return ui::SourceEventType::TOUCHPAD;
    case ui::mojom::SourceEventType::SCROLLBAR:
      return ui::SourceEventType::SCROLLBAR;
    case ui::mojom::SourceEventType::OTHER:
      return ui::SourceEventType::OTHER;
  }
  NOTREACHED();
  return ui::SourceEventType::UNKNOWN;
}

}  // namespace

// static
const std::string&
StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::trace_name(
    const ui::LatencyInfo& info) {
  return info.trace_name_;
}

// static
const ui::LatencyInfo::LatencyMap&
StructTraits<ui::mojom::LatencyInfoDataView,
             ui::LatencyInfo>::latency_components(const ui::LatencyInfo& info) {
  return info.latency_components();
}

// static
int64_t StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::trace_id(
    const ui::LatencyInfo& info) {
  return info.trace_id();
}

// static
ukm::SourceId
StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::ukm_source_id(
    const ui::LatencyInfo& info) {
  return info.ukm_source_id();
}

// static
bool StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::coalesced(
    const ui::LatencyInfo& info) {
  return info.coalesced();
}

// static
bool StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::began(
    const ui::LatencyInfo& info) {
  return info.began();
}

// static
bool StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::terminated(
    const ui::LatencyInfo& info) {
  return info.terminated();
}

// static
ui::mojom::SourceEventType
StructTraits<ui::mojom::LatencyInfoDataView,
             ui::LatencyInfo>::source_event_type(const ui::LatencyInfo& info) {
  return UISourceEventTypeToMojo(info.source_event_type());
}

// static
float StructTraits<ui::mojom::LatencyInfoDataView,
                   ui::LatencyInfo>::scroll_update_delta(const ui::LatencyInfo&
                                                             info) {
  return info.scroll_update_delta();
}

// static
float StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::
    predicted_scroll_update_delta(const ui::LatencyInfo& info) {
  return info.predicted_scroll_update_delta();
}

// static
bool StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::Read(
    ui::mojom::LatencyInfoDataView data,
    ui::LatencyInfo* out) {
  if (!data.ReadTraceName(&out->trace_name_))
    return false;
  if (!data.ReadLatencyComponents(&out->latency_components_))
    return false;
  out->trace_id_ = data.trace_id();
  out->ukm_source_id_ = data.ukm_source_id();
  out->coalesced_ = data.coalesced();
  out->began_ = data.began();
  out->terminated_ = data.terminated();
  out->source_event_type_ = MojoSourceEventTypeToUI(data.source_event_type());
  out->scroll_update_delta_ = data.scroll_update_delta();
  out->predicted_scroll_update_delta_ = data.predicted_scroll_update_delta();

  return true;
}

// static
ui::mojom::LatencyComponentType
EnumTraits<ui::mojom::LatencyComponentType, ui::LatencyComponentType>::ToMojom(
    ui::LatencyComponentType type) {
  switch (type) {
    case ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_UI_COMPONENT:
      return ui::mojom::LatencyComponentType::INPUT_EVENT_LATENCY_UI_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT;
    case ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT:
      return ui::mojom::LatencyComponentType::
          DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT;
    case ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT;
  }
  NOTREACHED();
  return ui::mojom::LatencyComponentType::kMaxValue;
}

// static
bool EnumTraits<ui::mojom::LatencyComponentType, ui::LatencyComponentType>::
    FromMojom(ui::mojom::LatencyComponentType input,
              ui::LatencyComponentType* output) {
  switch (input) {
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::INPUT_EVENT_LATENCY_UI_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_UI_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT:
      *output = ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT:
      *output = ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT;
      return true;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT:
      *output = ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT;
      return true;
  }
  return false;
}

}  // namespace mojo
