// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libgestures_glue/gesture_timer_provider.h"

#include <gestures/gestures.h>

#include "base/logging.h"
#include "base/timer/timer.h"

// libgestures requires that this be in the top level namespace.
struct GesturesTimer {
 public:
  GesturesTimer() {}
  ~GesturesTimer() {}

  void Set(stime_t delay, GesturesTimerCallback callback, void* callback_data) {
    callback_ = callback;
    callback_data_ = callback_data;
    timer_.Start(FROM_HERE,
                 base::Microseconds(delay * base::Time::kMicrosecondsPerSecond),
                 this, &GesturesTimer::OnTimerExpired);
  }

  void Cancel() { timer_.Stop(); }

 private:
  void OnTimerExpired() {
    // Run the callback and reschedule the next run if requested.
    stime_t next_delay = callback_(ui::StimeNow(), callback_data_);
    if (next_delay >= 0) {
      timer_.Start(
          FROM_HERE,
          base::Microseconds(next_delay * base::Time::kMicrosecondsPerSecond),
          this, &GesturesTimer::OnTimerExpired);
    }
  }

  GesturesTimerCallback callback_ = nullptr;
  void* callback_data_ = nullptr;
  base::OneShotTimer timer_;
};

namespace ui {

namespace {

GesturesTimer* GesturesTimerCreate(void* data) { return new GesturesTimer; }

void GesturesTimerSet(void* data,
                      GesturesTimer* timer,
                      stime_t delay,
                      GesturesTimerCallback callback,
                      void* callback_data) {
  timer->Set(delay, callback, callback_data);
}

void GesturesTimerCancel(void* data, GesturesTimer* timer) { timer->Cancel(); }

void GesturesTimerFree(void* data, GesturesTimer* timer) { delete timer; }

}  // namespace

stime_t StimeNow() {
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    PLOG(FATAL) << "clock_gettime";

  return StimeFromTimespec(&ts);
}

const GesturesTimerProvider kGestureTimerProvider = {
    GesturesTimerCreate, GesturesTimerSet, GesturesTimerCancel,
    GesturesTimerFree};

}  // namespace ui
