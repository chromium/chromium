// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/gesture_types.h"

namespace ui {

bool GestureConsumer::RequiresDoubleTapGestureEvents() const {
  return false;
}

const std::string& GestureConsumer::GetName() const {
  static const std::string name("GestureConsumer");
  return name;
}

}  // namespace ui
