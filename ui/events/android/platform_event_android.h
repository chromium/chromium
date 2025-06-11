// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_PLATFORM_EVENT_ANDROID_H_
#define UI_EVENTS_ANDROID_PLATFORM_EVENT_ANDROID_H_

#include <variant>

#include "ui/events/android/key_event_android.h"
#include "ui/events/events_export.h"

namespace ui {

// Concrete type for the `PlatformEvent` on Android.
class EVENTS_EXPORT PlatformEventAndroid {
 public:
  PlatformEventAndroid();

  explicit PlatformEventAndroid(const KeyEventAndroid& key_event);

  PlatformEventAndroid(const PlatformEventAndroid& other);
  PlatformEventAndroid& operator=(const PlatformEventAndroid& other);

  ~PlatformEventAndroid();

  bool IsKeyboardEvent() const;

  // Returns the keyboard event if the event is a keyboard event, otherwise
  // returns nullptr.
  const KeyEventAndroid* AsKeyboardEventAndroid() const;

 private:
  // TODO(crbug.com/417078839): Add `MotionEventAndroid` as a variant and
  // replace its direct usages with `PlatformEvent` as appropriate.
  std::variant<std::monostate, KeyEventAndroid> event_;
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_PLATFORM_EVENT_ANDROID_H_
