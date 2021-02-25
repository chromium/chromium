// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_AURALINUX_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_AURALINUX_H_

#include <atk/atk.h>
#include <atspi/atspi.h>

#include "ui/accessibility/ax_export.h"

namespace ui {
struct AXTreeSelector;

AX_EXPORT const char* ATSPIStateToString(AtspiStateType state);
AX_EXPORT const char* ATSPIRoleToString(AtspiRole role);
AX_EXPORT const char* AtkRoleToString(AtkRole role);
AX_EXPORT AtspiAccessible* FindAccessible(const AXTreeSelector&);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_AURALINUX_H_
