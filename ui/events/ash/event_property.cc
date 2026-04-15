// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
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
      auto bytes = base::span(it->second);
      if (bytes.size() == sizeof(int32_t)) {
        return base::I32FromNativeEndian(bytes.first<sizeof(int32_t)>());
      }
      DCHECK_EQ(bytes.size(), sizeof(int32_t));
    }
  }
  DCHECK(event.IsKeyEvent());
  return event.source_device_id();
}

void SetKeyboardDeviceIdProperty(Event* event, int device_id) {
  auto bytes = base::I32ToNativeEndian(device_id);
  std::vector<std::uint8_t> buf(bytes.begin(), bytes.end());
  auto properties =
      event->properties() ? *event->properties() : Event::Properties();
  properties.emplace(kPropertyKeyboardDeviceId, std::move(buf));
  event->SetProperties(properties);
}

}  // namespace ui
