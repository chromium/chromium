// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_AURALINUX_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_AURALINUX_H_

#include <atk/atk.h>
#include <atspi/atspi.h>

#include "base/component_export.h"
#include "base/values.h"

namespace ui {
struct AXTreeSelector;

COMPONENT_EXPORT(AX_PLATFORM)
const char* ATSPIStateToString(AtspiStateType state);
COMPONENT_EXPORT(AX_PLATFORM)
const char* ATSPIRelationToString(AtspiRelationType relation);
COMPONENT_EXPORT(AX_PLATFORM) const char* ATSPIRoleToString(AtspiRole role);
COMPONENT_EXPORT(AX_PLATFORM) const char* AtkRoleToString(AtkRole role);
COMPONENT_EXPORT(AX_PLATFORM)
std::vector<AtspiAccessible*> ChildrenOf(AtspiAccessible* node);
COMPONENT_EXPORT(AX_PLATFORM)
AtspiAccessible* FindAccessible(const AXTreeSelector&);
COMPONENT_EXPORT(AX_PLATFORM) std::string GetDOMId(AtspiAccessible* node);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_AURALINUX_H_
