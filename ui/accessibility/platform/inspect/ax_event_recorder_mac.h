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

namespace base {
class RunLoop;
}  // namespace base

namespace ui {

class AXPlatformTreeManager;

// Implementation of AXEventRecorder that uses AXObserver to watch for
// NSAccessibility events.
class COMPONENT_EXPORT(AX_PLATFORM) AXEventRecorderMac
    : public AXEventRecorder {
 public:
  // If `scope_window` is non-nil, recording is restricted to events whose
  // target element belongs to `scope_window` or to one of its descendant child
  // windows (e.g. menus, bubbles and tooltips anchored to it). Events
  // targeting elements in any other window of the application are dropped.
  // This lets a test observe a single widget without picking up unrelated
  // events from, e.g., a browser window that happens to be open in the same
  // process. The scope is fixed for the lifetime of the recorder: the
  // accessibility markers it applies to out-of-scope windows are removed in
  // the destructor. If `scope_window` is nil, events from all windows are
  // recorded.
  AXEventRecorderMac(base::WeakPtr<AXPlatformTreeManager> manager,
                     base::ProcessId pid,
                     const AXTreeSelector& selector,
                     NSWindow* scope_window = nil);

  AXEventRecorderMac(const AXEventRecorderMac&) = delete;
  AXEventRecorderMac& operator=(const AXEventRecorderMac&) = delete;

  ~AXEventRecorderMac() override;

  // Callback executed every time we receive an event notification.
  void EventReceived(AXUIElementRef element,
                     CFStringRef notification,
                     CFDictionaryRef user_info);
  static std::string SerializeTextSelectionChangedProperties(
      CFDictionaryRef user_info);

  void WaitForDoneRecording() override;

 private:
  // Add one notification to the list of notifications monitored by our
  // observer.
  void AddNotification(NSString* notification);

  // Returns true if `element` should be recorded given the current window
  // scope. See the constructor.
  bool IsElementInWindowScope(AXUIElementRef element);

  // Tags every window of the application that is outside the current window
  // scope with a marker accessibility identifier, so that an event target's
  // window can be classified through the accessibility API.
  void UpdateWindowScopeMarkers();

  base::WeakPtr<AXPlatformTreeManager> manager_;

  // The window to which recording is restricted, if any. Fixed at
  // construction.
  NSWindow* const __strong scope_window_;

  // The AXUIElement for the application.
  base::apple::ScopedCFTypeRef<AXUIElementRef> application_;

  // The AXObserver we use to monitor AX notifications.
  base::apple::ScopedCFTypeRef<AXObserverRef> observer_ref_;
  CFRunLoopSourceRef observer_run_loop_source_;

  bool has_seen_end_of_test_sentinel_ = false;
  std::unique_ptr<base::RunLoop> end_of_test_loop_runner_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_MAC_H_
