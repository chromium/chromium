// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_inspect_utils_mac.h"

#include <ostream>

#include "base/containers/fixed_flat_set.h"
#include "base/debug/stack_trace.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/pattern.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/accessibility/platform/ax_private_attributes_mac.h"
#include "ui/accessibility/platform/inspect/ax_element_wrapper_mac.h"

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
       NSAccessibilityVisitedAttribute},
      NSStringComparator());

  return kValidAttributes.contains(base::SysUTF8ToNSString(attribute));
}

NSArray* AXChildrenOf(const id node) {
  return AXElementWrapper(node).Children();
}

std::string GetDOMId(const id node) {
  return AXElementWrapper(node).DOMId();
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
            AXElementWrapper ax_node(static_cast<id>(node));
            NSString* role =
                *ax_node.GetAttributeValue(NSAccessibilityRoleAttribute);
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

  AXElementWrapper ax_window(window);
  NSString* role = *ax_window.GetAttributeValue(NSAccessibilityRoleAttribute);
  if (SysNSStringToUTF8(role) != "AXWindow")
    return nil;

  NSString* window_title =
      *ax_window.GetAttributeValue(NSAccessibilityTitleAttribute);
  if (base::MatchPattern(SysNSStringToUTF8(window_title), pattern))
    return static_cast<AXUIElementRef>(window);

  return nil;
}

}  // namespace ui

#pragma clang diagnostic pop
