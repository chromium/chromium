// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_event_recorder_mac.h"

#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <string>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"
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

AXEventRecorderMac::AXEventRecorderMac(
    base::WeakPtr<AXPlatformTreeManager> manager,
    base::ProcessId pid,
    const AXTreeSelector& selector)
    : manager_(manager), observer_run_loop_source_(nullptr) {
  base::apple::ScopedCFTypeRef<AXUIElementRef> node;
  if (pid) {
    node.reset(AXUIElementCreateApplication(pid));
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
  application_ = std::move(node);
  if (!application_.get())
    LOG(FATAL) << "Failed to create AXUIElement for application.";

  // Add the notifications we care about to the observer.
  static NSArray* notifications = @[
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
  ];

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
  AXObserverAddNotification(observer_ref_.get(), application_.get(),
                            base::apple::NSToCFPtrCast(notification), this);
}

void AXEventRecorderMac::EventReceived(AXUIElementRef element,
                                       CFStringRef notification,
                                       CFDictionaryRef user_info) {
  std::string notification_str = base::SysCFStringRefToUTF8(notification);

  if (notification_str == "AXApplicationDeactivated") {
    // The application deactivated event is used as an end-of-test signal
    // because it never occurs in tests.
    has_seen_end_of_test_sentinel_ = true;
    if (end_of_test_loop_runner_) {
      end_of_test_loop_runner_->Quit();
    }
    return;
  }

  auto formatter = AXTreeFormatterMac();
  formatter.SetPropertyFilters(property_filters_,
                               AXTreeFormatter::kFiltersDefaultSet);

  std::string element_str =
      formatter.FormatTree(formatter.BuildNode((__bridge id)element));

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
  if (user_info == nil) {
    return {};
  }

  NSDictionary* ns_user_info = base::apple::CFToNSPtrCast(user_info);
  std::vector<std::string> serialized_info;
  for (NSString* key in ns_user_info) {
    NSNumber* value = base::apple::ObjCCast<NSNumber>(ns_user_info[key]);
    std::string value_string;
    if ([key isEqual:NSAccessibilityTextStateChangeTypeKey]) {
      value_string =
          ToString(static_cast<AXTextStateChangeType>(value.intValue));
    } else if ([key isEqual:NSAccessibilityTextSelectionDirection]) {
      value_string =
          ToString(static_cast<AXTextSelectionDirection>(value.intValue));
    } else if ([key isEqual:NSAccessibilityTextSelectionGranularity]) {
      value_string =
          ToString(static_cast<AXTextSelectionGranularity>(value.intValue));
    } else if ([key isEqual:NSAccessibilityTextEditType]) {
      value_string = ToString(static_cast<AXTextEditType>(value.intValue));
    } else {
      continue;
    }
    serialized_info.push_back(base::SysNSStringToUTF8(key) + "=" +
                              value_string);
  }

  // Always sort the info so that we don't depend on CFDictionary for
  // consistent output ordering.
  std::sort(serialized_info.begin(), serialized_info.end());

  return base::JoinString(serialized_info, " ");
}

void AXEventRecorderMac::WaitForDoneRecording() {
  if (!manager_) {
    return;
  }
  manager_->FireSentinelEventForTesting();  // IN-TEST
  if (has_seen_end_of_test_sentinel_) {
    return;
  }

  end_of_test_loop_runner_ = std::make_unique<base::RunLoop>();
  end_of_test_loop_runner_->Run();
}

}  // namespace ui
