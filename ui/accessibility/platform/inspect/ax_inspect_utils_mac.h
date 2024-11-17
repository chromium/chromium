// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_MAC_H_

#include <ApplicationServices/ApplicationServices.h>

#include <optional>

#include "base/apple/scoped_cftyperef.h"
#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace ui {

// Returns true if the given accessibility attribute is valid, and could have
// been exposed on certain accessibility objects.
COMPONENT_EXPORT(AX_PLATFORM)
bool IsValidAXAttribute(const std::string& attribute);

// Return AXElement in a tree by a given accessibility role.
COMPONENT_EXPORT(AX_PLATFORM)
base::apple::ScopedCFTypeRef<AXUIElementRef> FindAXUIElement(
    const AXUIElementRef node,
    const char* role);

// Return AXElement in a tree by a given criteria.
using AXFindCriteria = base::RepeatingCallback<bool(const AXUIElementRef)>;
COMPONENT_EXPORT(AX_PLATFORM)
base::apple::ScopedCFTypeRef<AXUIElementRef> FindAXUIElement(
    const AXUIElementRef node,
    const AXFindCriteria& criteria);

// Returns AXUIElement and its application process id by a given tree selector.
COMPONENT_EXPORT(AX_PLATFORM)
std::pair<base::apple::ScopedCFTypeRef<AXUIElementRef>, int> FindAXUIElement(
    const AXTreeSelector&);

// Returns application AXUIElement and its application process id by a given
// tree selector.
COMPONENT_EXPORT(AX_PLATFORM)
std::pair<base::apple::ScopedCFTypeRef<AXUIElementRef>, int> FindAXApplication(
    const AXTreeSelector&);

// Returns AXUIElement for a window having title matching the given pattern.
COMPONENT_EXPORT(AX_PLATFORM)
base::apple::ScopedCFTypeRef<AXUIElementRef> FindAXWindowChild(
    AXUIElementRef parent,
    const std::string& pattern);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_MAC_H_
