// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/keyboard/event_auto_repeat_handler.h"

#include "base/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"

namespace ui {

namespace {

constexpr int kRepeatDelayMs = 500;
constexpr int kRepeatIntervalMs = 50;

}  // namespace

EventAutoRepeatHandler::EventAutoRepeatHandler(Delegate* delegate)
    : repeat_delay_(base::Milliseconds(kRepeatDelayMs)),
      repeat_interval_(base::Milliseconds(kRepeatIntervalMs)),
      delegate_(delegate) {
  DCHECK(delegate_);
}

EventAutoRepeatHandler::~EventAutoRepeatHandler() {}

bool EventAutoRepeatHandler::IsAutoRepeatEnabled() {
  return auto_repeat_enabled_;
}

void EventAutoRepeatHandler::SetAutoRepeatEnabled(bool enabled) {
  auto_repeat_enabled_ = enabled;
}

void EventAutoRepeatHandler::SetAutoRepeatRate(
    const base::TimeDelta& delay,
    const base::TimeDelta& interval) {
  repeat_delay_ = delay;
  repeat_interval_ = interval;
}

void EventAutoRepeatHandler::GetAutoRepeatRate(base::TimeDelta* delay,
                                               base::TimeDelta* interval) {
  *delay = repeat_delay_;
  *interval = repeat_interval_;
}

void EventAutoRepeatHandler::UpdateKeyRepeat(unsigned int key,
                                             unsigned int scan_code,
                                             bool down,
                                             bool suppress_auto_repeat,
                                             int device_id) {
  if (!auto_repeat_enabled_ || suppress_auto_repeat)
    StopKeyRepeat();
  else if (key != repeat_key_ && down)
    StartKeyRepeat(key, scan_code, device_id);
  else if (key == repeat_key_ && !down)
    StopKeyRepeat();
}

void EventAutoRepeatHandler::StartKeyRepeat(unsigned int key,
                                            unsigned int scan_code,
                                            int device_id) {
  repeat_key_ = key;
  repeat_scan_code_ = scan_code;
  repeat_device_id_ = device_id;
  repeat_sequence_++;

  ScheduleKeyRepeat(repeat_delay_);
}

void EventAutoRepeatHandler::StopKeyRepeat() {
  repeat_key_ = kInvalidKey;
  repeat_scan_code_ = 0;
  repeat_sequence_++;
}

void EventAutoRepeatHandler::ScheduleKeyRepeat(const base::TimeDelta& delay) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EventAutoRepeatHandler::OnRepeatTimeout,
                     weak_ptr_factory_.GetWeakPtr(), repeat_sequence_),
      delay);
}

void EventAutoRepeatHandler::OnRepeatTimeout(unsigned int sequence) {
  if (repeat_sequence_ != sequence)
    return;

  base::OnceClosure commit =
      base::BindOnce(&EventAutoRepeatHandler::OnRepeatCommit,
                     weak_ptr_factory_.GetWeakPtr(), repeat_sequence_);
  delegate_->FlushInput(std::move(commit));
}

void EventAutoRepeatHandler::OnRepeatCommit(unsigned int sequence) {
  if (repeat_sequence_ != sequence)
    return;

  delegate_->DispatchKey(repeat_key_, repeat_scan_code_, true /* down */,
                         true /* repeat */, EventTimeForNow(),
                         repeat_device_id_, ui::EF_NONE);

  ScheduleKeyRepeat(repeat_interval_);
}

}  // namespace ui
