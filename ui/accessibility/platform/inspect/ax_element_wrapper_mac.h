// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_ELEMENT_WRAPPER_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_ELEMENT_WRAPPER_MAC_H_

#import <Cocoa/Cocoa.h>

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"
#include "ui/accessibility/platform/inspect/ax_optional.h"

namespace ui {

// Optional tri-state id object.
using AXOptionalNSObject = AXOptional<id>;

// A wrapper around either AXUIElement or NSAccessibilityElement object.
class COMPONENT_EXPORT(AX_PLATFORM) AXElementWrapper final {
 public:
  enum class AXType { kNSAccessibilityElement = 0, kAXUIElement };

  // Returns type of the node.
  static AXType TypeOf(const id node);

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

  explicit AXElementWrapper(const id node) : node_(node) {}

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
  AXOptionalNSObject GetAttributeValue(NSString* attribute) const;
  AXOptionalNSObject GetParameterizedAttributeValue(NSString* attribute,
                                                    id parameter) const;

  // Performs the given selector on the object and returns the result. If
  // the object does not conform to the NSAccessibility protocol or the selector
  // is not found, then returns nullopt.
  std::optional<id> PerformSelector(const std::string& selector) const;

  // Performs the given selector on the object with exactly one string
  // argument and returns the result. If the object does not conform to the
  // NSAccessibility protocol or the selector is not found, then returns
  // nullopt.
  std::optional<id> PerformSelector(const std::string& selector_string,
                                    const std::string& argument_string) const;

  // Sets attribute value on the object.
  void SetAttributeValue(NSString* attribute, id value) const;

  // Returns the list of actions supported on the object.
  NSArray* ActionNames() const;

  // Performs the given action on the object.
  void PerformAction(NSString* action) const;

  // Returns true if the object responds to the given selector.
  bool RespondsToSelector(SEL selector) const {
    return [node_ respondsToSelector:selector];
  }

  // Invokes a method of the given signature.
  template <typename ReturnType, typename... Args>
  typename std::enable_if<!std::is_same<ReturnType, void>::value,
                          ReturnType>::type
  Invoke(SEL selector, Args... args) const {
    NSInvocation* invocation = InvokeInternal(selector, args...);
    ReturnType return_value;
    [invocation getReturnValue:&return_value];
    return return_value;
  }

  template <typename ReturnType, typename... Args>
  typename std::enable_if<std::is_same<ReturnType, void>::value, void>::type
  Invoke(SEL selector, Args... args) const {
    InvokeInternal(selector, args...);
  }

 private:
  template <typename... Args>
  NSInvocation* InvokeInternal(SEL selector, Args... args) const {
    NSInvocation* invocation = [NSInvocation
        invocationWithMethodSignature:
            [[node_ class] instanceMethodSignatureForSelector:selector]];
    [invocation setSelector:selector];
    [invocation setTarget:node_];
    SetInvocationArguments<Args...>(invocation, 2, args...);
    [invocation invoke];
    return invocation;
  }

  template <typename Arg, typename... Args>
  void SetInvocationArguments(NSInvocation* invocation,
                              int argument_index,
                              Arg& arg,
                              Args&... args) const {
    [invocation setArgument:&arg atIndex:argument_index];
    SetInvocationArguments<Args...>(invocation, argument_index + 1, args...);
  }

  template <typename... Args>
  void SetInvocationArguments(NSInvocation*, int) const {}

  // Generates an error message from the given error.
  std::string AXErrorMessage(AXError, const std::string& message) const;

  // Returns true on success, otherwise returns false and logs error.
  bool AXSuccess(AXError result, const std::string& message) const;

  // Converts the given value and the error object into AXOptional object.
  AXOptionalNSObject ToOptional(id, AXError, const std::string& message) const;

  id __strong node_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_ELEMENT_WRAPPER_MAC_H_
