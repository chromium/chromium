// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_SCOPED_VISIBILITY_TRACKER_H_
#define UI_BASE_SCOPED_VISIBILITY_TRACKER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

namespace base {
class TickClock;
}  // namespace base

namespace ui {

// This class tracks the total time it is visible, based on receiving
// OnShown/OnHidden notifications, which are logically idempotent.
class COMPONENT_EXPORT(UI_BASE) ScopedVisibilityTracker {
 public:
  // |tick_clock| must outlive this object.
  ScopedVisibilityTracker(const base::TickClock* tick_clock, bool is_shown);
  ~ScopedVisibilityTracker();

  void OnShown();
  void OnHidden();

  base::TimeDelta GetForegroundDuration() const;

  bool currently_in_foreground() const { return currently_in_foreground_; }

 private:
  void Update(bool in_foreground);

  raw_ptr<const base::TickClock> tick_clock_;

  base::TimeTicks last_time_shown_;
  base::TimeDelta foreground_duration_;
  bool currently_in_foreground_ = false;
};

}  // namespace ui

#endif  // UI_BASE_SCOPED_VISIBILITY_TRACKER_H_
