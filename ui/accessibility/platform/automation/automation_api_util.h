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

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_API_UTIL_H_
