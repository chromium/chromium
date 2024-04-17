// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was automatically generated with:
// ../../ui/gfx/x/gen_xproto.py \
//    ../../third_party/xcbproto/src \
//    gen/ui/gfx/x \
//    bigreq \
//    dri3 \
//    glx \
//    randr \
//    render \
//    screensaver \
//    shape \
//    shm \
//    sync \
//    xfixes \
//    xinput \
//    xkb \
//    xproto \
//    xtest

#ifndef UI_GFX_X_GENERATED_PROTOS_SYNC_H_
#define UI_GFX_X_GENERATED_PROTOS_SYNC_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/x/error.h"
#include "ui/gfx/x/ref_counted_fd.h"
#include "ui/gfx/x/xproto_types.h"
#include "xproto.h"

namespace x11 {

class Connection;

template <typename Reply>
struct Response;

template <typename Reply>
class Future;

class COMPONENT_EXPORT(X11) Sync {
 public:
  static constexpr unsigned major_version = 3;
  static constexpr unsigned minor_version = 1;

  Sync(Connection* connection, const x11::QueryExtensionReply& info);

  uint8_t present() const { return info_.present; }
  uint8_t major_opcode() const { return info_.major_opcode; }
  uint8_t first_event() const { return info_.first_event; }
  uint8_t first_error() const { return info_.first_error; }

  Connection* connection() const { return connection_; }

  enum class Alarm : uint32_t {};

  enum class Alarmstate : int {
    Active = 0,
    Inactive = 1,
    Destroyed = 2,
  };

  enum class Counter : uint32_t {};

  enum class Fence : uint32_t {};

  enum class Testtype : int {
    PositiveTransition = 0,
    NegativeTransition = 1,
    PositiveComparison = 2,
    NegativeComparison = 3,
  };

  enum class Valuetype : int {
    Absolute = 0,
    Relative = 1,
  };

  enum class ChangeAlarmAttribute : int {
    Counter = 1 << 0,
    ValueType = 1 << 1,
    Value = 1 << 2,
    TestType = 1 << 3,
    Delta = 1 << 4,
    Events = 1 << 5,
  };

  struct Int64 {
    bool operator==(const Int64& other) const {
      return hi == other.hi && lo == other.lo;
    }

    int32_t hi{};
    uint32_t lo{};
  };

  struct SystemCounter {
    bool operator==(const SystemCounter& other) const {
      return counter == other.counter && resolution == other.resolution &&
             name == other.name;
    }

    Counter counter{};
    Int64 resolution{};
    std::string name{};
  };

  struct Trigger {
    bool operator==(const Trigger& other) const {
      return counter == other.counter && wait_type == other.wait_type &&
             wait_value == other.wait_value && test_type == other.test_type;
    }

    Counter counter{};
    Valuetype wait_type{};
    Int64 wait_value{};
    Testtype test_type{};
  };

  struct WaitCondition {
    bool operator==(const WaitCondition& other) const {
      return trigger == other.trigger &&
             event_threshold == other.event_threshold;
    }

    Trigger trigger{};
    Int64 event_threshold{};
  };

  struct CounterError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_counter{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct AlarmError : public x11::Error {
    uint16_t sequence{};
    uint32_t bad_alarm{};
    uint16_t minor_opcode{};
    uint8_t major_opcode{};

    std::string ToString() const override;
  };

  struct CounterNotifyEvent {
    static constexpr uint8_t type_id = 8;
    static constexpr uint8_t opcode = 0;
    uint8_t kind{};
    uint16_t sequence{};
    Counter counter{};
    Int64 wait_value{};
    Int64 counter_value{};
    Time timestamp{};
    uint16_t count{};
    uint8_t destroyed{};
  };

  struct AlarmNotifyEvent {
    static constexpr uint8_t type_id = 9;
    static constexpr uint8_t opcode = 1;
    uint8_t kind{};
    uint16_t sequence{};
    Alarm alarm{};
    Int64 counter_value{};
    Int64 alarm_value{};
    Time timestamp{};
    Alarmstate state{};
  };

  struct InitializeRequest {
    uint8_t desired_major_version{};
    uint8_t desired_minor_version{};
  };

  struct InitializeReply {
    uint16_t sequence{};
    uint8_t major_version{};
    uint8_t minor_version{};
  };

  using InitializeResponse = Response<InitializeReply>;

  Future<InitializeReply> Initialize(const InitializeRequest& request);

  Future<InitializeReply> Initialize(const uint8_t& desired_major_version = {},
                                     const uint8_t& desired_minor_version = {});

  struct ListSystemCountersRequest {};

  struct ListSystemCountersReply {
    uint16_t sequence{};
    std::vector<SystemCounter> counters{};
  };

  using ListSystemCountersResponse = Response<ListSystemCountersReply>;

  Future<ListSystemCountersReply> ListSystemCounters(
      const ListSystemCountersRequest& request);

  Future<ListSystemCountersReply> ListSystemCounters();

