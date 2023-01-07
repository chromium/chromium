// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_API_UTIL_H_
#define UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_API_UTIL_H_

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event_generator.h"

namespace ui {

bool AX_EXPORT ShouldIgnoreAXEventForAutomation(ax::mojom::Event event_type);

bool AX_EXPORT
ShouldIgnoreGeneratedEventForAutomation(AXEventGenerator::Event event_type);

std::tuple<ax::mojom::Event, AXEventGenerator::Event> AX_EXPORT
MakeTupleForAutomationFromEventTypes(
    const ax::mojom::Event& ax_event,
    const AXEventGenerator::Event& generated_event);

// Parses a string representing an event type into an Event tuple.
std::tuple<ax::mojom::Event, AXEventGenerator::Event> AX_EXPORT
AutomationEventTypeToAXEventTuple(const char* event_type_string);

// Possible tree changes to listen to using addTreeChangeObserver. Note that
// listening to all tree changes can be expensive.
enum TreeChangeObserverFilter {
  kNone,
  kNoTreeChanges,
  kLiveRegionTreeChanges,
  kTextMarkerChanges,
  kAllTreeChanges,
};

struct TreeChangeObserver {
  int id;
  TreeChangeObserverFilter filter;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_API_UTIL_H_
