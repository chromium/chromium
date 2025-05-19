// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/platform_event_android.h"

namespace ui {

PlatformEventAndroid::PlatformEventAndroid() : event_(std::monostate{}) {}

PlatformEventAndroid::PlatformEventAndroid(const KeyEventAndroid& key_event)
    : event_(key_event) {}

PlatformEventAndroid::PlatformEventAndroid(const PlatformEventAndroid& other) =
    default;
PlatformEventAndroid& PlatformEventAndroid::operator=(
    const PlatformEventAndroid& other) = default;

PlatformEventAndroid::~PlatformEventAndroid() {}

bool PlatformEventAndroid::IsKeyboardEvent() const {
  return std::holds_alternative<KeyEventAndroid>(event_);
}

const KeyEventAndroid* PlatformEventAndroid::AsKeyboardEventAndroid() const {
  if (!IsKeyboardEvent()) {
    return nullptr;
  }
  return &std::get<KeyEventAndroid>(event_);
}

}  // namespace ui
