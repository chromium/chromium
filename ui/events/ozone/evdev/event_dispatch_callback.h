// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_DISPATCH_CALLBACK_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_DISPATCH_CALLBACK_H_

#include "base/functional/callback.h"

namespace ui {

class Event;

typedef base::RepeatingCallback<void(Event*)> EventDispatchCallback;

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_DISPATCH_CALLBACK_H_
