// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/component_export.h"
#include "base/process/process_handle.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace ui {

// Implementation of AXEventRecorder that uses AXObserver to watch for
// NSAccessibility events.
class COMPONENT_EXPORT(AX_PLATFORM) AXEventRecorderMac
    : public AXEventRecorder {
 public:
  AXEventRecorderMac(base::ProcessId pid, const AXTreeSelector& selector);

  AXEventRecorderMac(const AXEventRecorderMac&) = delete;
  AXEventRecorderMac& operator=(const AXEventRecorderMac&) = delete;

  ~AXEventRecorderMac() override;

  // Callback executed every time we receive an event notification.
  void EventReceived(AXUIElementRef element,
                     CFStringRef notification,
                     CFDictionaryRef user_info);
  static std::string SerializeTextSelectionChangedProperties(
      CFDictionaryRef user_info);

 private:
  // Add one notification to the list of notifications monitored by our
  // observer.
  void AddNotification(NSString* notification);

  // The AXUIElement for the application.
  base::apple::ScopedCFTypeRef<AXUIElementRef> application_;

  // The AXObserver we use to monitor AX notifications.
  base::apple::ScopedCFTypeRef<AXObserverRef> observer_ref_;
  CFRunLoopSourceRef observer_run_loop_source_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_MAC_H_
