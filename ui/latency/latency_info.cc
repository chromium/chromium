// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/latency_info.h"

#include <stddef.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "services/tracing/public/cpp/perfetto/flow_event_utils.h"
#include "services/tracing/public/cpp/perfetto/macros.h"

namespace {

using ::perfetto::protos::pbzero::ChromeLatencyInfo2;
using ::perfetto::protos::pbzero::TrackEvent;

const size_t kMaxLatencyInfoNumber = 100;

ChromeLatencyInfo2::LatencyComponentType GetComponentProtoEnum(
    ui::LatencyComponentType type) {
#define CASE_TYPE(t)      \
  case ui::t##_COMPONENT: \
    return ChromeLatencyInfo2::LatencyComponentType::COMPONENT_##t
  switch (type) {
    CASE_TYPE(INPUT_EVENT_LATENCY_BEGIN_RWH);
    CASE_TYPE(INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL);
    CASE_TYPE(INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL);
    CASE_TYPE(INPUT_EVENT_LATENCY_ORIGINAL);
    CASE_TYPE(INPUT_EVENT_LATENCY_UI);
    CASE_TYPE(INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN);
    CASE_TYPE(INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL);
    CASE_TYPE(INPUT_EVENT_LATENCY_RENDERER_MAIN);
    CASE_TYPE(INPUT_EVENT_LATENCY_RENDERER_SWAP);
    CASE_TYPE(DISPLAY_COMPOSITOR_RECEIVED_FRAME);
    CASE_TYPE(INPUT_EVENT_GPU_SWAP_BUFFER);
    CASE_TYPE(INPUT_EVENT_LATENCY_FRAME_SWAP);
    default:
      NOTREACHED_IN_MIGRATION() << "Unhandled LatencyComponentType: " << type;
      return ChromeLatencyInfo2::LatencyComponentType::COMPONENT_UNSPECIFIED;
  }
#undef CASE_TYPE
}

bool IsInputLatencyBeginComponent(ui::LatencyComponentType type) {
  return type == ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT;
}

constexpr const char kTraceCategoriesForAsyncEvents[] =
    "benchmark,latencyInfo,rail,input.scrolling";

struct LatencyInfoEnabledInitializer {
  LatencyInfoEnabledInitializer() :
      latency_info_enabled(TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          kTraceCategoriesForAsyncEvents)) {
  }

  raw_ptr<const unsigned char> latency_info_enabled;
};

