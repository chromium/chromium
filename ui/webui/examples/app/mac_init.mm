// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/app/mac_init.h"

#include "base/observer_list.h"

#import "base/mac/scoped_sending_event.h"
#import "base/message_loop/message_pump_apple.h"
#import "content/public/browser/native_event_processor_mac.h"
#import "content/public/browser/native_event_processor_observer_mac.h"

@interface ExamplesApplication
    : NSApplication <CrAppProtocol, CrAppControlProtocol, NativeEventProcessor>
@end

@implementation ExamplesApplication {
  base::ObserverList<content::NativeEventProcessorObserver>::Unchecked
      _observers;
  BOOL _handlingSendEvent;
}

- (BOOL)isHandlingSendEvent {
  return _handlingSendEvent;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  _handlingSendEvent = handlingSendEvent;
}

- (void)sendEvent:(NSEvent*)event {
  base::mac::ScopedSendingEvent sendingEventScoper;
  content::ScopedNotifyNativeEventProcessorObserver scopedObserverNotifier(
      &self->_observers, event);
  [super sendEvent:event];
}

- (void)addNativeEventProcessorObserver:
    (content::NativeEventProcessorObserver*)observer {
  _observers.AddObserver(observer);
}

- (void)removeNativeEventProcessorObserver:
    (content::NativeEventProcessorObserver*)observer {
  _observers.RemoveObserver(observer);
}

@end

namespace webui_examples {

void MacPreBrowserMain() {
  CHECK_EQ(NSApp, nil);
  [ExamplesApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}

}  // namespace webui_examples
