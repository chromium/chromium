// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "core/common/common.h"

#include <string>
#include <unordered_map>

namespace onnxruntime {
namespace profiling {

enum EventCategory {
  SESSION_EVENT = 0,
  NODE_EVENT,
  KERNEL_EVENT,
  API_EVENT,
  EVENT_CATEGORY_MAX
};

// Event descriptions for the above session events.
static constexpr const char* event_category_names_[EVENT_CATEGORY_MAX] = {
    "Session",
    "Node",
    "Kernel",
    "Api"};

// Timing record for all events.
struct EventRecord {
  EventRecord() = default;
  EventRecord(EventCategory category,
              int process_id,
              int thread_id,
              std::string&& event_name,
              long long time_stamp,
              long long duration,
              std::unordered_map<std::string, std::string>&& event_args)
      : cat(category),
        pid(process_id),
        tid(thread_id),
        name(std::move(event_name)),
        ts(time_stamp),
        dur(duration),
        args(std::move(event_args)) {}

  EventRecord(EventCategory category,
              int process_id,
              int thread_id,
              const std::string& event_name,
              long long time_stamp,
              long long duration,
              const std::unordered_map<std::string, std::string>& event_args)
      : cat(category),
        pid(process_id),
        tid(thread_id),
        name(event_name),
        ts(time_stamp),
        dur(duration),
        args(event_args) {}

  EventRecord(const EventRecord& other) = default;
  EventRecord(EventRecord&& other) noexcept = default;
  EventRecord& operator=(const EventRecord& other) = default;
  EventRecord& operator=(EventRecord&& other) = default;

  EventCategory cat = EventCategory::API_EVENT;
  int pid = -1;
  int tid = -1;
  std::string name{};
  long long ts = 0;
  long long dur = 0;
  std::unordered_map<std::string, std::string> args{};
};

using Events = std::vector<EventRecord>;

// Execution Provider Profiler
class EpProfiler {
 public:
  virtual ~EpProfiler() = default;
  virtual bool StartProfiling(TimePoint profiling_start_time) = 0;      // called when profiling starts
  virtual void EndProfiling(TimePoint start_time, Events& events) = 0;  // called when profiling ends, save all captures numbers to "events"
  virtual void Start(uint64_t){};                                       // called before op start, accept an id as argument to identify the op
  virtual void Stop(uint64_t){};                                        // called after op stop, accept an id as argument to identify the op
};

// Demangle C++ symbols
std::string demangle(const char* name);
std::string demangle(const std::string& name);

}  // namespace profiling
}  // namespace onnxruntime
