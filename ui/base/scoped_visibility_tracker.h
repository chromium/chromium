// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_SCOPED_VISIBILITY_TRACKER_H_
#define UI_BASE_SCOPED_VISIBILITY_TRACKER_H_

#include <memory>

#include "base/time/time.h"
#include "ui/base/ui_base_export.h"

namespace base {
class TickClock;
}  // namespace base

namespace ui {

// This class tracks the total time it is visible, based on receiving
// OnShown/OnHidden notifications, which are logically idempotent.
class UI_BASE_EXPORT ScopedVisibilityTracker {
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

  const base::TickClock* tick_clock_;

  base::TimeTicks last_time_shown_;
  base::TimeDelta foreground_duration_;
  bool currently_in_foreground_ = false;
};

}  // namespace ui

#endif  // UI_BASE_SCOPED_VISIBILITY_TRACKER_H_
