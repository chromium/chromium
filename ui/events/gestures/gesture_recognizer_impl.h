// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_IMPL_H_
#define UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/event_constants.h"
#include "ui/events/events_export.h"
#include "ui/events/gestures/gesture_provider_aura.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"

namespace aura::test {
FORWARD_DECLARE_TEST(GestureRecognizerTest,
                     DestroyGestureProviderAuraBeforeAck);
FORWARD_DECLARE_TEST(GestureRecognizerTest,
                     ResetGestureRecognizerWithGestureProvider);
}  // namespace aura::test

namespace ui {
class GestureConsumer;
class GestureEvent;
class GestureEventHelper;
class TouchEvent;

// TODO(tdresser): Once the unified gesture recognition process sticks
// (crbug.com/332418), GestureRecognizerImpl can be cleaned up
// significantly.
class EVENTS_EXPORT GestureRecognizerImpl : public GestureRecognizer,
                                            public GestureProviderAuraClient {
 public:
  typedef std::map<int, raw_ptr<GestureConsumer, CtnExperimental>>
      TouchIdToConsumerMap;

  GestureRecognizerImpl();

  GestureRecognizerImpl(const GestureRecognizerImpl&) = delete;
  GestureRecognizerImpl& operator=(const GestureRecognizerImpl&) = delete;

  ~GestureRecognizerImpl() override;

  std::vector<raw_ptr<GestureEventHelper, VectorExperimental>>& helpers() {
    return helpers_;
  }

  // Returns a list of events of type |type|, one for each pointer down on
  // |consumer|. Event locations are pulled from the active pointers.
  std::vector<std::unique_ptr<TouchEvent>> GetEventPerPointForConsumer(
      GestureConsumer* consumer,
      EventType type);

  // Overridden from GestureRecognizer
  GestureConsumer* GetTouchLockedTarget(const TouchEvent& event) override;
  GestureConsumer* GetTargetForLocation(const gfx::PointF& location,
                                        int source_device_id) override;
  void CancelActiveTouchesExcept(GestureConsumer* not_cancelled) override;
  void CancelActiveTouchesOn(
      const std::vector<GestureConsumer*>& consumers) override;
  void TransferEventsTo(
      GestureConsumer* current_consumer,
      GestureConsumer* new_consumer,
      TransferTouchesBehavior transfer_touches_behavior) override;
  bool GetLastTouchPointForTarget(GestureConsumer* consumer,
                                  gfx::PointF* point) override;
  bool CancelActiveTouches(GestureConsumer* consumer) override;

 protected:
  virtual GestureProviderAura* GetGestureProviderForConsumer(
      GestureConsumer* c);

  // Overridden from GestureRecognizer
  bool ProcessTouchEventPreDispatch(TouchEvent* event,
                                    GestureConsumer* consumer) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(aura::test::GestureRecognizerTest,
                           DestroyGestureProviderAuraBeforeAck);
  FRIEND_TEST_ALL_PREFIXES(aura::test::GestureRecognizerTest,
                           ResetGestureRecognizerWithGestureProvider);

  // Sets up the target consumer for gestures based on the touch-event.
  void SetupTargets(const TouchEvent& event, GestureConsumer* consumer);

  void DispatchGestureEvent(GestureConsumer* raw_input_consumer,
                            GestureEvent* event);

  Gestures AckTouchEvent(uint32_t unique_event_id,
                         ui::EventResult result,
                         bool is_source_touch_event_set_blocking,
                         GestureConsumer* consumer) override;

  void CancelActiveTouchesExceptImpl(GestureConsumer* not_cancelled);
  bool CancelActiveTouchesImpl(GestureConsumer* consumer);

  bool CleanupStateForConsumer(GestureConsumer* consumer) override;
  void AddGestureEventHelper(GestureEventHelper* helper) override;
  void RemoveGestureEventHelper(GestureEventHelper* helper) override;
  bool DoesConsumerHaveActiveTouch(GestureConsumer* consumer) const override;
  void SendSynthesizedEndEvents(GestureConsumer* consumer) override;

  // Overridden from GestureProviderAuraClient
  void OnGestureEvent(GestureConsumer* raw_input_consumer,
                      GestureEvent* event) override;
  void OnGestureProviderAuraWillBeDestroyed(
      GestureProviderAura* gesture_provider) override;

  // Convenience method to find the GestureEventHelper that can dispatch events
  // to a specific |consumer|.
  GestureEventHelper* FindDispatchHelperForConsumer(GestureConsumer* consumer);
  std::set<raw_ptr<GestureConsumer, SetExperimental>> consumers_;

  // Maps an event via its |unique_event_id| to the corresponding gesture
  // provider. This avoids any invalid reference while routing ACKs for events
  // that may arise post |TransferEventsTo()| function call.
  // See http://crbug.com/698843 for more info.
  std::map<uint32_t, raw_ptr<GestureProviderAura, CtnExperimental>>
      event_to_gesture_provider_;

  // |touch_id_target_| maps a touch-id to its target window.
  // touch-ids are removed from |touch_id_target_| on
  // EventType::kTouchRelease and EventType::kTouchCancel.
  TouchIdToConsumerMap touch_id_target_;

  std::vector<raw_ptr<GestureEventHelper, VectorExperimental>> helpers_;
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_IMPL_H_
