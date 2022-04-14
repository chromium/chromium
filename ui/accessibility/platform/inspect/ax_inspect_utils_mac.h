// Copyright 2020 The Chromium Authors. All rights reserved.
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

//
// Returns true if the given accessibility attribute is valid, and could have
// been exposed on certain accessibility objects.
AX_EXPORT bool IsValidAXAttribute(const std::string& attribute);

//
// Return true if the given object is internal BrowserAccessibilityCocoa.
AX_EXPORT bool IsNSAccessibilityElement(const id node);

//
// Returns true if the given object is AXUIElement.
AX_EXPORT bool IsAXUIElement(const id node);

//
// Returns children of an accessible object, either AXUIElement or
// BrowserAccessibilityCocoa.
AX_EXPORT NSArray* AXChildrenOf(const id node);

//
// Returns AXSize and AXPosition attributes for an accessible object.
AX_EXPORT NSSize AXSizeOf(const id node);
AX_EXPORT NSPoint AXPositionOf(const id node);

//
// Returns (parameterized) attributes of an accessible object, (either
// AXUIElement or BrowserAccessibilityCocoa).
AX_EXPORT NSArray* AXAttributeNamesOf(const id node);
AX_EXPORT NSArray* AXParameterizedAttributeNamesOf(const id node);

//
// Returns (parameterized) attribute value on a given node (either AXUIElement
// or BrowserAccessibilityCocoa).
AX_EXPORT id AXAttributeValueOf(const id node, NSString* attribute);
AX_EXPORT id AXParameterizedAttributeValueOf(const id node,
                                             NSString* attribute,
                                             id parameter);

//
// Performs the given selector on the given node and returns the result. If
// the node does not conform to the NSAccessibility protocol or the selector is
// not found, then returns nullopt.
AX_EXPORT absl::optional<id> PerformAXSelector(const id node,
                                               const std::string& selector);

//
// Performs the given selector on the given node with exactly one string
// argument and returns the result. If the node does not conform to the
// NSAccessibility protocol or the selector is not found, then returns nullopt.
AX_EXPORT absl::optional<id> PerformAXSelector(
    const id node,
    const std::string& selector_string,
    const std::string& argument_string);

//
// Sets attribute value on a given node (either AXUIElement or
// BrowserAccessibilityCocoa).
AX_EXPORT void SetAXAttributeValueOf(const id node,
                                     NSString* attribute,
                                     id value);

// Returns a list of actions supported on a given accessible node (either
// AXUIElement or BrowserAccessibilityCocoa).
AX_EXPORT NSArray* AXActionNamesOf(const id node);

// Performs action on a given accessible node (either AXUIElement or
// BrowserAccessibilityCocoa).
AX_EXPORT void PerformAXAction(const id node, NSString* action);

//
// Returns DOM id of a given node (either AXUIElement or
// BrowserAccessibilityCocoa).
AX_EXPORT std::string GetDOMId(const id node);

//
// Return AXElement in a tree by a given criteria.
using AXFindCriteria = base::RepeatingCallback<bool(const AXUIElementRef)>;
AX_EXPORT AXUIElementRef FindAXUIElement(const AXUIElementRef node,
                                         const AXFindCriteria& criteria);

//
// Returns AXUIElement and its application process id by a given tree selector.
AX_EXPORT std::pair<AXUIElementRef, int> FindAXUIElement(const AXTreeSelector&);

//
// Returns AXUIElement for a window having title matching the given pattern.
AX_EXPORT AXUIElementRef FindAXWindowChild(AXUIElementRef parent,
                                           const std::string& pattern);

// Returns true on success, otherwise returns false and logs error.
AX_EXPORT bool AXSuccess(AXError, const std::string& message);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_MAC_H_
