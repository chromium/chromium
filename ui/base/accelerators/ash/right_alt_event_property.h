// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_ASH_RIGHT_ALT_EVENT_PROPERTY_H_
#define UI_BASE_ACCELERATORS_ASH_RIGHT_ALT_EVENT_PROPERTY_H_

#include "base/component_export.h"

namespace ui {

class Event;

// Sets the right alt property on the given event to mark the event is supposed
// to be used as the right alt key for the purposes of accelerators.
COMPONENT_EXPORT(UI_BASE)
void SetRightAltProperty(Event* event);

// Checks if the right alt property is set on the given event.
COMPONENT_EXPORT(UI_BASE)
bool HasRightAltProperty(const Event& event);

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_ASH_RIGHT_ALT_EVENT_PROPERTY_H_
