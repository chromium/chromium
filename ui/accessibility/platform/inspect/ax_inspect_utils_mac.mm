// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_inspect_utils_mac.h"

#include <ostream>

#include "base/callback.h"
#include "base/containers/fixed_flat_set.h"
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

const char kChromeTitle[] = "Google Chrome";
const char kChromiumTitle[] = "Chromium";
const char kFirefoxTitle[] = "Firefox";
const char kSafariTitle[] = "Safari";

struct NSStringComparator {
  bool operator()(NSString* lhs, NSString* rhs) const {
    return [lhs compare:rhs] == NSOrderedAscending;
  }
};

bool IsValidAXAttribute(const std::string& attribute) {
  // static local to avoid a global static constructor.
  static auto kValidAttributes = base::MakeFixedFlatSet<NSString*>(
      {NSAccessibilityAccessKeyAttribute,
       NSAccessibilityARIAAtomicAttribute,
       NSAccessibilityARIABusyAttribute,
       NSAccessibilityARIAColumnCountAttribute,
       NSAccessibilityARIAColumnIndexAttribute,
       NSAccessibilityARIACurrentAttribute,
       NSAccessibilityARIALiveAttribute,
       NSAccessibilityARIAPosInSetAttribute,
       NSAccessibilityARIARelevantAttribute,
       NSAccessibilityARIARowCountAttribute,
       NSAccessibilityARIARowIndexAttribute,
       NSAccessibilityARIASetSizeAttribute,
       NSAccessibilityAutocompleteValueAttribute,
       NSAccessibilityBlockQuoteLevelAttribute,
       NSAccessibilityChromeAXNodeIdAttribute,
       NSAccessibilityColumnHeaderUIElementsAttribute,
       NSAccessibilityDescriptionAttribute,
       NSAccessibilityDetailsElementsAttribute,
       NSAccessibilityDOMClassList,
       NSAccessibilityDropEffectsAttribute,
       NSAccessibilityElementBusyAttribute,
       NSAccessibilityFocusableAncestorAttribute,
       NSAccessibilityGrabbedAttribute,
       NSAccessibilityHasPopupAttribute,
       NSAccessibilityInvalidAttribute,
       NSAccessibilityIsMultiSelectable,
       NSAccessibilityKeyShortcutsValueAttribute,
       NSAccessibilityLoadedAttribute,
       NSAccessibilityLoadingProgressAttribute,
       NSAccessibilityMathFractionNumeratorAttribute,
       NSAccessibilityMathFractionDenominatorAttribute,
       NSAccessibilityMathRootRadicandAttribute,
       NSAccessibilityMathRootIndexAttribute,
       NSAccessibilityMathBaseAttribute,
       NSAccessibilityMathSubscriptAttribute,
       NSAccessibilityMathSuperscriptAttribute,
       NSAccessibilityMathUnderAttribute,
       NSAccessibilityMathOverAttribute,
       NSAccessibilityMathPostscriptsAttribute,
       NSAccessibilityMathPrescriptsAttribute,
       NSAccessibilityOwnsAttribute,
       NSAccessibilityPopupValueAttribute,
       NSAccessibilityRequiredAttributeChrome,
       NSAccessibilityRoleDescriptionAttribute,
       NSAccessibilitySelectedAttribute,
       NSAccessibilityTitleAttribute,
       NSAccessibilityTitleUIElementAttribute,
       NSAccessibilityURLAttribute,
       NSAccessibilityVisitedAttribute},
      NSStringComparator());

  return kValidAttributes.contains(base::SysUTF8ToNSString(attribute));
}

bool IsNSAccessibilityElement(const id node) {
  return [node isKindOfClass:[NSAccessibilityElement class]];
}

bool IsAXUIElement(const id node) {
  return CFGetTypeID(node) == AXUIElementGetTypeID();
}

