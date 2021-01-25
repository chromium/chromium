// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/latency_info.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "services/tracing/public/cpp/perfetto/flow_event_utils.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_latency_info.pbzero.h"

namespace {

using perfetto::protos::pbzero::ChromeLatencyInfo;
using perfetto::protos::pbzero::TrackEvent;

const size_t kMaxLatencyInfoNumber = 100;

ChromeLatencyInfo::LatencyComponentType GetComponentProtoEnum(
    ui::LatencyComponentType type) {
#define CASE_TYPE(t)      \
  case ui::t##_COMPONENT: \
    return ChromeLatencyInfo::COMPONENT_##t
  switch (type) {
    CASE_TYPE(INPUT_EVENT_LATENCY_BEGIN_RWH);
    CASE_TYPE(INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL);
    CASE_TYPE(INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL);
    CASE_TYPE(INPUT_EVENT_LATENCY_ORIGINAL);
    CASE_TYPE(INPUT_EVENT_LATENCY_UI);
    CASE_TYPE(INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN);
    CASE_TYPE(INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL);
    CASE_TYPE(INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_RENDERER_MAIN);
    CASE_TYPE(INPUT_EVENT_LATENCY_RENDERER_SWAP);
    CASE_TYPE(DISPLAY_COMPOSITOR_RECEIVED_FRAME);
    CASE_TYPE(INPUT_EVENT_GPU_SWAP_BUFFER);
    CASE_TYPE(INPUT_EVENT_LATENCY_FRAME_SWAP);
    default:
      NOTREACHED() << "Unhandled LatencyComponentType: " << type;
      return ChromeLatencyInfo::COMPONENT_UNSPECIFIED;
  }
#undef CASE_TYPE
}

bool IsInputLatencyBeginComponent(ui::LatencyComponentType type) {
  return type == ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT;
}

// This class is for converting latency info to trace buffer friendly format.
class LatencyInfoTracedValue
    : public base::trace_event::ConvertableToTraceFormat {
 public:
  static std::unique_ptr<ConvertableToTraceFormat> FromValue(
      std::unique_ptr<base::Value> value);

  void AppendAsTraceFormat(std::string* out) const override;

 private:
  explicit LatencyInfoTracedValue(base::Value* value);
  ~LatencyInfoTracedValue() override;

  std::unique_ptr<base::Value> value_;

  DISALLOW_COPY_AND_ASSIGN(LatencyInfoTracedValue);
};

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
LatencyInfoTracedValue::FromValue(std::unique_ptr<base::Value> value) {
  return std::unique_ptr<base::trace_event::ConvertableToTraceFormat>(
      new LatencyInfoTracedValue(value.release()));
}

LatencyInfoTracedValue::~LatencyInfoTracedValue() {
}

void LatencyInfoTracedValue::AppendAsTraceFormat(std::string* out) const {
  std::string tmp;
  base::JSONWriter::Write(*value_, &tmp);
  *out += tmp;
}

LatencyInfoTracedValue::LatencyInfoTracedValue(base::Value* value)
    : value_(value) {
}

constexpr const char kTraceCategoriesForAsyncEvents[] =
    "benchmark,latencyInfo,rail";

struct LatencyInfoEnabledInitializer {
  LatencyInfoEnabledInitializer() :
      latency_info_enabled(TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          kTraceCategoriesForAsyncEvents)) {
  }

  const unsigned char* latency_info_enabled;
};

