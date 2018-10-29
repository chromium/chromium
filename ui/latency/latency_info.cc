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
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"

namespace {

const size_t kMaxLatencyInfoNumber = 100;

const char* GetComponentName(ui::LatencyComponentType type) {
#define CASE_TYPE(t) case ui::t:  return #t
  switch (type) {
    CASE_TYPE(INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT);
    CASE_TYPE(LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT);
    CASE_TYPE(LATENCY_BEGIN_FRAME_RENDERER_MAIN_COMPONENT);
    CASE_TYPE(LATENCY_BEGIN_FRAME_RENDERER_INVALIDATE_COMPONENT);
    CASE_TYPE(LATENCY_BEGIN_FRAME_RENDERER_COMPOSITOR_COMPONENT);
    CASE_TYPE(LATENCY_BEGIN_FRAME_UI_MAIN_COMPONENT);
    CASE_TYPE(LATENCY_BEGIN_FRAME_UI_COMPOSITOR_COMPONENT);
    CASE_TYPE(LATENCY_BEGIN_FRAME_DISPLAY_COMPOSITOR_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_UI_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_FORWARD_SCROLL_UPDATE_TO_MAIN_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT);
    CASE_TYPE(DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT);
    CASE_TYPE(INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT);
    CASE_TYPE(INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT);
    default:
      DLOG(WARNING) << "Unhandled LatencyComponentType.\n";
      break;
  }
#undef CASE_TYPE
  return "unknown";
}

bool IsInputLatencyBeginComponent(ui::LatencyComponentType type) {
  return type == ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT;
}

bool IsTraceBeginComponent(ui::LatencyComponentType type) {
  return (IsInputLatencyBeginComponent(type) ||
          type == ui::LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT);
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

const char kTraceCategoriesForAsyncEvents[] = "benchmark,latencyInfo,rail";

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
      source_event_type_(type) {}

LatencyInfo::LatencyInfo(const LatencyInfo& other) = default;

LatencyInfo::~LatencyInfo() {}

LatencyInfo::LatencyInfo(int64_t trace_id, bool terminated)
    : trace_id_(trace_id),
      ukm_source_id_(ukm::kInvalidSourceId),
      coalesced_(false),
      began_(false),
      terminated_(terminated),
      source_event_type_(SourceEventType::UNKNOWN) {}

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
    const char* event_name) {
  for (auto& latency : latency_info) {
    if (latency.trace_id() == -1)
      continue;
    TRACE_EVENT_WITH_FLOW1("input,benchmark", "LatencyInfo.Flow",
                           TRACE_ID_DONT_MANGLE(latency.trace_id()),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "step", event_name);
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
      AddLatencyNumberWithTimestamp(lc.first, lc.second, 1);
    }
  }

  coalesced_ = other.coalesced();
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
      AddLatencyNumberWithTimestamp(lc.first, lc.second, 1);
    }
  }

  coalesced_ = other.coalesced();
  // TODO(tdresser): Ideally we'd copy |began_| here as well, but |began_| isn't
  // very intuitive, and we can actually begin multiple times across copied
  // events.
  terminated_ = other.terminated();
}

void LatencyInfo::AddLatencyNumber(LatencyComponentType component) {
  AddLatencyNumberWithTimestampImpl(component, base::TimeTicks::Now(), 1,
                                    nullptr);
}

void LatencyInfo::AddLatencyNumberWithTraceName(
    LatencyComponentType component,
    const char* trace_name_str) {
  AddLatencyNumberWithTimestampImpl(component, base::TimeTicks::Now(), 1,
                                    trace_name_str);
}

void LatencyInfo::AddLatencyNumberWithTimestamp(
    LatencyComponentType component,
    base::TimeTicks time,
    uint32_t event_count) {
  AddLatencyNumberWithTimestampImpl(component, time, event_count, nullptr);
}

void LatencyInfo::AddLatencyNumberWithTimestampImpl(
    LatencyComponentType component,
    base::TimeTicks time,
    uint32_t event_count,
    const char* trace_name_str) {
  const unsigned char* latency_info_enabled =
      g_latency_info_enabled.Get().latency_info_enabled;

  if (IsTraceBeginComponent(component)) {
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

      if (trace_name_str) {
        if (IsInputLatencyBeginComponent(component))
          trace_name_ = std::string("InputLatency::") + trace_name_str;
        else
          trace_name_ = std::string("Latency::") + trace_name_str;
      }

      TRACE_EVENT_COPY_ASYNC_BEGIN_WITH_TIMESTAMP0(
          kTraceCategoriesForAsyncEvents,
          trace_name_.c_str(),
          TRACE_ID_DONT_MANGLE(trace_id_),
          ts);
    }

    TRACE_EVENT_WITH_FLOW1("input,benchmark",
                           "LatencyInfo.Flow",
                           TRACE_ID_DONT_MANGLE(trace_id_),
                           TRACE_EVENT_FLAG_FLOW_OUT,
                           "trace_id", trace_id_);
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
    TRACE_EVENT_COPY_ASYNC_END1(
        kTraceCategoriesForAsyncEvents, trace_name_.c_str(),
        TRACE_ID_DONT_MANGLE(trace_id_), "data", AsTraceableData());
  }

  TRACE_EVENT_WITH_FLOW0("input,benchmark", "LatencyInfo.Flow",
                         TRACE_ID_DONT_MANGLE(trace_id_),
                         TRACE_EVENT_FLAG_FLOW_IN);
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
LatencyInfo::AsTraceableData() {
  std::unique_ptr<base::DictionaryValue> record_data(
      new base::DictionaryValue());
  for (const auto& lc : latency_components_) {
    std::unique_ptr<base::DictionaryValue> component_info(
        new base::DictionaryValue());
    component_info->SetDouble(
        "time", static_cast<double>(lc.second.since_origin().InMicroseconds()));
    record_data->Set(GetComponentName(lc.first), std::move(component_info));
  }
  record_data->SetDouble("trace_id", static_cast<double>(trace_id_));
  return LatencyInfoTracedValue::FromValue(std::move(record_data));
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
