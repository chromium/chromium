// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_FILTERED_GESTURE_PROVIDER_H_
#define UI_EVENTS_GESTURE_DETECTION_FILTERED_GESTURE_PROVIDER_H_

#include <stdint.h>
#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/events/gesture_detection/gesture_event_data_packet.h"
#include "ui/events/gesture_detection/gesture_provider.h"
#include "ui/events/gesture_detection/touch_disposition_gesture_filter.h"

namespace ui {

// Provides filtered gesture detection and dispatch given a sequence of touch
// events and touch event acks.
class GESTURE_DETECTION_EXPORT FilteredGestureProvider
    : public ui::TouchDispositionGestureFilterClient,
      public ui::GestureProviderClient {
 public:
  // |client| will be offered all gestures detected by the |gesture_provider_|
  // and allowed by the |gesture_filter_|.
  FilteredGestureProvider(const GestureProvider::Config& config,
                          GestureProviderClient* client);
  ~FilteredGestureProvider() final;

  void UpdateConfig(const GestureProvider::Config& config);

  struct TouchHandlingResult {
    TouchHandlingResult();

    // True if |event| was both valid and successfully handled by the
    // gesture provider. Otherwise, false, in which case the caller should drop
    // |event| and cease further propagation.
    bool succeeded;

    // Whether |event| occurred beyond the touch slop region.
    bool moved_beyond_slop_region;
  };
  TouchHandlingResult OnTouchEvent(const MotionEvent& event) WARN_UNUSED_RESULT;

  // To be called upon asynchronous and synchronous ack of an event that was
  // forwarded after a successful call to |OnTouchEvent()|.
  void OnTouchEventAck(uint32_t unique_event_id,
                       bool event_consumed,
                       bool is_source_touch_event_set_non_blocking);

  void ResetGestureHandlingState();

  // Methods delegated to |gesture_provider_|.
  void ResetDetection();
  void SetMultiTouchZoomSupportEnabled(bool enabled);
  void SetDoubleTapSupportForPlatformEnabled(bool enabled);
  void SetDoubleTapSupportForPageEnabled(bool enabled);
  const ui::MotionEvent* GetCurrentDownEvent() const;

 private:
  // GestureProviderClient implementation.
  void OnGestureEvent(const ui::GestureEventData& event) override;
  bool RequiresDoubleTapGestureEvents() const override;

  // TouchDispositionGestureFilterClient implementation.
  void ForwardGestureEvent(const ui::GestureEventData& event) override;

  GestureProviderClient* const client_;

  std::unique_ptr<ui::GestureProvider> gesture_provider_;
  ui::TouchDispositionGestureFilter gesture_filter_;

  bool handling_event_;
  bool any_touch_moved_beyond_slop_region_;
  ui::GestureEventDataPacket pending_gesture_packet_;

  DISALLOW_COPY_AND_ASSIGN(FilteredGestureProvider);
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_FILTERED_GESTURE_PROVIDER_H_
