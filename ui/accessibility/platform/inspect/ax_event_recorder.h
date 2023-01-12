// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/synchronization/lock.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace ui {

using AXEventCallback = base::RepeatingCallback<void(const std::string&)>;

// Listens for native accessibility events fired by a given
// BrowserAccessibilityManager and saves human-readable log strings for
// each event fired to a vector. Construct an instance of this class to
// begin listening, call GetEventLogs() to get all of the logs so far, and
// destroy it to stop listening.
//
// A log string should be of the form "<event> on <node>" where <event> is
// the name of the event fired (platform-specific) and <node> is information
// about the accessibility node on which the event was fired, for example its
// role and name.
//
// The implementation is highly platform-specific; a subclass is needed for
// each platform does most of the work.
//
// As currently designed, there should only be one instance of this class.
class COMPONENT_EXPORT(AX_PLATFORM) AXEventRecorder {
 public:
  AXEventRecorder();

  AXEventRecorder(const AXEventRecorder&) = delete;
  AXEventRecorder& operator=(const AXEventRecorder&) = delete;

  virtual ~AXEventRecorder();

  // Scopes/unscopes events to a web area.
  void SetOnlyWebEvents(bool only_web_events) {
    only_web_events_ = only_web_events;
  }

  // Set filters that specify which properties of event targets should be
  // dumped.
  void SetPropertyFilters(
      const std::vector<AXPropertyFilter>& property_filters) {
    property_filters_ = property_filters;
  }

  // Sets a callback which will be called on every event fired.
  void ListenToEvents(AXEventCallback callback) {
    callback_ = std::move(callback);
  }

  // Stop listenting to events.
  void StopListeningToEvents();

  // Called to ensure the event recorder has finished recording async events.
  virtual void WaitForDoneRecording() {}

  // Access the vector of human-readable event logs, one string per event.
  const std::vector<std::string> GetEventLogs() const;

 protected:
  // Called by a derived class which implements platform event handling on
  // every fired event.
  void OnEvent(const std::string& event);

  bool only_web_events_ = false;
  std::vector<AXPropertyFilter> property_filters_;

 private:
  mutable base::Lock on_event_lock_;
  std::vector<std::string> event_logs_;
  AXEventCallback callback_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_EVENT_RECORDER_H_
