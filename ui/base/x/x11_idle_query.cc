// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_idle_query.h"

#include <stdint.h>

#include <algorithm>

#include "base/functional/bind.h"
#include "ui/gfx/x/future.h"

namespace ui {

namespace {

// Matches the 1-second resolution of ui::CalculateIdleTime().
// `idle_alarm_` fires on PositiveComparison (>= 1000 ms) and `reset_alarm_`
// fires on NegativeComparison (<= 999 ms), partitioning counter values into
// disjoint active and idle intervals.
constexpr base::TimeDelta kIdleThreshold = base::Seconds(1);

constexpr base::TimeDelta kFallbackPollInterval = base::Seconds(1);

// The XSync IDLETIME counter counts milliseconds.
base::TimeDelta SyncValueToTimeDelta(const x11::Sync::Int64& value) {
  const uint64_t hi = static_cast<uint32_t>(value.hi);
  return base::Milliseconds(static_cast<int64_t>((hi << 32) | value.lo));
}

x11::Sync::Int64 TimeDeltaToSyncValue(base::TimeDelta delta) {
  const uint64_t bits = static_cast<uint64_t>(delta.InMilliseconds());
  return {
      .hi = static_cast<int32_t>(bits >> 32),
      .lo = static_cast<uint32_t>(bits & 0xffffffffu),
  };
}

}  // namespace

IdleQueryX11::IdleQueryX11(x11::Connection* connection)
    : connection_(connection) {
  if (!connection_ || !connection_->Ready()) {
    mode_ = Mode::kUnavailable;
    return;
  }

  connection_->AddEventObserver(this);
  observing_events_ = true;

  auto future = connection_->sync().ListSystemCounters();
  connection_->Flush();
  future.OnResponse(base::BindOnce(&IdleQueryX11::OnListSystemCounters,
                                   weak_ptr_factory_.GetWeakPtr()));
}

IdleQueryX11::~IdleQueryX11() {
  if (!connection_) {
    return;
  }
  if (connection_->Ready()) {
    if (idle_alarm_ != x11::Sync::Alarm{}) {
      connection_->sync().DestroyAlarm(idle_alarm_).IgnoreError();
    }
    if (reset_alarm_ != x11::Sync::Alarm{}) {
      connection_->sync().DestroyAlarm(reset_alarm_).IgnoreError();
    }
    connection_->Flush();
  }
  if (observing_events_) {
    connection_->RemoveEventObserver(this);
  }
}

base::TimeDelta IdleQueryX11::IdleTime() {
  switch (mode_) {
    case Mode::kInitializing:
    case Mode::kUnavailable:
      return base::TimeDelta();

    case Mode::kSyncAlarms:
      if (!idle_start_ticks_.has_value()) {
        return base::TimeDelta();
      }
      return base::TimeTicks::Now() - *idle_start_ticks_;

    case Mode::kFallbackScreenSaver: {
      const base::TimeTicks now = base::TimeTicks::Now();
      if (!query_in_flight_ &&
          (last_query_ticks_.is_null() ||
           now - last_query_ticks_ >= kFallbackPollInterval)) {
        RequestFallbackIdleTime();
      }
      return cached_idle_time_;
    }
  }
}

void IdleQueryX11::OnEvent(const x11::Event& event) {
  if (mode_ != Mode::kSyncAlarms) {
    return;
  }
  const auto* notify = event.As<x11::Sync::AlarmNotifyEvent>();
  if (!notify || notify->state == x11::Sync::Alarmstate::Destroyed) {
    return;
  }
  if (notify->alarm == idle_alarm_) {
    const bool was_idle = idle_start_ticks_.has_value();
    const base::TimeDelta idle_time =
        std::max(kIdleThreshold, SyncValueToTimeDelta(notify->counter_value));
    idle_start_ticks_ = base::TimeTicks::Now() - idle_time;
    if (!was_idle) {
      ArmResetAlarm();
    }
  } else if (notify->alarm == reset_alarm_) {
    if (idle_start_ticks_.has_value()) {
      idle_start_ticks_.reset();
      ArmIdleAlarm();
    }
  }
}

void IdleQueryX11::OnListSystemCounters(
    x11::Sync::ListSystemCountersResponse response) {
  if (response) {
    for (const auto& counter : response->counters) {
      if (counter.name == "IDLETIME") {
        idle_counter_ = counter.counter;
        break;
      }
    }
  }

  if (idle_counter_ != x11::Sync::Counter{}) {
    mode_ = Mode::kSyncAlarms;
    idle_alarm_ = connection_->GenerateId<x11::Sync::Alarm>();
    reset_alarm_ = connection_->GenerateId<x11::Sync::Alarm>();

    // PositiveComparison triggers immediately if IDLETIME is already >=
    // kIdleThreshold, and `delta = 0` transitions the alarm to Inactive once
    // it fires so no further events are sent while idle.
    connection_->sync()
        .CreateAlarm({
            .id = idle_alarm_,
            .counter = idle_counter_,
            .valueType = x11::Sync::Valuetype::Absolute,
            .value = TimeDeltaToSyncValue(kIdleThreshold),
            .testType = x11::Sync::Testtype::PositiveComparison,
            .delta = TimeDeltaToSyncValue(base::TimeDelta()),
            .events = 1,
        })
        .IgnoreError();
    connection_->sync()
        .CreateAlarm({
            .id = reset_alarm_,
            .counter = idle_counter_,
            .valueType = x11::Sync::Valuetype::Absolute,
            .value =
                TimeDeltaToSyncValue(kIdleThreshold - base::Milliseconds(1)),
            .testType = x11::Sync::Testtype::NegativeComparison,
            .delta = TimeDeltaToSyncValue(base::TimeDelta()),
            .events = 1,
        })
        .IgnoreError();
    connection_->Flush();
    return;
  }

  if (connection_->screensaver().present()) {
    mode_ = Mode::kFallbackScreenSaver;
    RequestFallbackIdleTime();
  } else {
    mode_ = Mode::kUnavailable;
  }
}

void IdleQueryX11::ArmIdleAlarm() {
  connection_->sync()
      .ChangeAlarm({
          .id = idle_alarm_,
          .counter = idle_counter_,
          .valueType = x11::Sync::Valuetype::Absolute,
          .value = TimeDeltaToSyncValue(kIdleThreshold),
          .testType = x11::Sync::Testtype::PositiveComparison,
          .delta = TimeDeltaToSyncValue(base::TimeDelta()),
          .events = 1,
      })
      .IgnoreError();
  connection_->Flush();
}

void IdleQueryX11::ArmResetAlarm() {
  connection_->sync()
      .ChangeAlarm({
          .id = reset_alarm_,
          .counter = idle_counter_,
          .valueType = x11::Sync::Valuetype::Absolute,
          .value = TimeDeltaToSyncValue(kIdleThreshold - base::Milliseconds(1)),
          .testType = x11::Sync::Testtype::NegativeComparison,
          .delta = TimeDeltaToSyncValue(base::TimeDelta()),
          .events = 1,
      })
      .IgnoreError();
  connection_->Flush();
}

void IdleQueryX11::RequestFallbackIdleTime() {
  if (!connection_->Ready()) {
    mode_ = Mode::kUnavailable;
    return;
  }
  query_in_flight_ = true;
  last_query_ticks_ = base::TimeTicks::Now();
  auto future =
      connection_->screensaver().QueryInfo(connection_->default_root());
  connection_->Flush();
  future.OnResponse(base::BindOnce(&IdleQueryX11::OnQueryInfoResponse,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void IdleQueryX11::OnQueryInfoResponse(
    x11::ScreenSaver::QueryInfoResponse response) {
  query_in_flight_ = false;
  if (!response) {
    mode_ = Mode::kUnavailable;
    cached_idle_time_ = base::TimeDelta();
    return;
  }
  cached_idle_time_ = base::Milliseconds(response->ms_since_user_input);
}

}  // namespace ui
