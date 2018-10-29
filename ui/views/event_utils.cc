// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/event_utils.h"

#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/views/metrics.h"

namespace views {

bool IsPossiblyUnintendedInteraction(const base::TimeTicks& initial_timestamp,
                                     const ui::Event& event) {
  return (event.IsMouseEvent() || event.IsTouchEvent()) &&
         event.time_stamp() <
             initial_timestamp +
                 base::TimeDelta::FromMilliseconds(GetDoubleClickInterval());
}

}  // namespace views
