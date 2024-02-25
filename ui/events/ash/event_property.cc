// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "ui/events/event.h"

namespace ui {

namespace {
// Tag for the rewritten keyboard device id. The value is the byte array of
// and int value in native endian encoding.
// See also KeyboardDeviceIdEventRewriter.
constexpr char kPropertyKeyboardDeviceId[] = "keyboard_device_id";
}  // namespace

int GetKeyboardDeviceIdProperty(const Event& event) {
  if (auto* properties = event.properties()) {
    auto it = properties->find(kPropertyKeyboardDeviceId);
    if (it != properties->end()) {
      int result = 0;
      std::memcpy(&result, it->second.data(), it->second.size());
      return result;
    }
  }
  DCHECK(event.IsKeyEvent());
  return event.source_device_id();
}

void SetKeyboardDeviceIdProperty(Event* event, int device_id) {
  std::vector<std::uint8_t> buf;
  buf.resize(sizeof(device_id));
  std::memcpy(buf.data(), &device_id, buf.size());
  auto properties =
      event->properties() ? *event->properties() : Event::Properties();
  properties.emplace(kPropertyKeyboardDeviceId, std::move(buf));
  event->SetProperties(properties);
}

}  // namespace ui
