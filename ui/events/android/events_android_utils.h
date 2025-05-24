// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_EVENTS_ANDROID_UTILS_H_
#define UI_EVENTS_ANDROID_EVENTS_ANDROID_UTILS_H_

#include "ui/events/event.h"

namespace ui {

// Creates a PlatformEvent from the given event.
EVENTS_EXPORT PlatformEvent NativeEventFromEvent(Event& event);

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_EVENTS_ANDROID_UTILS_H_
