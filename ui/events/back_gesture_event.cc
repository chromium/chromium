// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/back_gesture_event.h"

namespace ui {

BackGestureEvent::BackGestureEvent(const gfx::PointF& location,
                                   float progress,
                                   BackGestureEventSwipeEdge edge)
    : location_(location), progress_(progress), edge_(edge) {}

}  // namespace ui
