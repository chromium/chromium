// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_inspect_utils_mac.h"

#include <CoreGraphics/CoreGraphics.h>

#include <ostream>

#include "base/apple/bridging.h"
#include "base/containers/fixed_flat_set.h"
#include "base/debug/stack_trace.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/memory/scoped_policy.h"
#include "base/strings/pattern.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/accessibility/platform/ax_private_attributes_mac.h"
#include "ui/accessibility/platform/inspect/ax_element_wrapper_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// error: 'accessibilityAttributeNames' is deprecated: first deprecated in
// macOS 10.10 - Use the NSAccessibility protocol methods instead (see
// NSAccessibilityProtocols.h
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace ui {

namespace {

const char kChromeTitle[] = "Google Chrome";
const char kChromiumTitle[] = "Chromium";
const char kFirefoxTitle[] = "Firefox";
const char kSafariTitle[] = "Safari";

NSArray* AXChildrenOf(const id node) {
  return AXElementWrapper(node).Children();
}

}  // namespace

bool IsValidAXAttribute(const std::string& attribute) {
  static NSSet<NSString*>* valid_attributes = [NSSet setWithArray:@[
    NSAccessibilityAccessKeyAttribute,
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
    NSAccessibilityBrailleLabelAttribute,
    NSAccessibilityBrailleRoleDescription,
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
    NSAccessibilityRequiredAttribute,
    NSAccessibilityRoleDescriptionAttribute,
    NSAccessibilitySelectedAttribute,
    NSAccessibilitySizeAttribute,
    NSAccessibilityTitleAttribute,
    NSAccessibilityTitleUIElementAttribute,
    NSAccessibilityURLAttribute,
    NSAccessibilityVisitedAttribute,
  ]];

  return [valid_attributes containsObject:base::SysUTF8ToNSString(attribute)];
}

base::ScopedCFTypeRef<AXUIElementRef> FindAXUIElement(
    const AXUIElementRef node,
    const AXFindCriteria& criteria) {
  if (criteria.Run(node)) {
    return base::ScopedCFTypeRef<AXUIElementRef>(node,
                                                 base::scoped_policy::RETAIN);
  }

  NSArray* children = AXChildrenOf((__bridge id)node);
  for (id child in children) {
    base::ScopedCFTypeRef<AXUIElementRef> found =
        FindAXUIElement((__bridge AXUIElementRef)child, criteria);
    if (found) {
      return found;
    }
  }

  return base::ScopedCFTypeRef<AXUIElementRef>();
}

std::pair<base::ScopedCFTypeRef<AXUIElementRef>, int> FindAXUIElement(
    const AXTreeSelector& selector) {
  if (selector.widget) {
    return {base::ScopedCFTypeRef<AXUIElementRef>(
                AXUIElementCreateApplication(selector.widget)),
            selector.widget};
  }

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
    return {base::ScopedCFTypeRef<AXUIElementRef>(), 0};

  NSArray* windows =
      base::apple::CFToNSOwnershipCast(CGWindowListCopyWindowInfo(
          kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
          kCGNullWindowID));

  for (NSDictionary* window_info in windows) {
    int pid = base::mac::ObjCCast<NSNumber>(window_info[@"kCGWindowOwnerPID"])
                  .intValue;
    std::string window_name = base::SysNSStringToUTF8(
        base::mac::ObjCCast<NSString>(window_info[@"kCGWindowOwnerName"]));

    base::ScopedCFTypeRef<AXUIElementRef> node;

    // Application pre-defined selectors match or application title exact match.
    bool app_title_match = window_name == selector.pattern;
    if (window_name == title || app_title_match) {
      node.reset(AXUIElementCreateApplication(pid));
    }

    // Window title match. Application contain an AXWindow accessible object as
    // a first child, which accessible name contain a window title. For example:
    // 'Inbox (2) - asurkov@igalia.com - Gmail'.
    if (!selector.pattern.empty() && !app_title_match) {
      if (!node) {
        node.reset(AXUIElementCreateApplication(pid));
      }

      base::ScopedCFTypeRef<AXUIElementRef> window =
          FindAXWindowChild(node, selector.pattern);
      if (window) {
        node = window;
      }
    }

    // ActiveTab selector.
    if (node && selector.types & AXTreeSelector::ActiveTab) {
      node = FindAXUIElement(
          node, base::BindRepeating([](const AXUIElementRef node) {
            // Only active tab in exposed in browsers, thus find first
            // AXWebArea role.
            AXElementWrapper ax_node((__bridge id)node);
            NSString* role =
                *ax_node.GetAttributeValue(NSAccessibilityRoleAttribute);
            return base::SysNSStringToUTF8(role) == "AXWebArea";
          }));
    }

    // Found a match.
    if (node)
      return {node, pid};
  }
  return {base::ScopedCFTypeRef<AXUIElementRef>(), 0};
}

base::ScopedCFTypeRef<AXUIElementRef> FindAXWindowChild(
    AXUIElementRef parent,
    const std::string& pattern) {
  NSArray* children = AXChildrenOf((__bridge id)parent);
  if (children.count == 0) {
    return base::ScopedCFTypeRef<AXUIElementRef>();
  }

  id window = children.firstObject;

  AXElementWrapper ax_window(window);
  NSString* role = *ax_window.GetAttributeValue(NSAccessibilityRoleAttribute);
  if (base::SysNSStringToUTF8(role) != "AXWindow") {
    return base::ScopedCFTypeRef<AXUIElementRef>();
  }

  NSString* window_title =
      *ax_window.GetAttributeValue(NSAccessibilityTitleAttribute);
  if (base::MatchPattern(base::SysNSStringToUTF8(window_title), pattern)) {
    return base::ScopedCFTypeRef<AXUIElementRef>(
        (__bridge AXUIElementRef)window, base::scoped_policy::RETAIN);
  }

  return base::ScopedCFTypeRef<AXUIElementRef>();
}

}  // namespace ui

#pragma clang diagnostic pop
