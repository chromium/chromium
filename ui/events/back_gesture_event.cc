// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/back_gesture_event.h"

#include "base/time/time.h"

namespace ui {

BackGestureEvent::BackGestureEvent(float progress)
    : progress_(progress), time_(base::TimeTicks::Now()) {}

BackGestureEvent::BackGestureEvent(float progress, base::TimeTicks time)
    : progress_(progress), time_(time) {}

}  // namespace ui
