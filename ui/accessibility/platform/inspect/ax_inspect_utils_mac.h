// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/callback_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

using ui::AXTreeSelector;

namespace ui {

// Returns true if the given accessibility attribute is valid, and could have
// been exposed on certain accessibility objects.
AX_EXPORT bool IsValidAXAttribute(const std::string& attribute);

// Return AXElement in a tree by a given criteria.
using AXFindCriteria = base::RepeatingCallback<bool(const AXUIElementRef)>;
AX_EXPORT AXUIElementRef FindAXUIElement(const AXUIElementRef node,
                                         const AXFindCriteria& criteria);

// Returns AXUIElement and its application process id by a given tree selector.
AX_EXPORT std::pair<AXUIElementRef, int> FindAXUIElement(const AXTreeSelector&);

// Returns AXUIElement for a window having title matching the given pattern.
AX_EXPORT AXUIElementRef FindAXWindowChild(AXUIElementRef parent,
                                           const std::string& pattern);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_MAC_H_