  struct CreateCounterRequest {
    Counter id{};
    Int64 initial_value{};
  };

  using CreateCounterResponse = Response<void>;

  Future<void> CreateCounter(const CreateCounterRequest& request);

  Future<void> CreateCounter(const Counter& id = {},
                             const Int64& initial_value = {{}, {}});

  struct DestroyCounterRequest {
    Counter counter{};
  };

  using DestroyCounterResponse = Response<void>;

  Future<void> DestroyCounter(const DestroyCounterRequest& request);

  Future<void> DestroyCounter(const Counter& counter = {});

  struct QueryCounterRequest {
    Counter counter{};
  };

  struct QueryCounterReply {
    uint16_t sequence{};
    Int64 counter_value{};
  };

  using QueryCounterResponse = Response<QueryCounterReply>;

  Future<QueryCounterReply> QueryCounter(const QueryCounterRequest& request);

  Future<QueryCounterReply> QueryCounter(const Counter& counter = {});

  struct AwaitRequest {
    std::vector<WaitCondition> wait_list{};
  };

  using AwaitResponse = Response<void>;

  Future<void> Await(const AwaitRequest& request);

  Future<void> Await(const std::vector<WaitCondition>& wait_list = {});

  struct ChangeCounterRequest {
    Counter counter{};
    Int64 amount{};
  };

  using ChangeCounterResponse = Response<void>;

  Future<void> ChangeCounter(const ChangeCounterRequest& request);

  Future<void> ChangeCounter(const Counter& counter = {},
                             const Int64& amount = {{}, {}});

  struct SetCounterRequest {
    Counter counter{};
    Int64 value{};
  };

  using SetCounterResponse = Response<void>;

  Future<void> SetCounter(const SetCounterRequest& request);

  Future<void> SetCounter(const Counter& counter = {},
                          const Int64& value = {{}, {}});

  struct CreateAlarmRequest {
    Alarm id{};
    std::optional<Counter> counter{};
    std::optional<Valuetype> valueType{};
    std::optional<Int64> value{};
    std::optional<Testtype> testType{};
    std::optional<Int64> delta{};
    std::optional<uint32_t> events{};
  };

  using CreateAlarmResponse = Response<void>;

  Future<void> CreateAlarm(const CreateAlarmRequest& request);

  Future<void> CreateAlarm(
      const Alarm& id = {},
      const std::optional<Counter>& counter = std::nullopt,
      const std::optional<Valuetype>& valueType = std::nullopt,
      const std::optional<Int64>& value = std::nullopt,
      const std::optional<Testtype>& testType = std::nullopt,
      const std::optional<Int64>& delta = std::nullopt,
      const std::optional<uint32_t>& events = std::nullopt);

  struct ChangeAlarmRequest {
    Alarm id{};
    std::optional<Counter> counter{};
    std::optional<Valuetype> valueType{};
    std::optional<Int64> value{};
    std::optional<Testtype> testType{};
    std::optional<Int64> delta{};
    std::optional<uint32_t> events{};
  };

  using ChangeAlarmResponse = Response<void>;

  Future<void> ChangeAlarm(const ChangeAlarmRequest& request);

  Future<void> ChangeAlarm(
      const Alarm& id = {},
      const std::optional<Counter>& counter = std::nullopt,
      const std::optional<Valuetype>& valueType = std::nullopt,
      const std::optional<Int64>& value = std::nullopt,
      const std::optional<Testtype>& testType = std::nullopt,
      const std::optional<Int64>& delta = std::nullopt,
      const std::optional<uint32_t>& events = std::nullopt);

  struct DestroyAlarmRequest {
    Alarm alarm{};
  };

  using DestroyAlarmResponse = Response<void>;

  Future<void> DestroyAlarm(const DestroyAlarmRequest& request);

  Future<void> DestroyAlarm(const Alarm& alarm = {});

  struct QueryAlarmRequest {
    Alarm alarm{};
  };

  struct QueryAlarmReply {
    uint16_t sequence{};
    Trigger trigger{};
    Int64 delta{};
    uint8_t events{};
    Alarmstate state{};
  };

  using QueryAlarmResponse = Response<QueryAlarmReply>;

  Future<QueryAlarmReply> QueryAlarm(const QueryAlarmRequest& request);

  Future<QueryAlarmReply> QueryAlarm(const Alarm& alarm = {});

  struct SetPriorityRequest {
    uint32_t id{};
    int32_t priority{};
  };

  using SetPriorityResponse = Response<void>;

  Future<void> SetPriority(const SetPriorityRequest& request);

  Future<void> SetPriority(const uint32_t& id = {},
                           const int32_t& priority = {});

  struct GetPriorityRequest {
    uint32_t id{};
  };

  struct GetPriorityReply {
    uint16_t sequence{};
    int32_t priority{};
  };

  using GetPriorityResponse = Response<GetPriorityReply>;

  Future<GetPriorityReply> GetPriority(const GetPriorityRequest& request);

