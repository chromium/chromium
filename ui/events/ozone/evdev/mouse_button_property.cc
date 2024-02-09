// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/mouse_button_property.h"

#include <cstdint>
#include <cstring>
#include <optional>

#include "ui/events/event.h"

namespace ui {

const char kForwardBackMouseButtonPropertyName[] = "ForwardBackMouseButtonCode";

std::optional<uint32_t> GetForwardBackMouseButtonProperty(const Event& event) {
  auto* properties = event.properties();
  if (!properties) {
    return std::nullopt;
  }

  auto iter = properties->find(kForwardBackMouseButtonPropertyName);
  if (iter == properties->end()) {
    return std::nullopt;
  }

  CHECK_LE(iter->second.size(), sizeof(uint32_t));
  uint32_t result = 0;
  std::memcpy(&result, iter->second.data(), iter->second.size());
  return result;
}

void SetForwardBackMouseButtonProperty(Event& event, uint32_t button) {
  std::vector<uint8_t> buffer;
  buffer.resize(sizeof(uint32_t));
  std::memcpy(buffer.data(), &button, buffer.size());
  Event::Properties properties =
      event.properties() ? *event.properties() : Event::Properties();
  properties.emplace(kForwardBackMouseButtonPropertyName, std::move(buffer));
  event.SetProperties(std::move(properties));
}

}  // namespace ui
