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

#include "sync.h"

#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_internal.h"

namespace x11 {

Sync::Sync(Connection* connection, const x11::QueryExtensionReply& info)
    : connection_(connection), info_(info) {}

std::string Sync::CounterError::ToString() const {
  std::stringstream ss_;
  ss_ << "Sync::CounterError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_counter = " << static_cast<uint64_t>(bad_counter) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Sync::CounterError>(Sync::CounterError* error_,
                                   ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_counter = (*error_).bad_counter;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_counter
  Read(&bad_counter, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  CHECK_LE(buf.offset, 32ul);
}

std::string Sync::AlarmError::ToString() const {
  std::stringstream ss_;
  ss_ << "Sync::AlarmError{";
  ss_ << ".sequence = " << static_cast<uint64_t>(sequence) << ", ";
  ss_ << ".bad_alarm = " << static_cast<uint64_t>(bad_alarm) << ", ";
  ss_ << ".minor_opcode = " << static_cast<uint64_t>(minor_opcode) << ", ";
  ss_ << ".major_opcode = " << static_cast<uint64_t>(major_opcode);
  ss_ << "}";
  return ss_.str();
}

template <>
void ReadError<Sync::AlarmError>(Sync::AlarmError* error_, ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& sequence = (*error_).sequence;
  auto& bad_alarm = (*error_).bad_alarm;
  auto& minor_opcode = (*error_).minor_opcode;
  auto& major_opcode = (*error_).major_opcode;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // error_code
  uint8_t error_code;
  Read(&error_code, &buf);

  // sequence
  Read(&sequence, &buf);

  // bad_alarm
  Read(&bad_alarm, &buf);

  // minor_opcode
  Read(&minor_opcode, &buf);

  // major_opcode
  Read(&major_opcode, &buf);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Sync::CounterNotifyEvent>(Sync::CounterNotifyEvent* event_,
                                         ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& kind = (*event_).kind;
  auto& sequence = (*event_).sequence;
  auto& counter = (*event_).counter;
  auto& wait_value = (*event_).wait_value;
  auto& counter_value = (*event_).counter_value;
  auto& timestamp = (*event_).timestamp;
  auto& count = (*event_).count;
  auto& destroyed = (*event_).destroyed;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // kind
  Read(&kind, &buf);

  // sequence
  Read(&sequence, &buf);

  // counter
  Read(&counter, &buf);

  // wait_value
  {
    auto& hi = wait_value.hi;
    auto& lo = wait_value.lo;

    // hi
    Read(&hi, &buf);

    // lo
    Read(&lo, &buf);
  }

  // counter_value
  {
    auto& hi = counter_value.hi;
    auto& lo = counter_value.lo;

    // hi
    Read(&hi, &buf);

    // lo
    Read(&lo, &buf);
  }

  // timestamp
  Read(&timestamp, &buf);

  // count
  Read(&count, &buf);

  // destroyed
  Read(&destroyed, &buf);

  // pad0
  Pad(&buf, 1);

  CHECK_LE(buf.offset, 32ul);
}

template <>
COMPONENT_EXPORT(X11)
void ReadEvent<Sync::AlarmNotifyEvent>(Sync::AlarmNotifyEvent* event_,
                                       ReadBuffer* buffer) {
  auto& buf = *buffer;

  auto& kind = (*event_).kind;
  auto& sequence = (*event_).sequence;
  auto& alarm = (*event_).alarm;
  auto& counter_value = (*event_).counter_value;
  auto& alarm_value = (*event_).alarm_value;
  auto& timestamp = (*event_).timestamp;
  auto& state = (*event_).state;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // kind
  Read(&kind, &buf);

  // sequence
  Read(&sequence, &buf);

  // alarm
  Read(&alarm, &buf);

  // counter_value
  {
    auto& hi = counter_value.hi;
    auto& lo = counter_value.lo;

    // hi
    Read(&hi, &buf);

    // lo
    Read(&lo, &buf);
  }

  // alarm_value
  {
    auto& hi = alarm_value.hi;
    auto& lo = alarm_value.lo;

    // hi
    Read(&hi, &buf);

    // lo
    Read(&lo, &buf);
  }

  // timestamp
  Read(&timestamp, &buf);

  // state
  uint8_t tmp0;
  Read(&tmp0, &buf);
  state = static_cast<Sync::Alarmstate>(tmp0);

  // pad0
  Pad(&buf, 3);

  CHECK_LE(buf.offset, 32ul);
}

Future<Sync::InitializeReply> Sync::Initialize(
    const Sync::InitializeRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& desired_major_version = request.desired_major_version;
  auto& desired_minor_version = request.desired_minor_version;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 0;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // desired_major_version
  buf.Write(&desired_major_version);

  // desired_minor_version
  buf.Write(&desired_minor_version);

  Align(&buf, 4);

  return connection_->SendRequest<Sync::InitializeReply>(
      &buf, "Sync::Initialize", false);
}

Future<Sync::InitializeReply> Sync::Initialize(
    const uint8_t& desired_major_version,
    const uint8_t& desired_minor_version) {
  return Sync::Initialize(
      Sync::InitializeRequest{desired_major_version, desired_minor_version});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Sync::InitializeReply> detail::ReadReply<Sync::InitializeReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Sync::InitializeReply>();

  auto& sequence = (*reply).sequence;
  auto& major_version = (*reply).major_version;
  auto& minor_version = (*reply).minor_version;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // major_version
  Read(&major_version, &buf);

  // minor_version
  Read(&minor_version, &buf);

  // pad1
  Pad(&buf, 22);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<Sync::ListSystemCountersReply> Sync::ListSystemCounters(
    const Sync::ListSystemCountersRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 1;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  Align(&buf, 4);

  return connection_->SendRequest<Sync::ListSystemCountersReply>(
      &buf, "Sync::ListSystemCounters", false);
}

Future<Sync::ListSystemCountersReply> Sync::ListSystemCounters() {
  return Sync::ListSystemCounters(Sync::ListSystemCountersRequest{});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Sync::ListSystemCountersReply> detail::ReadReply<
    Sync::ListSystemCountersReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Sync::ListSystemCountersReply>();

  auto& sequence = (*reply).sequence;
  uint32_t counters_len{};
  auto& counters = (*reply).counters;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // counters_len
  Read(&counters_len, &buf);

  // pad1
  Pad(&buf, 20);

  // counters
  counters.resize(counters_len);
  for (auto& counters_elem : counters) {
    // counters_elem
    {
      auto& counter = counters_elem.counter;
      auto& resolution = counters_elem.resolution;
      uint16_t name_len{};
      auto& name = counters_elem.name;

      // counter
      Read(&counter, &buf);

      // resolution
      {
        auto& hi = resolution.hi;
        auto& lo = resolution.lo;

        // hi
        Read(&hi, &buf);

        // lo
        Read(&lo, &buf);
      }

      // name_len
      Read(&name_len, &buf);

      // name
      name.resize(name_len);
      for (auto& name_elem : name) {
        // name_elem
        Read(&name_elem, &buf);
      }

      // pad0
      Align(&buf, 4);
    }
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Sync::CreateCounter(const Sync::CreateCounterRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& id = request.id;
  auto& initial_value = request.initial_value;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 2;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // id
  buf.Write(&id);

  // initial_value
  {
    auto& hi = initial_value.hi;
    auto& lo = initial_value.lo;

    // hi
    buf.Write(&hi);

    // lo
    buf.Write(&lo);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Sync::CreateCounter", false);
}

Future<void> Sync::CreateCounter(const Counter& id,
                                 const Int64& initial_value) {
  return Sync::CreateCounter(Sync::CreateCounterRequest{id, initial_value});
}

Future<void> Sync::DestroyCounter(const Sync::DestroyCounterRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& counter = request.counter;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 6;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // counter
  buf.Write(&counter);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Sync::DestroyCounter", false);
}

Future<void> Sync::DestroyCounter(const Counter& counter) {
  return Sync::DestroyCounter(Sync::DestroyCounterRequest{counter});
}

Future<Sync::QueryCounterReply> Sync::QueryCounter(
    const Sync::QueryCounterRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& counter = request.counter;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 5;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // counter
  buf.Write(&counter);

  Align(&buf, 4);

  return connection_->SendRequest<Sync::QueryCounterReply>(
      &buf, "Sync::QueryCounter", false);
}

Future<Sync::QueryCounterReply> Sync::QueryCounter(const Counter& counter) {
  return Sync::QueryCounter(Sync::QueryCounterRequest{counter});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Sync::QueryCounterReply> detail::ReadReply<
    Sync::QueryCounterReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Sync::QueryCounterReply>();

  auto& sequence = (*reply).sequence;
  auto& counter_value = (*reply).counter_value;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // counter_value
  {
    auto& hi = counter_value.hi;
    auto& lo = counter_value.lo;

    // hi
    Read(&hi, &buf);

    // lo
    Read(&lo, &buf);
  }

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Sync::Await(const Sync::AwaitRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& wait_list = request.wait_list;
  size_t wait_list_len = wait_list.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 7;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // wait_list
  CHECK_EQ(static_cast<size_t>(wait_list_len), wait_list.size());
  for (auto& wait_list_elem : wait_list) {
    // wait_list_elem
    {
      auto& trigger = wait_list_elem.trigger;
      auto& event_threshold = wait_list_elem.event_threshold;

      // trigger
      {
        auto& counter = trigger.counter;
        auto& wait_type = trigger.wait_type;
        auto& wait_value = trigger.wait_value;
        auto& test_type = trigger.test_type;

        // counter
        buf.Write(&counter);

        // wait_type
        uint32_t tmp1;
        tmp1 = static_cast<uint32_t>(wait_type);
        buf.Write(&tmp1);

        // wait_value
        {
          auto& hi = wait_value.hi;
          auto& lo = wait_value.lo;

          // hi
          buf.Write(&hi);

          // lo
          buf.Write(&lo);
        }

        // test_type
        uint32_t tmp2;
        tmp2 = static_cast<uint32_t>(test_type);
        buf.Write(&tmp2);
      }

      // event_threshold
      {
        auto& hi = event_threshold.hi;
        auto& lo = event_threshold.lo;

        // hi
        buf.Write(&hi);

        // lo
        buf.Write(&lo);
      }
    }
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Sync::Await", false);
}

Future<void> Sync::Await(const std::vector<WaitCondition>& wait_list) {
  return Sync::Await(Sync::AwaitRequest{wait_list});
}

Future<void> Sync::ChangeCounter(const Sync::ChangeCounterRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& counter = request.counter;
  auto& amount = request.amount;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 4;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // counter
  buf.Write(&counter);

  // amount
  {
    auto& hi = amount.hi;
    auto& lo = amount.lo;

    // hi
    buf.Write(&hi);

    // lo
    buf.Write(&lo);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Sync::ChangeCounter", false);
}

Future<void> Sync::ChangeCounter(const Counter& counter, const Int64& amount) {
  return Sync::ChangeCounter(Sync::ChangeCounterRequest{counter, amount});
}

Future<void> Sync::SetCounter(const Sync::SetCounterRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& counter = request.counter;
  auto& value = request.value;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 3;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // counter
  buf.Write(&counter);

  // value
  {
    auto& hi = value.hi;
    auto& lo = value.lo;

    // hi
    buf.Write(&hi);

    // lo
    buf.Write(&lo);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Sync::SetCounter", false);
}

Future<void> Sync::SetCounter(const Counter& counter, const Int64& value) {
  return Sync::SetCounter(Sync::SetCounterRequest{counter, value});
}

Future<void> Sync::CreateAlarm(const Sync::CreateAlarmRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& id = request.id;
  ChangeAlarmAttribute value_mask{};
  auto& value_list = request;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 8;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // id
  buf.Write(&id);

  // value_mask
  SwitchVar(ChangeAlarmAttribute::Counter, value_list.counter.has_value(), true,
            &value_mask);
  SwitchVar(ChangeAlarmAttribute::ValueType, value_list.valueType.has_value(),
            true, &value_mask);
  SwitchVar(ChangeAlarmAttribute::Value, value_list.value.has_value(), true,
            &value_mask);
  SwitchVar(ChangeAlarmAttribute::TestType, value_list.testType.has_value(),
            true, &value_mask);
  SwitchVar(ChangeAlarmAttribute::Delta, value_list.delta.has_value(), true,
            &value_mask);
  SwitchVar(ChangeAlarmAttribute::Events, value_list.events.has_value(), true,
            &value_mask);
  uint32_t tmp3;
  tmp3 = static_cast<uint32_t>(value_mask);
  buf.Write(&tmp3);

  // value_list
  auto value_list_expr = value_mask;
  if (CaseAnd(value_list_expr, ChangeAlarmAttribute::Counter)) {
    auto& counter = *value_list.counter;

    // counter
    buf.Write(&counter);
  }
  if (CaseAnd(value_list_expr, ChangeAlarmAttribute::ValueType)) {
    auto& valueType = *value_list.valueType;

    // valueType
    uint32_t tmp4;
    tmp4 = static_cast<uint32_t>(valueType);
    buf.Write(&tmp4);
  }
  if (CaseAnd(value_list_expr, ChangeAlarmAttribute::Value)) {
    auto& value = *value_list.value;

    // value
    {
      auto& hi = value.hi;
      auto& lo = value.lo;

      // hi
      buf.Write(&hi);

      // lo
      buf.Write(&lo);
    }
  }
  if (CaseAnd(value_list_expr, ChangeAlarmAttribute::TestType)) {
    auto& testType = *value_list.testType;

    // testType
    uint32_t tmp5;
    tmp5 = static_cast<uint32_t>(testType);
    buf.Write(&tmp5);
  }
  if (CaseAnd(value_list_expr, ChangeAlarmAttribute::Delta)) {
    auto& delta = *value_list.delta;

    // delta
    {
      auto& hi = delta.hi;
      auto& lo = delta.lo;

      // hi
      buf.Write(&hi);

      // lo
      buf.Write(&lo);
    }
  }
  if (CaseAnd(value_list_expr, ChangeAlarmAttribute::Events)) {
    auto& events = *value_list.events;

    // events
    buf.Write(&events);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Sync::CreateAlarm", false);
}

Future<void> Sync::CreateAlarm(const Alarm& id,
                               const std::optional<Counter>& counter,
                               const std::optional<Valuetype>& valueType,
                               const std::optional<Int64>& value,
                               const std::optional<Testtype>& testType,
                               const std::optional<Int64>& delta,
                               const std::optional<uint32_t>& events) {
  return Sync::CreateAlarm(Sync::CreateAlarmRequest{
      id, counter, valueType, value, testType, delta, events});
}

Future<void> Sync::ChangeAlarm(const Sync::ChangeAlarmRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& id = request.id;
  ChangeAlarmAttribute value_mask{};
  auto& value_list = request;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 9;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // id
  buf.Write(&id);

  // value_mask
  SwitchVar(ChangeAlarmAttribute::Counter, value_list.counter.has_value(), true,
            &value_mask);
  SwitchVar(ChangeAlarmAttribute::ValueType, value_list.valueType.has_value(),
            true, &value_mask);
  SwitchVar(ChangeAlarmAttribute::Value, value_list.value.has_value(), true,
            &value_mask);
  SwitchVar(ChangeAlarmAttribute::TestType, value_list.testType.has_value(),
            true, &value_mask);
  SwitchVar(ChangeAlarmAttribute::Delta, value_list.delta.has_value(), true,
            &value_mask);
  SwitchVar(ChangeAlarmAttribute::Events, value_list.events.has_value(), true,
            &value_mask);
  uint32_t tmp6;
  tmp6 = static_cast<uint32_t>(value_mask);
  buf.Write(&tmp6);

  // value_list
  auto value_list_expr = value_mask;
  if (CaseAnd(value_list_expr, ChangeAlarmAttribute::Counter)) {
    auto& counter = *value_list.counter;

    // counter
    buf.Write(&counter);
  }
  if (CaseAnd(value_list_expr, ChangeAlarmAttribute::ValueType)) {
    auto& valueType = *value_list.valueType;

    // valueType
    uint32_t tmp7;
    tmp7 = static_cast<uint32_t>(valueType);
    buf.Write(&tmp7);
  }
  if (CaseAnd(value_list_expr, ChangeAlarmAttribute::Value)) {
    auto& value = *value_list.value;

    // value
    {
      auto& hi = value.hi;
      auto& lo = value.lo;

      // hi
      buf.Write(&hi);

      // lo
      buf.Write(&lo);
    }
  }
  if (CaseAnd(value_list_expr, ChangeAlarmAttribute::TestType)) {
    auto& testType = *value_list.testType;

    // testType
    uint32_t tmp8;
    tmp8 = static_cast<uint32_t>(testType);
    buf.Write(&tmp8);
  }
  if (CaseAnd(value_list_expr, ChangeAlarmAttribute::Delta)) {
    auto& delta = *value_list.delta;

    // delta
    {
      auto& hi = delta.hi;
      auto& lo = delta.lo;

      // hi
      buf.Write(&hi);

      // lo
      buf.Write(&lo);
    }
  }
  if (CaseAnd(value_list_expr, ChangeAlarmAttribute::Events)) {
    auto& events = *value_list.events;

    // events
    buf.Write(&events);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Sync::ChangeAlarm", false);
}

Future<void> Sync::ChangeAlarm(const Alarm& id,
                               const std::optional<Counter>& counter,
                               const std::optional<Valuetype>& valueType,
                               const std::optional<Int64>& value,
                               const std::optional<Testtype>& testType,
                               const std::optional<Int64>& delta,
                               const std::optional<uint32_t>& events) {
  return Sync::ChangeAlarm(Sync::ChangeAlarmRequest{
      id, counter, valueType, value, testType, delta, events});
}

Future<void> Sync::DestroyAlarm(const Sync::DestroyAlarmRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& alarm = request.alarm;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 11;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // alarm
  buf.Write(&alarm);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Sync::DestroyAlarm", false);
}

Future<void> Sync::DestroyAlarm(const Alarm& alarm) {
  return Sync::DestroyAlarm(Sync::DestroyAlarmRequest{alarm});
}

Future<Sync::QueryAlarmReply> Sync::QueryAlarm(
    const Sync::QueryAlarmRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& alarm = request.alarm;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 10;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // alarm
  buf.Write(&alarm);

  Align(&buf, 4);

  return connection_->SendRequest<Sync::QueryAlarmReply>(
      &buf, "Sync::QueryAlarm", false);
}

Future<Sync::QueryAlarmReply> Sync::QueryAlarm(const Alarm& alarm) {
  return Sync::QueryAlarm(Sync::QueryAlarmRequest{alarm});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Sync::QueryAlarmReply> detail::ReadReply<Sync::QueryAlarmReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Sync::QueryAlarmReply>();

  auto& sequence = (*reply).sequence;
  auto& trigger = (*reply).trigger;
  auto& delta = (*reply).delta;
  auto& events = (*reply).events;
  auto& state = (*reply).state;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // trigger
  {
    auto& counter = trigger.counter;
    auto& wait_type = trigger.wait_type;
    auto& wait_value = trigger.wait_value;
    auto& test_type = trigger.test_type;

    // counter
    Read(&counter, &buf);

    // wait_type
    uint32_t tmp9;
    Read(&tmp9, &buf);
    wait_type = static_cast<Sync::Valuetype>(tmp9);

    // wait_value
    {
      auto& hi = wait_value.hi;
      auto& lo = wait_value.lo;

      // hi
      Read(&hi, &buf);

      // lo
      Read(&lo, &buf);
    }

    // test_type
    uint32_t tmp10;
    Read(&tmp10, &buf);
    test_type = static_cast<Sync::Testtype>(tmp10);
  }

  // delta
  {
    auto& hi = delta.hi;
    auto& lo = delta.lo;

    // hi
    Read(&hi, &buf);

    // lo
    Read(&lo, &buf);
  }

  // events
  Read(&events, &buf);

  // state
  uint8_t tmp11;
  Read(&tmp11, &buf);
  state = static_cast<Sync::Alarmstate>(tmp11);

  // pad1
  Pad(&buf, 2);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Sync::SetPriority(const Sync::SetPriorityRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& id = request.id;
  auto& priority = request.priority;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 12;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // id
  buf.Write(&id);

  // priority
  buf.Write(&priority);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Sync::SetPriority", false);
}

Future<void> Sync::SetPriority(const uint32_t& id, const int32_t& priority) {
  return Sync::SetPriority(Sync::SetPriorityRequest{id, priority});
}

Future<Sync::GetPriorityReply> Sync::GetPriority(
    const Sync::GetPriorityRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& id = request.id;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 13;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // id
  buf.Write(&id);

  Align(&buf, 4);

  return connection_->SendRequest<Sync::GetPriorityReply>(
      &buf, "Sync::GetPriority", false);
}

Future<Sync::GetPriorityReply> Sync::GetPriority(const uint32_t& id) {
  return Sync::GetPriority(Sync::GetPriorityRequest{id});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Sync::GetPriorityReply> detail::ReadReply<
    Sync::GetPriorityReply>(ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Sync::GetPriorityReply>();

  auto& sequence = (*reply).sequence;
  auto& priority = (*reply).priority;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // priority
  Read(&priority, &buf);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Sync::CreateFence(const Sync::CreateFenceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& drawable = request.drawable;
  auto& fence = request.fence;
  auto& initially_triggered = request.initially_triggered;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 14;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // drawable
  buf.Write(&drawable);

  // fence
  buf.Write(&fence);

  // initially_triggered
  buf.Write(&initially_triggered);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Sync::CreateFence", false);
}

Future<void> Sync::CreateFence(const Drawable& drawable,
                               const Fence& fence,
                               const uint8_t& initially_triggered) {
  return Sync::CreateFence(
      Sync::CreateFenceRequest{drawable, fence, initially_triggered});
}

Future<void> Sync::TriggerFence(const Sync::TriggerFenceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& fence = request.fence;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 15;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // fence
  buf.Write(&fence);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Sync::TriggerFence", false);
}

Future<void> Sync::TriggerFence(const Fence& fence) {
  return Sync::TriggerFence(Sync::TriggerFenceRequest{fence});
}

Future<void> Sync::ResetFence(const Sync::ResetFenceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& fence = request.fence;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 16;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // fence
  buf.Write(&fence);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Sync::ResetFence", false);
}

Future<void> Sync::ResetFence(const Fence& fence) {
  return Sync::ResetFence(Sync::ResetFenceRequest{fence});
}

Future<void> Sync::DestroyFence(const Sync::DestroyFenceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& fence = request.fence;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 17;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // fence
  buf.Write(&fence);

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Sync::DestroyFence", false);
}

Future<void> Sync::DestroyFence(const Fence& fence) {
  return Sync::DestroyFence(Sync::DestroyFenceRequest{fence});
}

Future<Sync::QueryFenceReply> Sync::QueryFence(
    const Sync::QueryFenceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& fence = request.fence;

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 18;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // fence
  buf.Write(&fence);

  Align(&buf, 4);

  return connection_->SendRequest<Sync::QueryFenceReply>(
      &buf, "Sync::QueryFence", false);
}

Future<Sync::QueryFenceReply> Sync::QueryFence(const Fence& fence) {
  return Sync::QueryFence(Sync::QueryFenceRequest{fence});
}

template <>
COMPONENT_EXPORT(X11)
std::unique_ptr<Sync::QueryFenceReply> detail::ReadReply<Sync::QueryFenceReply>(
    ReadBuffer* buffer) {
  auto& buf = *buffer;
  auto reply = std::make_unique<Sync::QueryFenceReply>();

  auto& sequence = (*reply).sequence;
  auto& triggered = (*reply).triggered;

  // response_type
  uint8_t response_type;
  Read(&response_type, &buf);

  // pad0
  Pad(&buf, 1);

  // sequence
  Read(&sequence, &buf);

  // length
  uint32_t length;
  Read(&length, &buf);

  // triggered
  Read(&triggered, &buf);

  // pad1
  Pad(&buf, 23);

  Align(&buf, 4);
  CHECK_EQ(buf.offset < 32 ? 0 : buf.offset - 32, 4 * length);

  return reply;
}

Future<void> Sync::AwaitFence(const Sync::AwaitFenceRequest& request) {
  if (!connection_->Ready() || !present())
    return {};

  WriteBuffer buf;

  auto& fence_list = request.fence_list;
  size_t fence_list_len = fence_list.size();

  // major_opcode
  uint8_t major_opcode = info_.major_opcode;
  buf.Write(&major_opcode);

  // minor_opcode
  uint8_t minor_opcode = 19;
  buf.Write(&minor_opcode);

  // length
  // Caller fills in length for writes.
  Pad(&buf, sizeof(uint16_t));

  // fence_list
  CHECK_EQ(static_cast<size_t>(fence_list_len), fence_list.size());
  for (auto& fence_list_elem : fence_list) {
    // fence_list_elem
    buf.Write(&fence_list_elem);
  }

  Align(&buf, 4);

  return connection_->SendRequest<void>(&buf, "Sync::AwaitFence", false);
}

Future<void> Sync::AwaitFence(const std::vector<Fence>& fence_list) {
  return Sync::AwaitFence(Sync::AwaitFenceRequest{fence_list});
}

}  // namespace x11