  Future<GetPriorityReply> GetPriority(const uint32_t& id = {});

  struct CreateFenceRequest {
    Drawable drawable{};
    Fence fence{};
    uint8_t initially_triggered{};
  };

  using CreateFenceResponse = Response<void>;

  Future<void> CreateFence(const CreateFenceRequest& request);

  Future<void> CreateFence(const Drawable& drawable = {},
                           const Fence& fence = {},
                           const uint8_t& initially_triggered = {});

  struct TriggerFenceRequest {
    Fence fence{};
  };

  using TriggerFenceResponse = Response<void>;

  Future<void> TriggerFence(const TriggerFenceRequest& request);

  Future<void> TriggerFence(const Fence& fence = {});

  struct ResetFenceRequest {
    Fence fence{};
  };

  using ResetFenceResponse = Response<void>;

  Future<void> ResetFence(const ResetFenceRequest& request);

  Future<void> ResetFence(const Fence& fence = {});

  struct DestroyFenceRequest {
    Fence fence{};
  };

  using DestroyFenceResponse = Response<void>;

  Future<void> DestroyFence(const DestroyFenceRequest& request);

  Future<void> DestroyFence(const Fence& fence = {});

  struct QueryFenceRequest {
    Fence fence{};
  };

  struct QueryFenceReply {
    uint16_t sequence{};
    uint8_t triggered{};
  };

  using QueryFenceResponse = Response<QueryFenceReply>;

  Future<QueryFenceReply> QueryFence(const QueryFenceRequest& request);

  Future<QueryFenceReply> QueryFence(const Fence& fence = {});

  struct AwaitFenceRequest {
    std::vector<Fence> fence_list{};
  };

  using AwaitFenceResponse = Response<void>;

  Future<void> AwaitFence(const AwaitFenceRequest& request);

  Future<void> AwaitFence(const std::vector<Fence>& fence_list = {});

 private:
  Connection* const connection_;
  x11::QueryExtensionReply info_{};
};

}  // namespace x11

inline constexpr x11::Sync::Alarmstate operator|(x11::Sync::Alarmstate l,
                                                 x11::Sync::Alarmstate r) {
  using T = std::underlying_type_t<x11::Sync::Alarmstate>;
  return static_cast<x11::Sync::Alarmstate>(static_cast<T>(l) |
                                            static_cast<T>(r));
}

inline constexpr x11::Sync::Alarmstate operator&(x11::Sync::Alarmstate l,
                                                 x11::Sync::Alarmstate r) {
  using T = std::underlying_type_t<x11::Sync::Alarmstate>;
  return static_cast<x11::Sync::Alarmstate>(static_cast<T>(l) &
                                            static_cast<T>(r));
}

inline constexpr x11::Sync::Testtype operator|(x11::Sync::Testtype l,
                                               x11::Sync::Testtype r) {
  using T = std::underlying_type_t<x11::Sync::Testtype>;
  return static_cast<x11::Sync::Testtype>(static_cast<T>(l) |
                                          static_cast<T>(r));
}

inline constexpr x11::Sync::Testtype operator&(x11::Sync::Testtype l,
                                               x11::Sync::Testtype r) {
  using T = std::underlying_type_t<x11::Sync::Testtype>;
  return static_cast<x11::Sync::Testtype>(static_cast<T>(l) &
                                          static_cast<T>(r));
}

inline constexpr x11::Sync::Valuetype operator|(x11::Sync::Valuetype l,
                                                x11::Sync::Valuetype r) {
  using T = std::underlying_type_t<x11::Sync::Valuetype>;
  return static_cast<x11::Sync::Valuetype>(static_cast<T>(l) |
                                           static_cast<T>(r));
}

inline constexpr x11::Sync::Valuetype operator&(x11::Sync::Valuetype l,
                                                x11::Sync::Valuetype r) {
  using T = std::underlying_type_t<x11::Sync::Valuetype>;
  return static_cast<x11::Sync::Valuetype>(static_cast<T>(l) &
                                           static_cast<T>(r));
}

inline constexpr x11::Sync::ChangeAlarmAttribute operator|(
    x11::Sync::ChangeAlarmAttribute l,
    x11::Sync::ChangeAlarmAttribute r) {
  using T = std::underlying_type_t<x11::Sync::ChangeAlarmAttribute>;
  return static_cast<x11::Sync::ChangeAlarmAttribute>(static_cast<T>(l) |
                                                      static_cast<T>(r));
}

inline constexpr x11::Sync::ChangeAlarmAttribute operator&(
    x11::Sync::ChangeAlarmAttribute l,
    x11::Sync::ChangeAlarmAttribute r) {
  using T = std::underlying_type_t<x11::Sync::ChangeAlarmAttribute>;
  return static_cast<x11::Sync::ChangeAlarmAttribute>(static_cast<T>(l) &
                                                      static_cast<T>(r));
}

#endif  // UI_GFX_X_GENERATED_PROTOS_SYNC_H_
