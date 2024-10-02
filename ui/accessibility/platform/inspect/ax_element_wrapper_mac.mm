// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_element_wrapper_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>

#include <ostream>

#include "base/apple/bridging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/containers/fixed_flat_set.h"
#include "base/debug/stack_trace.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/pattern.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/accessibility/platform/ax_platform_node_cocoa.h"
#include "ui/accessibility/platform/ax_private_attributes_mac.h"

// error: 'accessibilityAttributeNames' is deprecated: first deprecated in
// macOS 10.10 - Use the NSAccessibility protocol methods instead (see
// NSAccessibilityProtocols.h
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace ui {

constexpr char kUnsupportedObject[] =
    "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";

// static
AXElementWrapper::AXType AXElementWrapper::TypeOf(const id node) {
  DCHECK(IsValidElement(node));
  if (IsNSAccessibilityElement(node)) {
    return AXType::kNSAccessibilityElement;
  }
  if (IsAXUIElement(node)) {
    return AXType::kAXUIElement;
  }
  NOTREACHED() << "Unknown accessibility object type";
}

// static
bool AXElementWrapper::IsValidElement(const id node) {
  return AXElementWrapper(node).IsValidElement();
}

// static
bool AXElementWrapper::IsNSAccessibilityElement(const id node) {
  return AXElementWrapper(node).IsNSAccessibilityElement();
}

// static
bool AXElementWrapper::IsAXUIElement(const id node) {
  return AXElementWrapper(node).IsAXUIElement();
}

// static
NSArray* AXElementWrapper::ChildrenOf(const id node) {
  return AXElementWrapper(node).Children();
}

// Returns DOM id of a given node (either AXUIElement or
// BrowserAccessibilityCocoa).
// static
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
  return CFGetTypeID((__bridge CFTypeRef)node_) == AXUIElementGetTypeID();
}

id AXElementWrapper::AsId() const {
  return node_;
}

std::string AXElementWrapper::DOMId() const {
  const id domid_value = *GetAttributeValue(@"AXDOMIdentifier");
  return base::SysNSStringToUTF8(static_cast<NSString*>(domid_value));
}

