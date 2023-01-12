// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_NATIVE_WIDGET_MAC_EVENT_MONITOR_H_
#define UI_VIEWS_COCOA_NATIVE_WIDGET_MAC_EVENT_MONITOR_H_

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/views_export.h"

namespace ui {
class Event;
}  // namespace ui

namespace views {

class NativeWidgetMacNSWindowHost;

// A class for event monitoring. This will create a NSEvent local monitor in
// for this widget (in this process or in a remote process). The monitor will
// call back through the Client interface.
class VIEWS_EXPORT NativeWidgetMacEventMonitor {
 public:
  class Client {
   public:
    // Called for every observed NSEvent. If this client handles the event,
    // it should set `event_handled` to true. The initial value of
    // `event_handled` will be true if another client handled this event.
    virtual void NativeWidgetMacEventMonitorOnEvent(ui::Event* event,
                                                    bool* event_handled) = 0;
  };
  ~NativeWidgetMacEventMonitor();

 private:
  friend class NativeWidgetMacNSWindowHost;
  explicit NativeWidgetMacEventMonitor(Client* client);

  const raw_ptr<Client> client_;

  // Scoped closure runner that will unregister `this` from its
  // NativeWidgetMacNSWindowHost when `this` is destroyed.
  base::ScopedClosureRunner remove_closure_runner_;
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_NATIVE_WIDGET_MAC_EVENT_MONITOR_H_
