// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/keyboard/event_auto_repeat_handler.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/events/base_event_utils.h"

namespace ui {

namespace {

constexpr int kRepeatDelayMs = 500;
constexpr int kRepeatIntervalMs = 50;

}  // namespace

EventAutoRepeatHandler::EventAutoRepeatHandler(Delegate* delegate)
    : repeat_delay_(base::TimeDelta::FromMilliseconds(kRepeatDelayMs)),
      repeat_interval_(base::TimeDelta::FromMilliseconds(kRepeatIntervalMs)),
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
                                             bool down,
                                             bool suppress_auto_repeat,
                                             int device_id) {
  if (!auto_repeat_enabled_ || suppress_auto_repeat)
    StopKeyRepeat();
  else if (key != repeat_key_ && down)
    StartKeyRepeat(key, device_id);
  else if (key == repeat_key_ && !down)
    StopKeyRepeat();
}

void EventAutoRepeatHandler::StartKeyRepeat(unsigned int key, int device_id) {
  repeat_key_ = key;
  repeat_device_id_ = device_id;
  repeat_sequence_++;

  ScheduleKeyRepeat(repeat_delay_);
}

void EventAutoRepeatHandler::StopKeyRepeat() {
  repeat_key_ = kInvalidKey;
  repeat_sequence_++;
}

void EventAutoRepeatHandler::ScheduleKeyRepeat(const base::TimeDelta& delay) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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

  delegate_->DispatchKey(repeat_key_, true /* down */, true /* repeat */,
                         EventTimeForNow(), repeat_device_id_);

  ScheduleKeyRepeat(repeat_interval_);
}

}  // namespace ui
