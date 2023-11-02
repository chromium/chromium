// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_event_recorder_mac.h"

#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/accessibility/platform/ax_private_webkit_constants_mac.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_mac.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter_mac.h"

namespace ui {

// Callback function registered using AXObserverCreate.
static void EventReceivedThunk(AXObserverRef observer_ref,
                               AXUIElementRef element,
                               CFStringRef notification,
                               CFDictionaryRef user_info,
                               void* refcon) {
  AXEventRecorderMac* this_ptr = static_cast<AXEventRecorderMac*>(refcon);
  this_ptr->EventReceived(element, notification, user_info);
}

AXEventRecorderMac::AXEventRecorderMac(base::ProcessId pid,
                                       const AXTreeSelector& selector)
    : observer_run_loop_source_(nullptr) {
  AXUIElementRef node = nil;
  if (pid) {
    node = AXUIElementCreateApplication(pid);
    if (!node) {
      LOG(FATAL) << "Failed to get AXUIElement for pid " << pid;
    }
  } else {
    std::tie(node, pid) = FindAXUIElement(selector);
    if (!node) {
      LOG(FATAL) << "Failed to get AXUIElement for selector";
    }
  }

  if (kAXErrorSuccess !=
      AXObserverCreateWithInfoCallback(pid, EventReceivedThunk,
                                       observer_ref_.InitializeInto())) {
    LOG(FATAL) << "Failed to create AXObserverRef";
  }

  // Get an AXUIElement for the Chrome application.
  application_.reset(node);
  if (!application_.get())
    LOG(FATAL) << "Failed to create AXUIElement for application.";

  // Add the notifications we care about to the observer.
  static NSArray* notifications = [@[
    @"AXAutocorrectionOccurred",
    @"AXElementBusyChanged",
    @"AXExpandedChanged",
    @"AXInvalidStatusChanged",
    @"AXLiveRegionChanged",
    @"AXLiveRegionCreated",
    @"AXLoadComplete",
    @"AXMenuItemSelected",
    (NSString*)kAXMenuClosedNotification,
    (NSString*)kAXMenuOpenedNotification,
    NSAccessibilityAnnouncementRequestedNotification,
    NSAccessibilityApplicationActivatedNotification,
    NSAccessibilityApplicationDeactivatedNotification,
    NSAccessibilityApplicationHiddenNotification,
    NSAccessibilityApplicationShownNotification,
    NSAccessibilityCreatedNotification,
    NSAccessibilityDrawerCreatedNotification,
    NSAccessibilityFocusedUIElementChangedNotification,
    NSAccessibilityFocusedWindowChangedNotification,
    NSAccessibilityHelpTagCreatedNotification,
    NSAccessibilityLayoutChangedNotification,
    NSAccessibilityMainWindowChangedNotification,
    NSAccessibilityMovedNotification,
    NSAccessibilityResizedNotification,
    NSAccessibilityRowCollapsedNotification,
    NSAccessibilityRowCountChangedNotification,
    NSAccessibilityRowExpandedNotification,
    NSAccessibilitySelectedCellsChangedNotification,
    NSAccessibilitySelectedChildrenChangedNotification,
    NSAccessibilitySelectedChildrenMovedNotification,
    NSAccessibilitySelectedColumnsChangedNotification,
    NSAccessibilitySelectedRowsChangedNotification,
    NSAccessibilitySelectedTextChangedNotification,
    NSAccessibilitySheetCreatedNotification,
    NSAccessibilityTitleChangedNotification,
    NSAccessibilityUIElementDestroyedNotification,
    NSAccessibilityUnitsChangedNotification,
    NSAccessibilityValueChangedNotification,
    NSAccessibilityWindowCreatedNotification,
    NSAccessibilityWindowDeminiaturizedNotification,
    NSAccessibilityWindowMiniaturizedNotification,
    NSAccessibilityWindowMovedNotification,
    NSAccessibilityWindowResizedNotification,
  ] retain];

  for (NSString* notification : notifications) {
    AddNotification(notification);
  }

  // Add the observer to the current message loop.
  observer_run_loop_source_ = AXObserverGetRunLoopSource(observer_ref_.get());
  CFRunLoopAddSource(CFRunLoopGetCurrent(), observer_run_loop_source_,
                     kCFRunLoopDefaultMode);
}

AXEventRecorderMac::~AXEventRecorderMac() {
  CFRunLoopRemoveSource(CFRunLoopGetCurrent(), observer_run_loop_source_,
                        kCFRunLoopDefaultMode);
}

void AXEventRecorderMac::AddNotification(NSString* notification) {
  AXObserverAddNotification(observer_ref_, application_,
                            base::mac::NSToCFCast(notification), this);
}

void AXEventRecorderMac::EventReceived(AXUIElementRef element,
                                       CFStringRef notification,
                                       CFDictionaryRef user_info) {
  std::string notification_str = base::SysCFStringRefToUTF8(notification);

  auto formatter = ui::AXTreeFormatterMac();
  formatter.SetPropertyFilters(property_filters_,
                               AXTreeFormatter::kFiltersDefaultSet);

  std::string element_str =
      formatter.FormatTree(formatter.BuildNode(static_cast<id>(element)));

  // Element dumps contain a new line character at the end, remove it.
  if (!element_str.empty() && element_str.back() == '\n') {
    element_str.pop_back();
  }

  std::string log = base::StringPrintf("%s on %s", notification_str.c_str(),
                                       element_str.c_str());

  if (notification_str ==
      base::SysNSStringToUTF8(NSAccessibilitySelectedTextChangedNotification))
    log += " " + SerializeTextSelectionChangedProperties(user_info);

  OnEvent(log);
}

std::string AXEventRecorderMac::SerializeTextSelectionChangedProperties(
    CFDictionaryRef user_info) {
  if (user_info == nil)
    return {};

  std::vector<std::string> serialized_info;
  CFDictionaryApplyFunction(
      user_info,
      [](const void* raw_key, const void* raw_value, void* context) {
        auto* key = static_cast<NSString*>(raw_key);
        auto* value = static_cast<NSObject*>(raw_value);
        auto* serialized_info = static_cast<std::vector<std::string>*>(context);
        std::string value_string;
        if ([key isEqual:NSAccessibilityTextStateChangeTypeKey]) {
          value_string = ToString(static_cast<AXTextStateChangeType>(
              [static_cast<NSNumber*>(value) intValue]));
        } else if ([key isEqual:NSAccessibilityTextSelectionDirection]) {
          value_string = ToString(static_cast<AXTextSelectionDirection>(
              [static_cast<NSNumber*>(value) intValue]));
        } else if ([key isEqual:NSAccessibilityTextSelectionGranularity]) {
          value_string = ToString(static_cast<AXTextSelectionGranularity>(
              [static_cast<NSNumber*>(value) intValue]));
        } else if ([key isEqual:NSAccessibilityTextEditType]) {
          value_string = ToString(static_cast<AXTextEditType>(
              [static_cast<NSNumber*>(value) intValue]));
        } else {
          return;
        }
        serialized_info->push_back(base::SysNSStringToUTF8(key) + "=" +
                                   value_string);
      },
      &serialized_info);

  // Always sort the info so that we don't depend on CFDictionary for
  // consistent output ordering.
  std::sort(serialized_info.begin(), serialized_info.end());

  return base::JoinString(serialized_info, " ");
}

}  // namespace ui