NSArray* AXElementWrapper::Children() const {
  if (IsNSAccessibilityElement())
    return [node_ children];

  if (IsAXUIElement()) {
    base::apple::ScopedCFTypeRef<CFTypeRef> children_ref;
    if ((AXUIElementCopyAttributeValue(
            (__bridge AXUIElementRef)node_, kAXChildrenAttribute,
            children_ref.InitializeInto())) == kAXErrorSuccess) {
      return base::apple::CFToNSOwnershipCast(
          (CFArrayRef)children_ref.release());
    }
    return nil;
  }

  NOTREACHED_IN_MIGRATION()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

NSSize AXElementWrapper::Size() const {
  if (IsNSAccessibilityElement()) {
    return [node_ accessibilityFrame].size;
  }

  if (!IsAXUIElement()) {
    NOTREACHED_IN_MIGRATION()
        << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
    return NSMakeSize(0, 0);
  }

  id value = *GetAttributeValue(NSAccessibilitySizeAttribute);
  if (value && CFGetTypeID((__bridge CFTypeRef)value) == AXValueGetTypeID()) {
    AXValueType type = AXValueGetType((__bridge AXValueRef)value);
    if (type == kAXValueCGSizeType) {
      NSSize size;
      if (AXValueGetValue((__bridge AXValueRef)value, type, &size)) {
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

  if (IsAXUIElement()) {
    id value = *GetAttributeValue(NSAccessibilityPositionAttribute);
    if (value && CFGetTypeID((__bridge CFTypeRef)value) == AXValueGetTypeID()) {
      AXValueType type = AXValueGetType((__bridge AXValueRef)value);
      if (type == kAXValueCGPointType) {
        NSPoint point;
        if (AXValueGetValue((__bridge AXValueRef)value, type, &point)) {
          return point;
        }
      }
    }
    return NSMakePoint(0, 0);
  }

  NOTREACHED_IN_MIGRATION()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return NSMakePoint(0, 0);
}

NSArray* AXElementWrapper::AttributeNames() const {
  if (IsNSAccessibilityElement()) {
    // The NSAccessibility protocol implementation in AXPlatformNodeCocoa no
    // longer exposes old-style attributes. Instead, it provides the
    // internalAccessibilityAttributeNames method for backward compatibility in
    // testing.
    if ([node_ isKindOfClass:[AXPlatformNodeCocoa class]]) {
      return [node_ internalAccessibilityAttributeNames];
    }
    return [node_ accessibilityAttributeNames];
  }

  if (IsAXUIElement()) {
    base::apple::ScopedCFTypeRef<CFArrayRef> attributes_ref;
    AXError result = AXUIElementCopyAttributeNames(
        (__bridge AXUIElementRef)node_, attributes_ref.InitializeInto());
    if (AXSuccess(result, "AXAttributeNamesOf")) {
      return base::apple::CFToNSOwnershipCast(attributes_ref.release());
    }
    return nil;
  }

  NOTREACHED_IN_MIGRATION()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

NSArray* AXElementWrapper::ParameterizedAttributeNames() const {
  if (IsNSAccessibilityElement()) {
    return [node_ accessibilityParameterizedAttributeNames];
  }

  if (IsAXUIElement()) {
    base::apple::ScopedCFTypeRef<CFArrayRef> attributes_ref;
    AXError result = AXUIElementCopyParameterizedAttributeNames(
        (__bridge AXUIElementRef)node_, attributes_ref.InitializeInto());
    if (AXSuccess(result, "AXParameterizedAttributeNamesOf")) {
      return base::apple::CFToNSOwnershipCast(attributes_ref.release());
    }
    return nil;
  }

  NOTREACHED_IN_MIGRATION()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

AXOptionalNSObject AXElementWrapper::GetAttributeValue(
    NSString* attribute) const {
  if (IsNSAccessibilityElement()) {
    return AXOptionalNSObject([node_ accessibilityAttributeValue:attribute]);
  }

  if (IsAXUIElement()) {
    base::apple::ScopedCFTypeRef<CFTypeRef> value_ref;
    AXError result = AXUIElementCopyAttributeValue(
        (__bridge AXUIElementRef)node_, (__bridge CFStringRef)attribute,
        value_ref.InitializeInto());
    return ToOptional(
        (__bridge id)value_ref.get(), result,
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
    base::apple::ScopedCFTypeRef<CFTypeRef> parameter_ref(
        CFBridgingRetain(parameter));
    if ([parameter isKindOfClass:[NSValue class]] &&
        !strcmp([parameter objCType], @encode(NSRange))) {
      NSRange range = [parameter rangeValue];
      parameter_ref.reset(AXValueCreate(kAXValueTypeCFRange, &range));
    }

    // Get value.
    base::apple::ScopedCFTypeRef<CFTypeRef> value_ref;
    AXError result = AXUIElementCopyParameterizedAttributeValue(
        (__bridge AXUIElementRef)node_, (__bridge CFStringRef)attribute,
        parameter_ref.get(), value_ref.InitializeInto());

    return ToOptional((__bridge id)value_ref.get(), result,
                      "GetParameterizedAttributeValue(" +
                          base::SysNSStringToUTF8(attribute) + ")");
  }

  return AXOptionalNSObject::Error(kUnsupportedObject);
}

std::optional<id> AXElementWrapper::PerformSelector(
    const std::string& selector_string) const {
  if (![node_ conformsToProtocol:@protocol(NSAccessibility)])
    return std::nullopt;

  NSString* selector_nsstring = base::SysUTF8ToNSString(selector_string);
  SEL selector = NSSelectorFromString(selector_nsstring);

  if ([node_ respondsToSelector:selector])
    return [node_ valueForKey:selector_nsstring];
  return std::nullopt;
}

std::optional<id> AXElementWrapper::PerformSelector(
    const std::string& selector_string,
    const std::string& argument_string) const {
  if (![node_ conformsToProtocol:@protocol(NSAccessibility)])
    return std::nullopt;

  SEL selector =
      NSSelectorFromString(base::SysUTF8ToNSString(selector_string + ":"));
  NSString* argument = base::SysUTF8ToNSString(argument_string);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
  if ([node_ respondsToSelector:selector])
    return [node_ performSelector:selector withObject:argument];
#pragma clang diagnostic pop
  return std::nullopt;
}

void AXElementWrapper::SetAttributeValue(NSString* attribute, id value) const {
  if (IsNSAccessibilityElement()) {
    [node_ accessibilitySetValue:value forAttribute:attribute];
    return;
  }

  if (IsAXUIElement()) {
    AXUIElementSetAttributeValue((__bridge AXUIElementRef)node_,
                                 (__bridge CFStringRef)attribute,
                                 (__bridge CFTypeRef)value);
    return;
  }

  NOTREACHED_IN_MIGRATION()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
}

NSArray* AXElementWrapper::ActionNames() const {
  if (IsNSAccessibilityElement())
    return [node_ accessibilityActionNames];

  if (IsAXUIElement()) {
    base::apple::ScopedCFTypeRef<CFArrayRef> attributes_ref;
    if ((AXUIElementCopyActionNames((__bridge AXUIElementRef)node_,
                                    attributes_ref.InitializeInto())) ==
        kAXErrorSuccess) {
      return base::apple::CFToNSOwnershipCast(attributes_ref.release());
    }
    return nil;
  }

  NOTREACHED_IN_MIGRATION()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

void AXElementWrapper::PerformAction(NSString* action) const {
  if (IsNSAccessibilityElement()) {
    [node_ accessibilityPerformAction:action];
    return;
  }

  if (IsAXUIElement()) {
    AXUIElementPerformAction((__bridge AXUIElementRef)node_,
                             (__bridge CFStringRef)action);
    return;
  }

  NOTREACHED_IN_MIGRATION()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
}

std::string AXElementWrapper::AXErrorMessage(AXError result,
                                             const std::string& message) const {
  if (result == kAXErrorSuccess) {
    return {};
  }

  std::string error;
  switch (result) {
    case kAXErrorAPIDisabled:
      error = "API disabled; you may need to add terminal and/or this binary "
              "to System Settings -> Privacy & Security -> Accessibility";
      break;
    case kAXErrorActionUnsupported:
      error = "action unsupported";
      break;
    case kAXErrorAttributeUnsupported:
      error = "attribute unsupported";
      break;
    case kAXErrorCannotComplete:
      error = "cannot complete";
      break;
    case kAXErrorFailure:
      error = "failure";
      break;
    case kAXErrorIllegalArgument:
      error = "illegal argument";
      break;
    case kAXErrorInvalidUIElement:
      error = "invalid UI element";
      break;
    case kAXErrorInvalidUIElementObserver:
      error = "illegal UI element observer";
      break;
    case kAXErrorNoValue:
      error = "no value";
      break;
    case kAXErrorNotEnoughPrecision:
      error = "not enough precision";
      break;
    case kAXErrorNotImplemented:
      error = "not implemented";
      break;
    case kAXErrorNotificationAlreadyRegistered:
      error = "notification already registered";
      break;
    case kAXErrorNotificationNotRegistered:
      error = "notification not registered";
      break;
    case kAXErrorNotificationUnsupported:
      error = "notification unsupported";
      break;
    case kAXErrorParameterizedAttributeUnsupported:
      error = "parameterized attribute unsupported";
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
