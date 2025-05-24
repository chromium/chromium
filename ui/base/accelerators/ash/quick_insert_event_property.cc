// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/ash/quick_insert_event_property.h"

#include <cstdint>
#include <vector>

#include "ui/events/event.h"

namespace ui {

namespace {
// Tag used to mark events as being for quick insert.
constexpr char kPropertyQuickInsert[] = "quick_insert_event";
}  // namespace

void SetQuickInsertProperty(Event* event) {
  auto properties =
      event->properties() ? *event->properties() : Event::Properties();
  properties.emplace(kPropertyQuickInsert, std::vector<std::uint8_t>());
  event->SetProperties(properties);
}

bool HasQuickInsertProperty(const Event& event) {
  auto* properties = event.properties();
  if (!properties) {
    return false;
  }
  return properties->find(kPropertyQuickInsert) != properties->end();
}

}  // namespace ui
