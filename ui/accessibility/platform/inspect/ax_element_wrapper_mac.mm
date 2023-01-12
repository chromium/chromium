// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_element_wrapper_mac.h"

#include <ostream>

#include "base/containers/fixed_flat_set.h"
#include "base/debug/stack_trace.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/pattern.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/accessibility/platform/ax_private_attributes_mac.h"

// error: 'accessibilityAttributeNames' is deprecated: first deprecated in
// macOS 10.10 - Use the NSAccessibility protocol methods instead (see
// NSAccessibilityProtocols.h
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace ui {

using base::SysNSStringToUTF8;

constexpr char kUnsupportedObject[] =
    "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";

// static
bool AXElementWrapper::IsValidElement(const id node) {
  return AXElementWrapper(node).IsValidElement();
}

bool AXElementWrapper::IsNSAccessibilityElement(const id node) {
  return AXElementWrapper(node).IsNSAccessibilityElement();
}

bool AXElementWrapper::IsAXUIElement(const id node) {
  return AXElementWrapper(node).IsAXUIElement();
}

NSArray* AXElementWrapper::ChildrenOf(const id node) {
  return AXElementWrapper(node).Children();
}

// Returns DOM id of a given node (either AXUIElement or
// BrowserAccessibilityCocoa).
std::string AXElementWrapper::DOMIdOf(const id node) {
  return AXElementWrapper(node).DOMId();
}

bool AXElementWrapper::IsValidElement() const {
  return IsNSAccessibilityElement() || IsAXUIElement();
}

bool AXElementWrapper::IsNSAccessibilityElement() const {
  return [node_ isKindOfClass:[NSAccessibilityElement class]];
}

bool AXElementWrapper::IsAXUIElement() const {
  return CFGetTypeID(node_) == AXUIElementGetTypeID();
}

id AXElementWrapper::AsId() const {
  return node_;
}

std::string AXElementWrapper::DOMId() const {
  const id domid_value =
      *GetAttributeValue(base::SysUTF8ToNSString("AXDOMIdentifier"));
  return base::SysNSStringToUTF8(static_cast<NSString*>(domid_value));
}

NSArray* AXElementWrapper::Children() const {
  if (IsNSAccessibilityElement())
    return [node_ children];

  if (IsAXUIElement()) {
    CFTypeRef children_ref;
    if ((AXUIElementCopyAttributeValue(static_cast<AXUIElementRef>(node_),
                                       kAXChildrenAttribute, &children_ref)) ==
        kAXErrorSuccess)
      return static_cast<NSArray*>(children_ref);
    return nil;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

NSSize AXElementWrapper::Size() const {
  if (IsNSAccessibilityElement()) {
    return [node_ accessibilityFrame].size;
  }

  if (!IsAXUIElement()) {
    NOTREACHED()
        << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
    return NSMakeSize(0, 0);
  }

  id value = *GetAttributeValue(NSAccessibilitySizeAttribute);
  if (value && CFGetTypeID(value) == AXValueGetTypeID()) {
    AXValueType type = AXValueGetType(static_cast<AXValueRef>(value));
    if (type == kAXValueCGSizeType) {
      NSSize size;
      if (AXValueGetValue(static_cast<AXValueRef>(value), type, &size)) {
        return size;
      }
    }
  }
  return NSMakeSize(0, 0);
}

NSPoint AXElementWrapper::Position() const {
  if (IsNSAccessibilityElement()) {
    return [node_ accessibilityFrame].origin;
  }

  if (!IsAXUIElement()) {
    NOTREACHED()
        << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
    return NSMakePoint(0, 0);
  }

  id value = *GetAttributeValue(NSAccessibilityPositionAttribute);
  if (value && CFGetTypeID(value) == AXValueGetTypeID()) {
    AXValueType type = AXValueGetType(static_cast<AXValueRef>(value));
    if (type == kAXValueCGPointType) {
      NSPoint point;
      if (AXValueGetValue(static_cast<AXValueRef>(value), type, &point)) {
        return point;
      }
    }
  }
  return NSMakePoint(0, 0);
}

NSArray* AXElementWrapper::AttributeNames() const {
  if (IsNSAccessibilityElement())
    return [node_ accessibilityAttributeNames];

  if (IsAXUIElement()) {
    CFArrayRef attributes_ref;
    AXError result = AXUIElementCopyAttributeNames(
        static_cast<AXUIElementRef>(node_), &attributes_ref);
    if (AXSuccess(result, "AXAttributeNamesOf"))
      return static_cast<NSArray*>(attributes_ref);
    return nil;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

NSArray* AXElementWrapper::ParameterizedAttributeNames() const {
  if (IsNSAccessibilityElement())
    return [node_ accessibilityParameterizedAttributeNames];

  if (IsAXUIElement()) {
    CFArrayRef attributes_ref;
    AXError result = AXUIElementCopyParameterizedAttributeNames(
        static_cast<AXUIElementRef>(node_), &attributes_ref);
    if (AXSuccess(result, "AXParameterizedAttributeNamesOf"))
      return static_cast<NSArray*>(attributes_ref);
    return nil;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

AXOptionalNSObject AXElementWrapper::GetAttributeValue(
    NSString* attribute) const {
  if (IsNSAccessibilityElement())
    return AXOptionalNSObject([node_ accessibilityAttributeValue:attribute]);

  if (IsAXUIElement()) {
    CFTypeRef value_ref;
    AXError result = AXUIElementCopyAttributeValue(
        static_cast<AXUIElementRef>(node_), static_cast<CFStringRef>(attribute),
        &value_ref);
    return ToOptional(
        static_cast<id>(value_ref), result,
        "AXGetAttributeValue(" + base::SysNSStringToUTF8(attribute) + ")");
  }

  return AXOptionalNSObject::Error(kUnsupportedObject);
}

AXOptionalNSObject AXElementWrapper::GetParameterizedAttributeValue(
    NSString* attribute,
    id parameter) const {
  if (IsNSAccessibilityElement())
    return AXOptionalNSObject([node_ accessibilityAttributeValue:attribute
                                                    forParameter:parameter]);

  if (IsAXUIElement()) {
    // Convert NSValue parameter to CFTypeRef if needed.
    CFTypeRef parameter_ref = static_cast<CFTypeRef>(parameter);
    if ([parameter isKindOfClass:[NSValue class]] &&
        !strcmp([static_cast<NSValue*>(parameter) objCType],
                @encode(NSRange))) {
      NSRange range = [static_cast<NSValue*>(parameter) rangeValue];
      parameter_ref = AXValueCreate(kAXValueTypeCFRange, &range);
    }

    // Get value.
    CFTypeRef value_ref;
    AXError result = AXUIElementCopyParameterizedAttributeValue(
        static_cast<AXUIElementRef>(node_), static_cast<CFStringRef>(attribute),
        parameter_ref, &value_ref);

    return ToOptional(static_cast<id>(value_ref), result,
                      "GetParameterizedAttributeValue(" +
                          base::SysNSStringToUTF8(attribute) + ")");
  }

  return AXOptionalNSObject::Error(kUnsupportedObject);
}

absl::optional<id> AXElementWrapper::PerformSelector(
    const std::string& selector_string) const {
  if (![node_ conformsToProtocol:@protocol(NSAccessibility)])
    return absl::nullopt;

  NSString* selector_nsstring = base::SysUTF8ToNSString(selector_string);
  SEL selector = NSSelectorFromString(selector_nsstring);

  if ([node_ respondsToSelector:selector])
    return [node_ valueForKey:selector_nsstring];
  return absl::nullopt;
}

absl::optional<id> AXElementWrapper::PerformSelector(
    const std::string& selector_string,
    const std::string& argument_string) const {
  if (![node_ conformsToProtocol:@protocol(NSAccessibility)])
    return absl::nullopt;

  SEL selector =
      NSSelectorFromString(base::SysUTF8ToNSString(selector_string + ":"));
  NSString* argument = base::SysUTF8ToNSString(argument_string);

  if ([node_ respondsToSelector:selector])
    return [node_ performSelector:selector withObject:argument];
  return absl::nullopt;
}

void AXElementWrapper::SetAttributeValue(NSString* attribute, id value) const {
  if (IsNSAccessibilityElement()) {
    [node_ accessibilitySetValue:value forAttribute:attribute];
    return;
  }

  if (IsAXUIElement()) {
    AXUIElementSetAttributeValue(static_cast<AXUIElementRef>(node_),
                                 static_cast<CFStringRef>(attribute),
                                 static_cast<CFTypeRef>(value));
    return;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
}

NSArray* AXElementWrapper::ActionNames() const {
  if (IsNSAccessibilityElement())
    return [node_ accessibilityActionNames];

  if (IsAXUIElement()) {
    CFArrayRef attributes_ref;
    if ((AXUIElementCopyActionNames(static_cast<AXUIElementRef>(node_),
                                    &attributes_ref)) == kAXErrorSuccess)
      return static_cast<NSArray*>(attributes_ref);
    return nil;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

void AXElementWrapper::PerformAction(NSString* action) const {
  if (IsNSAccessibilityElement()) {
    [node_ accessibilityPerformAction:action];
    return;
  }

  if (IsAXUIElement()) {
    AXUIElementPerformAction(static_cast<AXUIElementRef>(node_),
                             static_cast<CFStringRef>(action));
    return;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
}

std::string AXElementWrapper::AXErrorMessage(AXError result,
                                             const std::string& message) const {
  if (result == kAXErrorSuccess) {
    return {};
  }

  std::string error;
  switch (result) {
    case kAXErrorAttributeUnsupported:
      error = "attribute unsupported";
      break;
    case kAXErrorParameterizedAttributeUnsupported:
      error = "parameterized attribute unsupported";
      break;
    case kAXErrorNoValue:
      error = "no value";
      break;
    case kAXErrorIllegalArgument:
      error = "illegal argument";
      break;
    case kAXErrorInvalidUIElement:
      error = "invalid UIElement";
      break;
    case kAXErrorCannotComplete:
      error = "cannot complete";
      break;
    case kAXErrorNotImplemented:
      error = "not implemented";
      break;
    default:
      error = "unknown error";
      break;
  }
  return {message + ": " + error};
}

bool AXElementWrapper::AXSuccess(AXError result,
                                 const std::string& message) const {
  std::string message_text = AXErrorMessage(result, message);
  if (message_text.empty())
    return true;

  LOG(WARNING) << message_text;
  return false;
}

AXOptionalNSObject AXElementWrapper::ToOptional(
    id value,
    AXError result,
    const std::string& message) const {
  if (result == kAXErrorSuccess)
    return AXOptionalNSObject(value);

  return AXOptionalNSObject::Error(AXErrorMessage(result, message));
}

}  // namespace ui

#pragma clang diagnostic pop
