// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_updates_and_events.h"

#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_node_id_forward.h"

namespace ui {

AXUpdatesAndEvents::AXUpdatesAndEvents() : ax_tree_id(AXTreeIDUnknown()) {}

AXUpdatesAndEvents::AXUpdatesAndEvents(AXUpdatesAndEvents&& other) = default;
AXUpdatesAndEvents& AXUpdatesAndEvents::operator=(AXUpdatesAndEvents&& other) =
    default;

AXUpdatesAndEvents::~AXUpdatesAndEvents() = default;

bool AXUpdatesAndEvents::HasValidAXNodeIDsFromRenderer() const {
  for (const AXTreeUpdate& update : updates) {
    if (!update.HasValidAXNodeIDsFromRenderer()) {
      return false;
    }
  }
  for (const AXEvent& event : events) {
    if (!IsValidAXNodeIDFromRenderer(event.id)) {
      return false;
    }
  }
  return true;
}

}  // namespace ui
