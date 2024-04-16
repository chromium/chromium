// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/base_event_utils.h"

#include "base/atomic_sequence_num.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_switches.h"

namespace ui {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
const int kSystemKeyModifierMask = EF_ALT_DOWN | EF_COMMAND_DOWN;
#elif BUILDFLAG(IS_APPLE)
// Alt modifier is used to input extended characters on Mac.
const int kSystemKeyModifierMask = EF_COMMAND_DOWN;
#else
const int kSystemKeyModifierMask = EF_ALT_DOWN;
#endif

}  // namespace

base::AtomicSequenceNumber g_next_event_id;

uint32_t GetNextTouchEventId() {
  // Set the first touch event ID to 1 because we set id to 0 for other types
  // of events.
  uint32_t id = g_next_event_id.GetNext();
  if (id == 0)
    id = g_next_event_id.GetNext();
  DCHECK_NE(0U, id);
  return id;
}

bool IsSystemKeyModifier(int flags) {
  // AltGr modifier is used to type alternative keys on certain keyboard layouts
  // so we don't consider keys with the AltGr modifier as a system key.
  return (kSystemKeyModifierMask & flags) != 0 &&
         (EF_ALTGR_DOWN & flags) == 0;
}

base::LazyInstance<const base::TickClock*>::Leaky g_tick_clock =
    LAZY_INSTANCE_INITIALIZER;

base::TimeTicks EventTimeForNow() {
  return g_tick_clock.Get() ? g_tick_clock.Get()->NowTicks()
                            : base::TimeTicks::Now();
}

void SetEventTickClockForTesting(const base::TickClock* tick_clock) {
  g_tick_clock.Get() = tick_clock;
}

double EventTimeStampToSeconds(base::TimeTicks time_stamp) {
  return (time_stamp - base::TimeTicks()).InSecondsF();
}

base::TimeTicks EventTimeStampFromSeconds(double time_stamp_seconds) {
  return base::TimeTicks() + base::Seconds(time_stamp_seconds);
}

bool IsValidTimebase(base::TimeTicks now, base::TimeTicks timestamp) {
  int64_t delta = (now - timestamp).InMilliseconds();
  return delta >= 0 && delta <= 60 * 1000;
}

void ValidateEventTimeClock(base::TimeTicks* timestamp) {
  // Some fraction of devices, across all platforms provide bogus event
  // timestamps. See https://crbug.com/650338#c1. Correct timestamps which are
  // clearly bogus.
  // TODO(crbug.com/41400553): Replace this with an approach that doesn't
  // require an extra read of the current time per event.
  base::TimeTicks now = EventTimeForNow();
  if (!IsValidTimebase(now, *timestamp))
    *timestamp = now;
}

}  // namespace ui