static base::LazyInstance<LatencyInfoEnabledInitializer>::Leaky
  g_latency_info_enabled = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace ui {

LatencyInfo::LatencyInfo() : LatencyInfo(SourceEventType::UNKNOWN) {}

LatencyInfo::LatencyInfo(SourceEventType type)
    : trace_id_(-1),
      ukm_source_id_(ukm::kInvalidSourceId),
      coalesced_(false),
      began_(false),
      terminated_(false),
      source_event_type_(type),
      scroll_update_delta_(0),
      predicted_scroll_update_delta_(0),
      gesture_scroll_id_(0) {}

LatencyInfo::LatencyInfo(const LatencyInfo& other) = default;
LatencyInfo::LatencyInfo(LatencyInfo&& other) = default;

LatencyInfo::~LatencyInfo() {}

LatencyInfo::LatencyInfo(int64_t trace_id, bool terminated)
    : trace_id_(trace_id),
      ukm_source_id_(ukm::kInvalidSourceId),
      coalesced_(false),
      began_(false),
      terminated_(terminated),
      source_event_type_(SourceEventType::UNKNOWN),
      scroll_update_delta_(0),
      predicted_scroll_update_delta_(0),
      gesture_scroll_id_(0) {}

LatencyInfo& LatencyInfo::operator=(const LatencyInfo& other) = default;

bool LatencyInfo::Verify(const std::vector<LatencyInfo>& latency_info,
                         const char* referring_msg) {
  if (latency_info.size() > kMaxLatencyInfoNumber) {
    LOG(ERROR) << referring_msg << ", LatencyInfo vector size "
               << latency_info.size() << " is too big.";
    TRACE_EVENT_INSTANT1("input,benchmark", "LatencyInfo::Verify Fails",
                         TRACE_EVENT_SCOPE_GLOBAL,
                         "size", latency_info.size());
    return false;
  }
  return true;
}

void LatencyInfo::TraceIntermediateFlowEvents(
    const std::vector<LatencyInfo>& latency_info,
    perfetto::protos::pbzero::ChromeLatencyInfo::Step step) {
  for (auto& latency : latency_info) {
    if (latency.trace_id() == -1)
      continue;

    TRACE_EVENT(
        "input,benchmark", "LatencyInfo.Flow",
        [&latency, &step](perfetto::EventContext ctx) {
          ChromeLatencyInfo* info = ctx.event()->set_chrome_latency_info();
          info->set_step(step);
          info->set_trace_id(latency.trace_id());
          tracing::FillFlowEvent(ctx, TrackEvent::LegacyEvent::FLOW_INOUT,
                                 latency.trace_id());
        });
  }
}

void LatencyInfo::CopyLatencyFrom(const LatencyInfo& other,
                                  LatencyComponentType type) {
  // Don't clobber an existing trace_id_ or ukm_source_id_.
  if (trace_id_ == -1) {
    DCHECK_EQ(ukm_source_id_, ukm::kInvalidSourceId);
    DCHECK(latency_components().empty());
    trace_id_ = other.trace_id();
    ukm_source_id_ = other.ukm_source_id();
  } else {
    DCHECK_NE(ukm_source_id_, ukm::kInvalidSourceId);
  }

  for (const auto& lc : other.latency_components()) {
    if (lc.first == type) {
      AddLatencyNumberWithTimestamp(lc.first, lc.second);
    }
  }

  coalesced_ = other.coalesced();
  gesture_scroll_id_ = other.gesture_scroll_id();
  scroll_update_delta_ = other.scroll_update_delta();
  // TODO(tdresser): Ideally we'd copy |began_| here as well, but |began_|
  // isn't very intuitive, and we can actually begin multiple times across
  // copied events.
  terminated_ = other.terminated();
}

void LatencyInfo::AddNewLatencyFrom(const LatencyInfo& other) {
  // Don't clobber an existing trace_id_ or ukm_source_id_.
  if (trace_id_ == -1) {
    trace_id_ = other.trace_id();
  }

  if (ukm_source_id_ == ukm::kInvalidSourceId) {
    ukm_source_id_ = other.ukm_source_id();
  }

  for (const auto& lc : other.latency_components()) {
    if (!FindLatency(lc.first, nullptr)) {
      AddLatencyNumberWithTimestamp(lc.first, lc.second);
    }
  }

  coalesced_ = other.coalesced();
  gesture_scroll_id_ = other.gesture_scroll_id();
  scroll_update_delta_ = other.scroll_update_delta();
  // TODO(tdresser): Ideally we'd copy |began_| here as well, but |began_| isn't
  // very intuitive, and we can actually begin multiple times across copied
  // events.
  terminated_ = other.terminated();
}

void LatencyInfo::AddLatencyNumber(LatencyComponentType component) {
  AddLatencyNumberWithTimestampImpl(component, base::TimeTicks::Now(), nullptr);
}

void LatencyInfo::AddLatencyNumberWithTraceName(
    LatencyComponentType component,
    const char* trace_name_str) {
  AddLatencyNumberWithTimestampImpl(component, base::TimeTicks::Now(),
                                    trace_name_str);
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

    TRACE_EVENT("input,benchmark", "LatencyInfo.Flow",
                [this](perfetto::EventContext ctx) {
                  ChromeLatencyInfo* info =
                      ctx.event()->set_chrome_latency_info();
                  info->set_trace_id(trace_id_);
                  tracing::FillFlowEvent(ctx, TrackEvent::LegacyEvent::FLOW_OUT,
                                         trace_id_);
                });
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
    TRACE_EVENT_END(
        kTraceCategoriesForAsyncEvents, perfetto::Track::Global(trace_id_),
        [this](perfetto::EventContext ctx) {
          ChromeLatencyInfo* info = ctx.event()->set_chrome_latency_info();
          for (const auto& lc : latency_components_) {
            ChromeLatencyInfo::ComponentInfo* component =
                info->add_component_info();

            component->set_component_type(GetComponentProtoEnum(lc.first));
            component->set_time_us(lc.second.since_origin().InMicroseconds());
          }

          if (gesture_scroll_id_ > 0) {
            info->set_gesture_scroll_id(gesture_scroll_id_);
          }

          info->set_trace_id(trace_id_);
          info->set_is_coalesced(coalesced_);
        });
  }

  TRACE_EVENT("input,benchmark", "LatencyInfo.Flow",
              [this](perfetto::EventContext ctx) {
                ChromeLatencyInfo* info =
                    ctx.event()->set_chrome_latency_info();
                info->set_trace_id(trace_id_);
                tracing::FillFlowEvent(ctx, TrackEvent::LegacyEvent::FLOW_IN,
                                       trace_id_);
              });
}

void LatencyInfo::CoalesceScrollUpdateWith(const LatencyInfo& other) {
  base::TimeTicks other_timestamp;
  if (other.FindLatency(INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT,
                        &other_timestamp)) {
    latency_components_
        [INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT] =
            other_timestamp;
  }

  scroll_update_delta_ += other.scroll_update_delta();
  predicted_scroll_update_delta_ += other.predicted_scroll_update_delta();
}

LatencyInfo LatencyInfo::ScaledBy(float scale) const {
  ui::LatencyInfo scaled_latency_info(*this);
  scaled_latency_info.set_scroll_update_delta(scroll_update_delta_ * scale);
  scaled_latency_info.set_predicted_scroll_update_delta(
      predicted_scroll_update_delta_ * scale);
  return scaled_latency_info;
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
