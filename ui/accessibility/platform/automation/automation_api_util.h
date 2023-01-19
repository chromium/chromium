// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_API_UTIL_H_
#define UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_API_UTIL_H_

#include "base/component_export.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event_generator.h"

namespace ui {

enum class AXPositionKind;

bool COMPONENT_EXPORT(AX_PLATFORM)
    ShouldIgnoreAXEventForAutomation(ax::mojom::Event event_type);

bool COMPONENT_EXPORT(AX_PLATFORM)
    ShouldIgnoreGeneratedEventForAutomation(AXEventGenerator::Event event_type);

std::tuple<ax::mojom::Event, AXEventGenerator::Event> COMPONENT_EXPORT(
    AX_PLATFORM)
    MakeTupleForAutomationFromEventTypes(
        const ax::mojom::Event& ax_event,
        const AXEventGenerator::Event& generated_event);

// Parses a string representing an event type into an Event tuple.
std::tuple<ax::mojom::Event, AXEventGenerator::Event> COMPONENT_EXPORT(
    AX_PLATFORM)
    AutomationEventTypeToAXEventTuple(const char* event_type_string);

AXPositionKind COMPONENT_EXPORT(AX_PLATFORM)
    StringToAXPositionKind(const std::string& type);

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
