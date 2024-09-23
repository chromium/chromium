// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/mojom/latency_info_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

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
int64_t
StructTraits<ui::mojom::LatencyInfoDataView,
             ui::LatencyInfo>::gesture_scroll_id(const ui::LatencyInfo& info) {
  return info.gesture_scroll_id();
}

// static
int64_t
StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::touch_trace_id(
    const ui::LatencyInfo& info) {
  return info.touch_trace_id();
}

// static
bool StructTraits<ui::mojom::LatencyInfoDataView, ui::LatencyInfo>::Read(
    ui::mojom::LatencyInfoDataView data,
    ui::LatencyInfo* out) {
  if (!data.ReadLatencyComponents(&out->latency_components_))
    return false;
  out->trace_id_ = data.trace_id();
  out->coalesced_ = data.coalesced();
  out->began_ = data.began();
  out->terminated_ = data.terminated();
  out->gesture_scroll_id_ = data.gesture_scroll_id();
  out->touch_trace_id_ = data.touch_trace_id();

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
  NOTREACHED_IN_MIGRATION();
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
  }
  return false;
}

}  // namespace mojo
