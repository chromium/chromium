// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_ASH_QUICK_INSERT_EVENT_PROPERTY_H_
#define UI_BASE_ACCELERATORS_ASH_QUICK_INSERT_EVENT_PROPERTY_H_

#include "base/component_export.h"

namespace ui {

class Event;

// Sets the quick insert property on the given event to mark the event is
// supposed to be used as the quick insert key for the purposes of accelerators.
COMPONENT_EXPORT(UI_BASE)
void SetQuickInsertProperty(Event* event);

// Checks if the quick insert property is set on the given event.
COMPONENT_EXPORT(UI_BASE)
bool HasQuickInsertProperty(const Event& event);

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_ASH_QUICK_INSERT_EVENT_PROPERTY_H_
