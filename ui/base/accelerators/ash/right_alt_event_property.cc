// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/ash/right_alt_event_property.h"

#include <cstdint>
#include <vector>

#include "ui/events/event.h"

namespace ui {

namespace {
// Tag used to mark events as being for right alt.
constexpr char kPropertyRightAlt[] = "right_alt_event";
}  // namespace

void SetRightAltProperty(Event* event) {
  auto properties =
      event->properties() ? *event->properties() : Event::Properties();
  properties.emplace(kPropertyRightAlt, std::vector<std::uint8_t>());
  event->SetProperties(properties);
}

bool HasRightAltProperty(const Event& event) {
  auto* properties = event.properties();
  if (!properties) {
    return false;
  }
  return properties->find(kPropertyRightAlt) != properties->end();
}

}  // namespace ui