static base::LazyInstance<LatencyInfoEnabledInitializer>::Leaky
  g_latency_info_enabled = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace ui {

LatencyInfo::LatencyInfo() = default;

LatencyInfo::LatencyInfo(const LatencyInfo& other) = default;
LatencyInfo::LatencyInfo(LatencyInfo&& other) = default;

LatencyInfo::~LatencyInfo() = default;

LatencyInfo::LatencyInfo(int64_t trace_id, bool terminated)
    : trace_id_(trace_id), terminated_(terminated) {}

LatencyInfo& LatencyInfo::operator=(const LatencyInfo& other) = default;

bool LatencyInfo::Verify(const std::vector<LatencyInfo>& latency_info,
                         const char* referring_msg) {
  if (latency_info.size() > kMaxLatencyInfoNumber) {
    LOG(ERROR) << referring_msg << ", LatencyInfo vector size "
               << latency_info.size() << " is too big.";
    TRACE_EVENT_INSTANT1("input,benchmark,latencyInfo",
                         "LatencyInfo::Verify Fails", TRACE_EVENT_SCOPE_GLOBAL,
                         "size", latency_info.size());
    return false;
  }
  return true;
}

ChromeLatencyInfo2* LatencyInfo::FillTraceEvent(
    perfetto::EventContext& ctx,
    int64_t latency_trace_id,
    ChromeLatencyInfo2::Step step,
    std::optional<ChromeLatencyInfo2::InputType> input_type,
    std::optional<ChromeLatencyInfo2::InputResultState> input_result_state) {
  // The flow id needs to be written first. Due to ProtoZero write semantics, we
  // need to write to submessages in one go, so we write the flow id first and
  // then can write fields of `ChromeLatencyInfo2`.
  ctx.event()->add_flow_ids(latency_trace_id);

  auto* info = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                   ->set_chrome_latency_info();
  info->set_trace_id(latency_trace_id);
  info->set_step(step);
  if (input_type.has_value()) {
    info->set_input_type(input_type.value());
  }
  if (input_result_state.has_value()) {
    info->set_input_result_state(input_result_state.value());
  }
  return info;
}

void LatencyInfo::AddNewLatencyFrom(const LatencyInfo& other) {
  // Don't clobber an existing trace_id_.
  if (trace_id_ == -1) {
    trace_id_ = other.trace_id();
  }

  for (const auto& lc : other.latency_components()) {
    if (!FindLatency(lc.first, nullptr)) {
      AddLatencyNumberWithTimestamp(lc.first, lc.second);
    }
  }

  coalesced_ = other.coalesced();
  gesture_scroll_id_ = other.gesture_scroll_id();
  touch_trace_id_ = other.touch_trace_id();
  // TODO(tdresser): Ideally we'd copy |began_| here as well, but |began_| isn't
  // very intuitive, and we can actually begin multiple times across copied
  // events.
  terminated_ = other.terminated();
}

void LatencyInfo::AddLatencyNumber(LatencyComponentType component) {
  AddLatencyNumberWithTimestampImpl(component, base::TimeTicks::Now(), nullptr);
}

void LatencyInfo::AddLatencyNumberWithTraceName(LatencyComponentType component,
                                                const char* trace_name_str,
                                                base::TimeTicks now) {
  AddLatencyNumberWithTimestampImpl(component, now, trace_name_str);
}

void LatencyInfo::AddLatencyNumberWithTimestamp(LatencyComponentType component,
                                                base::TimeTicks time) {
  AddLatencyNumberWithTimestampImpl(component, time, nullptr);
}

void LatencyInfo::AddLatencyNumberWithTimestampImpl(
    LatencyComponentType component,
    base::TimeTicks time,
    const char* trace_name_str) {
  const unsigned char* latency_info_enabled =
      g_latency_info_enabled.Get().latency_info_enabled;

  if (IsInputLatencyBeginComponent(component)) {
    // Should only ever add begin component once.
    CHECK(!began_);
    began_ = true;
    // We should have a trace ID assigned by now.
    DCHECK(trace_id_ != -1);

    if (*latency_info_enabled) {
      // The timestamp for ASYNC_BEGIN trace event is used for drawing the
      // beginning of the trace event in trace viewer. For better visualization,
      // for an input event, we want to draw the beginning as when the event is
      // originally created, e.g. the timestamp of its ORIGINAL/UI_COMPONENT,
      // not when we actually issue the ASYNC_BEGIN trace event.
      base::TimeTicks begin_timestamp;
      base::TimeTicks ts;
      if (FindLatency(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                      &begin_timestamp) ||
          FindLatency(INPUT_EVENT_LATENCY_UI_COMPONENT, &begin_timestamp)) {
        ts = begin_timestamp;
      } else {
        ts = base::TimeTicks::Now();
      }

      TRACE_EVENT_BEGIN(kTraceCategoriesForAsyncEvents,
                        perfetto::StaticString{trace_name_str},
                        perfetto::Track::Global(trace_id_), ts);
    }
  }

  auto it = latency_components_.find(component);
  DCHECK(it == latency_components_.end());
  latency_components_[component] = time;

  if (component == INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT)
    Terminate();
}

void LatencyInfo::Terminate() {
  if (!began_)
    return;

  // Should only ever be terminated once.
  CHECK(!terminated_);
  terminated_ = true;

  if (*g_latency_info_enabled.Get().latency_info_enabled) {
    base::TimeTicks gpu_swap_end_timestamp;
    if (!this->FindLatency(INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT,
                           &gpu_swap_end_timestamp)) {
      gpu_swap_end_timestamp = base::TimeTicks::Now();
    }
    TRACE_EVENT_END(
        kTraceCategoriesForAsyncEvents, perfetto::Track::Global(trace_id_),
        gpu_swap_end_timestamp, [this](perfetto::EventContext ctx) {
          auto* info = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                           ->set_chrome_latency_info();
          for (const auto& lc : latency_components_) {
            auto* component = info->add_component_info();

            component->set_component_type(GetComponentProtoEnum(lc.first));
            component->set_time_us(lc.second.since_origin().InMicroseconds());
          }

          if (gesture_scroll_id_ > 0) {
            info->set_gesture_scroll_id(gesture_scroll_id_);
          }
          if (touch_trace_id_ > 0) {
            info->set_touch_id(touch_trace_id_);
          }

          info->set_trace_id(trace_id_);
          info->set_is_coalesced(coalesced_);
        });
  }
}

bool LatencyInfo::FindLatency(LatencyComponentType type,
                              base::TimeTicks* output) const {
  auto it = latency_components_.find(type);
  if (it == latency_components_.end())
    return false;
  if (output)
    *output = it->second;
  return true;
}

}  // namespace ui
