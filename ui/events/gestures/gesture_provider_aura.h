// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURES_GESTURE_PROVIDER_AURA_H_
#define UI_EVENTS_GESTURES_GESTURE_PROVIDER_AURA_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/events/event.h"
#include "ui/events/events_export.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"
#include "ui/events/gesture_detection/gesture_event_data_packet.h"
#include "ui/events/gesture_detection/touch_disposition_gesture_filter.h"
#include "ui/events/gestures/motion_event_aura.h"

namespace ui {

class GestureProviderAura;

class EVENTS_EXPORT GestureProviderAuraClient {
 public:
  virtual ~GestureProviderAuraClient() {}
  virtual void OnGestureEvent(GestureConsumer* consumer,
                              GestureEvent* event) = 0;

  // Called when `gesture_provider` will be destroyed.
  virtual void OnGestureProviderAuraWillBeDestroyed(
      GestureProviderAura* gesture_provider) {}
};

// Provides gesture detection and dispatch given a sequence of touch events
// and touch event acks.
class EVENTS_EXPORT GestureProviderAura : public GestureProviderClient {
 public:
  GestureProviderAura(GestureConsumer* consumer,
                      GestureProviderAuraClient* client);

  GestureProviderAura(const GestureProviderAura&) = delete;
  GestureProviderAura& operator=(const GestureProviderAura&) = delete;

  ~GestureProviderAura() override;

  void set_gesture_consumer(GestureConsumer* consumer) {
    gesture_consumer_ = consumer;
  }

  FilteredGestureProvider& filtered_gesture_provider() {
    return filtered_gesture_provider_;
  }

  bool OnTouchEvent(TouchEvent* event);
  void OnTouchEventAck(uint32_t unique_touch_event_id,
                       bool event_consumed,
                       bool is_source_touch_event_set_blocking);
  const MotionEventAura& pointer_state() { return pointer_state_; }
  std::vector<std::unique_ptr<GestureEvent>> GetAndResetPendingGestures();
  void OnTouchEnter(int pointer_id, float x, float y);

  void ResetGestureHandlingState();

  // Synthesizes gesture end events and sends to the associated consumer.
  void SendSynthesizedEndEvents();

  // GestureProviderClient implementation
  void OnGestureEvent(const GestureEventData& gesture) override;
  bool RequiresDoubleTapGestureEvents() const override;

 private:
  raw_ptr<GestureProviderAuraClient> client_;
  MotionEventAura pointer_state_;
  FilteredGestureProvider filtered_gesture_provider_;

  bool handling_event_;
  std::vector<std::unique_ptr<GestureEvent>> pending_gestures_;

  // The |gesture_consumer_| owns this provider.
  raw_ptr<GestureConsumer> gesture_consumer_;
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURES_GESTURE_PROVIDER_AURA_H_
