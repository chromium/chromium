// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_EVENT_FLAGS_ANDROID_H_
#define UI_EVENTS_ANDROID_EVENT_FLAGS_ANDROID_H_

#include "ui/events/event_constants.h"
#include "ui/events/events_export.h"

namespace ui {

// Converts android meta state to event flags.
EVENTS_EXPORT EventFlags EventFlagsFromAndroidMetaState(int meta_state);

// Converts event flags to android meta state.
EVENTS_EXPORT int AndroidMetaStateFromEventFlags(EventFlags event_flags);

// Converts android button state to event flags.
EVENTS_EXPORT EventFlags EventFlagsFromAndroidButtonState(int button_state);

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_EVENT_FLAGS_ANDROID_H_
