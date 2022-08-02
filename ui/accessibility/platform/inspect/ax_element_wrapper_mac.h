// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_ELEMENT_WRAPPER_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_ELEMENT_WRAPPER_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/callback_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace ui {

// A wrapper around AXUIElement or NSAccessibilityElement object.
class AX_EXPORT AXElementWrapper final {
 public:
  // Returns true if the object is either NSAccessibilityElement or
  // AXUIElement.
  static bool IsValidElement(const id node);

  // Return true if the object is internal BrowserAccessibilityCocoa.
  static bool IsNSAccessibilityElement(const id node);

  // Returns true if the object is AXUIElement.
  static bool IsAXUIElement(const id node);

  // Returns the children of an accessible object, either AXUIElement or
  // BrowserAccessibilityCocoa.
  static NSArray* ChildrenOf(const id node);

  // Returns the DOM id of a given node (either AXUIElement or
  // BrowserAccessibilityCocoa).
  static std::string DOMIdOf(const id node);

  AXElementWrapper(const id node) : node_(node){};

  // Returns true if the object is either an NSAccessibilityElement or
  // AXUIElement.
  bool IsValidElement() const;

  // Return true if the object is an internal BrowserAccessibilityCocoa.
  bool IsNSAccessibilityElement() const;

  // Returns true if the object is an AXUIElement.
  bool IsAXUIElement() const;

  // Returns the wrapped object.
  id AsId() const;

  // Returns the DOM id of the object.
  std::string DOMId() const;

  // Returns the children of the object.
  NSArray* Children() const;

  // Returns the AXSize and AXPosition attributes for the object.
  NSSize Size() const;
  NSPoint Position() const;

  // Returns the (parameterized) attributes of the object.
  NSArray* AttributeNames() const;
  NSArray* ParameterizedAttributeNames() const;

  // Returns (parameterized) attribute value on the object.
  id GetAttributeValue(NSString* attribute) const;
  id GetParameterizedAttributeValue(NSString* attribute, id parameter) const;

  // Performs the given selector on the object and returns the result. If
  // the object does not conform to the NSAccessibility protocol or the selector
  // is not found, then returns nullopt.
  absl::optional<id> PerformSelector(const std::string& selector) const;

  // Performs the given selector on the object with exactly one string
  // argument and returns the result. If the object does not conform to the
  // NSAccessibility protocol or the selector is not found, then returns
  // nullopt.
  absl::optional<id> PerformSelector(const std::string& selector_string,
                                     const std::string& argument_string) const;

  // Sets attribute value on the object.
  void SetAttributeValue(NSString* attribute, id value) const;

  // Returns the list of actions supported on the object.
  NSArray* ActionNames() const;

  // Performs the given action on the object.
  void PerformAction(NSString* action) const;

 private:
  // Returns true on success, otherwise returns false and logs error.
  bool AXSuccess(AXError, const std::string& message) const;

  const id node_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_ELEMENT_WRAPPER_MAC_H_