NSArray* AXChildrenOf(const id node) {
  if (IsNSAccessibilityElement(node))
    return [node children];

  if (IsAXUIElement(node)) {
    CFTypeRef children_ref;
    if ((AXUIElementCopyAttributeValue(static_cast<AXUIElementRef>(node),
                                       kAXChildrenAttribute, &children_ref)) ==
        kAXErrorSuccess)
      return static_cast<NSArray*>(children_ref);
    return nil;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

NSSize AXSizeOf(const id node) {
  if (IsNSAccessibilityElement(node)) {
    return [node accessibilityFrame].size;
  }

  if (!IsAXUIElement(node)) {
    NOTREACHED()
        << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
    return NSMakeSize(0, 0);
  }

  id value = AXAttributeValueOf(node, NSAccessibilitySizeAttribute);
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

NSPoint AXPositionOf(const id node) {
  if (IsNSAccessibilityElement(node)) {
    return [node accessibilityFrame].origin;
  }

  if (!IsAXUIElement(node)) {
    NOTREACHED()
        << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
    return NSMakePoint(0, 0);
  }

  id value = AXAttributeValueOf(node, NSAccessibilityPositionAttribute);
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

NSArray* AXAttributeNamesOf(const id node) {
  if (IsNSAccessibilityElement(node))
    return [node accessibilityAttributeNames];

  if (IsAXUIElement(node)) {
    CFArrayRef attributes_ref;
    AXError result = AXUIElementCopyAttributeNames(
        static_cast<AXUIElementRef>(node), &attributes_ref);
    if (AXSuccess(result, "AXAttributeNamesOf"))
      return static_cast<NSArray*>(attributes_ref);
    return nil;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

NSArray* AXParameterizedAttributeNamesOf(const id node) {
  if (IsNSAccessibilityElement(node))
    return [node accessibilityParameterizedAttributeNames];

  if (IsAXUIElement(node)) {
    CFArrayRef attributes_ref;
    AXError result = AXUIElementCopyParameterizedAttributeNames(
        static_cast<AXUIElementRef>(node), &attributes_ref);
    if (AXSuccess(result, "AXParameterizedAttributeNamesOf"))
      return static_cast<NSArray*>(attributes_ref);
    return nil;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

id AXAttributeValueOf(const id node, NSString* attribute) {
  if (IsNSAccessibilityElement(node))
    return [node accessibilityAttributeValue:attribute];

  if (IsAXUIElement(node)) {
    CFTypeRef value_ref;
    AXError result = AXUIElementCopyAttributeValue(
        static_cast<AXUIElementRef>(node), static_cast<CFStringRef>(attribute),
        &value_ref);
    if (AXSuccess(result, "AXAttributeValueOf(" +
                              base::SysNSStringToUTF8(attribute) + ")"))
      return static_cast<id>(value_ref);
    return nil;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

id AXParameterizedAttributeValueOf(const id node,
                                   NSString* attribute,
                                   id parameter) {
  if (IsNSAccessibilityElement(node))
    return [node accessibilityAttributeValue:attribute forParameter:parameter];

  if (IsAXUIElement(node)) {
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
        static_cast<AXUIElementRef>(node), static_cast<CFStringRef>(attribute),
        parameter_ref, &value_ref);
    if (AXSuccess(result, "AXParameterizedAttributeValueOf(" +
                              base::SysNSStringToUTF8(attribute) + ")"))
      return static_cast<id>(value_ref);

    return nil;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

absl::optional<id> PerformAXSelector(const id node,
                                     const std::string& selector_string) {
  if (![node conformsToProtocol:@protocol(NSAccessibility)])
    return absl::nullopt;

  NSString* selector_nsstring = base::SysUTF8ToNSString(selector_string);
  SEL selector = NSSelectorFromString(selector_nsstring);

  if ([node respondsToSelector:selector])
    return [node valueForKey:selector_nsstring];
  return absl::nullopt;
}

absl::optional<id> PerformAXSelector(const id node,
                                     const std::string& selector_string,
                                     const std::string& argument_string) {
  if (![node conformsToProtocol:@protocol(NSAccessibility)])
    return absl::nullopt;

  SEL selector =
      NSSelectorFromString(base::SysUTF8ToNSString(selector_string + ":"));
  NSString* argument = base::SysUTF8ToNSString(argument_string);

  if ([node respondsToSelector:selector])
    return [node performSelector:selector withObject:argument];
  return absl::nullopt;
}

void SetAXAttributeValueOf(const id node, NSString* attribute, id value) {
  if (IsNSAccessibilityElement(node)) {
    [node accessibilitySetValue:value forAttribute:attribute];
    return;
  }

  if (IsAXUIElement(node)) {
    AXUIElementSetAttributeValue(static_cast<AXUIElementRef>(node),
                                 static_cast<CFStringRef>(attribute),
                                 static_cast<CFTypeRef>(value));
    return;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
}

NSArray* AXActionNamesOf(const id node) {
  if (IsNSAccessibilityElement(node))
    return [node accessibilityActionNames];

  if (IsAXUIElement(node)) {
    CFArrayRef attributes_ref;
    if ((AXUIElementCopyActionNames(static_cast<AXUIElementRef>(node),
                                    &attributes_ref)) == kAXErrorSuccess)
      return static_cast<NSArray*>(attributes_ref);
    return nil;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
  return nil;
}

void PerformAXAction(const id node, NSString* action) {
  if (IsNSAccessibilityElement(node)) {
    [node accessibilityPerformAction:action];
    return;
  }

  if (IsAXUIElement(node)) {
    AXUIElementPerformAction(static_cast<AXUIElementRef>(node),
                             static_cast<CFStringRef>(action));
    return;
  }

  NOTREACHED()
      << "Only AXUIElementRef and BrowserAccessibilityCocoa are supported.";
}

std::string GetDOMId(const id node) {
  const id domid_value =
      AXAttributeValueOf(node, base::SysUTF8ToNSString("AXDOMIdentifier"));
  return base::SysNSStringToUTF8(static_cast<NSString*>(domid_value));
}

AXUIElementRef FindAXUIElement(const AXUIElementRef node,
                               const AXFindCriteria& criteria) {
  if (criteria.Run(node))
    return node;

  NSArray* children = AXChildrenOf(static_cast<id>(node));
  for (id child in children) {
    AXUIElementRef found =
        FindAXUIElement(static_cast<AXUIElementRef>(child), criteria);
    if (found != nil)
      return found;
  }

  return nil;
}

std::pair<AXUIElementRef, int> FindAXUIElement(const AXTreeSelector& selector) {
  if (selector.widget) {
    return {AXUIElementCreateApplication(selector.widget), selector.widget};
  }

  NSArray* windows = static_cast<NSArray*>(CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
      kCGNullWindowID));

  std::string title;
  if (selector.types & AXTreeSelector::Chrome)
    title = kChromeTitle;
  else if (selector.types & AXTreeSelector::Chromium)
    title = kChromiumTitle;
  else if (selector.types & AXTreeSelector::Firefox)
    title = kFirefoxTitle;
  else if (selector.types & AXTreeSelector::Safari)
    title = kSafariTitle;
  else
    return {nil, 0};

  for (NSDictionary* window_info in windows) {
    int pid =
        [static_cast<NSNumber*>([window_info objectForKey:@"kCGWindowOwnerPID"])
            intValue];
    std::string window_name = SysNSStringToUTF8(static_cast<NSString*>(
        [window_info objectForKey:@"kCGWindowOwnerName"]));

    AXUIElementRef node = nil;

    // Application pre-defined selectors match or application title exact match.
    bool appTitleMatch = window_name == selector.pattern;
    if (window_name == title || appTitleMatch)
      node = AXUIElementCreateApplication(pid);

    // Window title match. Application contain an AXWindow accessible object as
    // a first child, which accessible name contain a window title. For example:
    // 'Inbox (2) - asurkov@igalia.com - Gmail'.
    if (!selector.pattern.empty() && !appTitleMatch) {
      if (!node)
        node = AXUIElementCreateApplication(pid);

      AXUIElementRef window = FindAXWindowChild(node, selector.pattern);
      if (window)
        node = window;
    }

    // ActiveTab selector.
    if (node && selector.types & AXTreeSelector::ActiveTab) {
      node = FindAXUIElement(
          node, base::BindRepeating([](const AXUIElementRef node) {
            // Only active tab in exposed in browsers, thus find first
            // AXWebArea role.
            NSString* role = AXAttributeValueOf(static_cast<id>(node),
                                                NSAccessibilityRoleAttribute);
            return SysNSStringToUTF8(role) == "AXWebArea";
          }));
    }

    // Found a match.
    if (node)
      return {node, pid};
  }
  return {nil, 0};
}

AXUIElementRef FindAXWindowChild(AXUIElementRef parent,
                                 const std::string& pattern) {
  NSArray* children = AXChildrenOf(static_cast<id>(parent));
  if ([children count] == 0)
    return nil;

  id window = [children objectAtIndex:0];
  NSString* role = AXAttributeValueOf(window, NSAccessibilityRoleAttribute);
  if (SysNSStringToUTF8(role) != "AXWindow")
    return nil;

  NSString* window_title =
      AXAttributeValueOf(window, NSAccessibilityTitleAttribute);
  if (base::MatchPattern(SysNSStringToUTF8(window_title), pattern))
    return static_cast<AXUIElementRef>(window);

  return nil;
}

AX_EXPORT bool AXSuccess(AXError result, const std::string& message) {
  if (result == kAXErrorSuccess) {
    return true;
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
  LOG(WARNING) << message << ": " << error;
  return false;
}

}  // namespace ui

#pragma clang diagnostic pop
