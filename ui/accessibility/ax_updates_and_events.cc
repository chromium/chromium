// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_updates_and_events.h"

#include "ui/accessibility/ax_event.h"

namespace ui {

AXUpdatesAndEvents::AXUpdatesAndEvents() : ax_tree_id(ui::AXTreeIDUnknown()) {}

AXUpdatesAndEvents::AXUpdatesAndEvents(AXUpdatesAndEvents&& other) = default;
AXUpdatesAndEvents& AXUpdatesAndEvents::operator=(AXUpdatesAndEvents&& other) =
    default;

AXUpdatesAndEvents::~AXUpdatesAndEvents() = default;

AXLocationChanges::AXLocationChanges()
    : id(-1), ax_tree_id(ui::AXTreeIDUnknown()) {}

AXLocationChanges::AXLocationChanges(const AXLocationChanges& other) = default;

AXLocationChanges::~AXLocationChanges() {}

}  // namespace ui
