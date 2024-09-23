// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/gesture_recognizer_impl.h"

#include <stddef.h>

#include <limits>
#include <memory>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_switches.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gestures/gesture_provider_aura.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/events/types/event_type.h"

namespace ui {

namespace {

void TransferConsumer(
    GestureConsumer* current_consumer,
    GestureConsumer* new_consumer,
    std::set<raw_ptr<GestureConsumer, SetExperimental>>& consumers) {
  consumers.erase(current_consumer);
  if (!new_consumer) {
    current_consumer->reset_gesture_provider();
    return;
  }
  new_consumer->set_gesture_provider(current_consumer->TakeProvider());
  if (new_consumer->provider()) {
    consumers.insert(new_consumer);
  } else {
    consumers.erase(new_consumer);
  }
}

// Generic function to remove every entry from a map having the given value.
template <class Key, class T, class Value>
bool RemoveValueFromMap(std::map<Key, T>* map, const Value& value) {
  bool removed = false;
  // This is a bandaid fix for crbug/732232 that requires further investigation.
  if (!map || map->empty())
    return removed;
  for (auto i = map->begin(); i != map->end();) {
    if (i->second == value) {
      map->erase(i++);
      removed = true;
    } else {
      ++i;
    }
  }
  return removed;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerImpl, public:

GestureRecognizerImpl::GestureRecognizerImpl() = default;

GestureRecognizerImpl::~GestureRecognizerImpl() {
  // The gesture recognizer impl observes the gesture providers that are owned
  // by `consumers_`. Clear `consumers`' providers
  // explicitly so that the notifications sent by gesture providers during
  // destruction are handled properly.
  for (GestureConsumer* consumer : consumers_) {
    consumer->reset_gesture_provider();
  }
  consumers_.clear();
}

// Checks if this finger is already down, if so, returns the current target.
// Otherwise, returns nullptr.
GestureConsumer* GestureRecognizerImpl::GetTouchLockedTarget(
    const TouchEvent& event) {
  return touch_id_target_[event.pointer_details().id];
}

GestureConsumer* GestureRecognizerImpl::GetTargetForLocation(
    const gfx::PointF& location,
    int source_device_id) {
  const float max_distance =
      GestureConfiguration::GetInstance()
          ->max_separation_for_gesture_touches_in_pixels();

  gfx::PointF closest_point;
  int closest_touch_id = 0;
  double closest_distance_squared = std::numeric_limits<double>::infinity();

  for (GestureConsumer* consumer : consumers_) {
    GestureProviderAura* provider = consumer->provider();
    const MotionEventAura& pointer_state = provider->pointer_state();
    for (size_t j = 0; j < pointer_state.GetPointerCount(); ++j) {
      if (source_device_id != pointer_state.GetSourceDeviceId(j))
        continue;
      gfx::PointF point(pointer_state.GetX(j), pointer_state.GetY(j));
      // Relative distance is all we need here, so LengthSquared() is
      // appropriate, and cheaper than Length().
      double distance_squared = (point - location).LengthSquared();
      if (distance_squared < closest_distance_squared) {
        closest_point = point;
        closest_touch_id = pointer_state.GetPointerId(j);
        closest_distance_squared = distance_squared;
      }
    }
  }

  if (closest_distance_squared < max_distance * max_distance)
    return touch_id_target_[closest_touch_id];
  return nullptr;
}

void GestureRecognizerImpl::CancelActiveTouchesExcept(
    GestureConsumer* not_cancelled) {
  CancelActiveTouchesExceptImpl(not_cancelled);
}

void GestureRecognizerImpl::CancelActiveTouchesOn(
    const std::vector<GestureConsumer*>& consumers) {
  for (auto* consumer : consumers) {
    if (std::find(consumers_.begin(), consumers_.end(), consumer) !=
        consumers_.end()) {
      CancelActiveTouchesImpl(consumer);
    }
  }
}

void GestureRecognizerImpl::TransferEventsTo(
    GestureConsumer* current_consumer,
    GestureConsumer* new_consumer,
    TransferTouchesBehavior transfer_touches_behavior) {
  // This method transfers the gesture stream from |current_consumer| to
  // |new_consumer|. If |transfer_touches_behavior| is kCancel, it ensures that
  // both consumers retain a touch event stream which is reasonably valid. In
  // order to do this we
  // - record what pointers are currently down on |current_consumer|
  // - cancel touches on consumers other than |current_consumer|
  // - move the gesture provider from |current_consumer| to |new_consumer|
  // - if |transfer_touches_behavior| is kCancel
  //     - synchronize the state of the new gesture provider associated with
  //       current_consumer with with the touch state of the consumer itself via
  //       OnTouchEnter.
  //     - synthesize touch cancels on |current_consumer|.
  // - retarget the pointers that were previously targeted to
  //   |current_consumer| to |new_consumer|.
  // NOTE: This currently doesn't synthesize touch press events on
  // |new_consumer|, so the event stream it sees is still invalid.
  DCHECK(current_consumer);
  DCHECK(new_consumer);
  // The new consumer can be deleted while canceling active touches.
  // See ash_unittests DragDropControllerTest.TabletSplitViewDragTwoBrowserTabs
  // for an example where this happens.
  base::WeakPtr<GestureConsumer> new_consumer_ptr = new_consumer->GetWeakPtr();
  GestureEventHelper* helper = FindDispatchHelperForConsumer(current_consumer);

  std::vector<int> touchids_targeted_at_current;

  for (const auto& touch_id_target : touch_id_target_) {
    if (touch_id_target.second == current_consumer)
      touchids_targeted_at_current.push_back(touch_id_target.first);
  }

  CancelActiveTouchesExceptImpl(current_consumer);

  std::vector<std::unique_ptr<TouchEvent>> cancelling_touches =
      GetEventPerPointForConsumer(current_consumer, EventType::kTouchCancelled);

  TransferConsumer(current_consumer, new_consumer_ptr.get(), consumers_);

  // We're now in a situation where current_consumer has no gesture recognizer,
  // but has some pointers down which need cancelling. In order to ensure that
  // the GR sees a valid event stream, inform it of these pointers via
  // OnTouchEnter, and then synthesize a touch cancel per pointer.
  if (transfer_touches_behavior == TransferTouchesBehavior::kCancel && helper) {
    GestureProviderAura* gesture_provider =
        GetGestureProviderForConsumer(current_consumer);

    for (std::unique_ptr<TouchEvent>& event : cancelling_touches) {
      gesture_provider->OnTouchEnter(event->pointer_details().id, event->x(),
                                     event->y());
      helper->DispatchSyntheticTouchEvent(event.get());
    }
  }

  // The underlying gesture provider for current_consumer might have filtered
  // gesture detection for some reasons but that might not be applied to the new
  // consumer. See also:
  // https://docs.google.com/document/d/1AKeK8IuF-j2TJ-2sPsewORXdjnr6oAzy5nnR1zwrsfc/edit#
  new_consumer = new_consumer_ptr.get();
  GestureProviderAura* provider =
      new_consumer ? new_consumer->provider() : nullptr;
  if (provider) {
    provider->ResetGestureHandlingState();
  }

  for (int touch_id : touchids_targeted_at_current) {
    if (new_consumer) {
      touch_id_target_[touch_id] = new_consumer_ptr.get();
    } else {
      touch_id_target_.erase(touch_id);
    }
  }
}

bool GestureRecognizerImpl::GetLastTouchPointForTarget(
    GestureConsumer* consumer,
    gfx::PointF* point) {
  GestureProviderAura* provider = consumer->provider();
  if (!provider) {
    return false;
  }
  const MotionEvent& pointer_state = provider->pointer_state();
  if (!pointer_state.GetPointerCount())
    return false;
  *point = gfx::PointF(pointer_state.GetX(), pointer_state.GetY());
  return true;
}

std::vector<std::unique_ptr<TouchEvent>>
GestureRecognizerImpl::GetEventPerPointForConsumer(GestureConsumer* consumer,
                                                   EventType type) {
  std::vector<std::unique_ptr<TouchEvent>> cancelling_touches;
  GestureProviderAura* provider = consumer->provider();
  if (!provider) {
    return cancelling_touches;
  }

  const MotionEventAura& pointer_state = provider->pointer_state();
  if (pointer_state.GetPointerCount() == 0)
    return cancelling_touches;
  cancelling_touches.reserve(pointer_state.GetPointerCount());
  for (size_t i = 0; i < pointer_state.GetPointerCount(); ++i) {
    auto touch_event = std::make_unique<TouchEvent>(
        type, gfx::Point(), EventTimeForNow(),
        PointerDetails(ui::EventPointerType::kTouch,
                       pointer_state.GetPointerId(i)),
        EF_IS_SYNTHESIZED);
    gfx::PointF point(pointer_state.GetX(i), pointer_state.GetY(i));
    touch_event->set_location_f(point);
    touch_event->set_root_location_f(point);
    cancelling_touches.push_back(std::move(touch_event));
  }
  return cancelling_touches;
}

bool GestureRecognizerImpl::CancelActiveTouches(GestureConsumer* consumer) {
  return CancelActiveTouchesImpl(consumer);
}

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerImpl, protected:

GestureProviderAura* GestureRecognizerImpl::GetGestureProviderForConsumer(
    GestureConsumer* consumer) {
  GestureProviderAura* provider = consumer->provider();
  if (!provider) {
    consumers_.insert(consumer);
    consumer->set_gesture_provider(
        std::make_unique<GestureProviderAura>(consumer, this));
  }
  return consumer->provider();
}

bool GestureRecognizerImpl::ProcessTouchEventPreDispatch(
    TouchEvent* event,
    GestureConsumer* consumer) {
  SetupTargets(*event, consumer);

  if (event->result() & ER_CONSUMED)
    return false;

  GestureProviderAura* gesture_provider =
      GetGestureProviderForConsumer(consumer);
  return gesture_provider->OnTouchEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerImpl, private:

void GestureRecognizerImpl::SetupTargets(const TouchEvent& event,
                                         GestureConsumer* target) {
  event_to_gesture_provider_[event.unique_event_id()] =
      GetGestureProviderForConsumer(target);
  if (event.type() == ui::EventType::kTouchReleased ||
      event.type() == ui::EventType::kTouchCancelled) {
    touch_id_target_.erase(event.pointer_details().id);
  } else if (event.type() == ui::EventType::kTouchPressed) {
    touch_id_target_[event.pointer_details().id] = target;
  }
}

void GestureRecognizerImpl::DispatchGestureEvent(
    GestureConsumer* raw_input_consumer,
    GestureEvent* event) {
  if (raw_input_consumer) {
    GestureEventHelper* helper =
        FindDispatchHelperForConsumer(raw_input_consumer);
    if (helper)
      helper->DispatchGestureEvent(raw_input_consumer, event);
  }
}

GestureRecognizer::Gestures GestureRecognizerImpl::AckTouchEvent(
    uint32_t unique_event_id,
    ui::EventResult result,
    bool is_source_touch_event_set_blocking,
    GestureConsumer* consumer) {
  GestureProviderAura* gesture_provider = nullptr;

  // Check if we have already processed this event before dispatch and have a
  // consumer associated with it.
  auto event_to_gesture_provider_iterator =
      event_to_gesture_provider_.find(unique_event_id);
  if (event_to_gesture_provider_iterator != event_to_gesture_provider_.end()) {
    gesture_provider = event_to_gesture_provider_iterator->second;
    event_to_gesture_provider_.erase(event_to_gesture_provider_iterator);
  } else {
    gesture_provider = GetGestureProviderForConsumer(consumer);
  }
  gesture_provider->OnTouchEventAck(unique_event_id, result != ER_UNHANDLED,
                                    is_source_touch_event_set_blocking);
  return gesture_provider->GetAndResetPendingGestures();
}

void GestureRecognizerImpl::CancelActiveTouchesExceptImpl(
    GestureConsumer* not_cancelled) {
  // Do not iterate directly over |consumers_| because canceling
  // active touches may cause the consumer to be removed from
  // |consumers_|. See https://crbug.com/651258 for more info.
  std::set<raw_ptr<GestureConsumer, SetExperimental>> consumers(consumers_);
  for (GestureConsumer* consumer : consumers) {
    if (consumer != not_cancelled) {
      CancelActiveTouchesImpl(consumer);
    }
  }
}

bool GestureRecognizerImpl::CancelActiveTouchesImpl(GestureConsumer* consumer) {
  GestureEventHelper* helper = FindDispatchHelperForConsumer(consumer);

  if (!helper)
    return false;

  std::vector<std::unique_ptr<TouchEvent>> cancelling_touches =
      GetEventPerPointForConsumer(consumer, EventType::kTouchCancelled);
  if (cancelling_touches.empty())
    return false;
  for (const std::unique_ptr<TouchEvent>& cancelling_touch : cancelling_touches)
    helper->DispatchSyntheticTouchEvent(cancelling_touch.get());
  return true;
}

bool GestureRecognizerImpl::CleanupStateForConsumer(GestureConsumer* consumer) {
  bool state_cleaned_up = false;
  state_cleaned_up |= RemoveValueFromMap(&touch_id_target_, consumer);

  // This is a bandaid fix for crbug/732232 that should be further looked into.
  if (consumers_.empty()) {
    return state_cleaned_up;
  }

  if (consumers_.find(consumer) != consumers_.end()) {
    GestureProviderAura* provider = consumer->provider();
    if (provider) {
      RemoveValueFromMap(&event_to_gesture_provider_, provider);
      consumer->reset_gesture_provider();
    }
    consumers_.erase(consumer);
    state_cleaned_up = true;
  }
  return state_cleaned_up;
}

void GestureRecognizerImpl::AddGestureEventHelper(GestureEventHelper* helper) {
  helpers_.push_back(helper);
}

void GestureRecognizerImpl::RemoveGestureEventHelper(
    GestureEventHelper* helper) {
  auto it = base::ranges::find(helpers_, helper);
  if (it != helpers_.end())
    helpers_.erase(it);
}

bool GestureRecognizerImpl::DoesConsumerHaveActiveTouch(
    GestureConsumer* consumer) const {
  for (const auto& id_consumer_pair : touch_id_target_) {
    if (id_consumer_pair.second == consumer)
      return true;
  }
  return false;
}

void GestureRecognizerImpl::SendSynthesizedEndEvents(
    GestureConsumer* consumer) {
  GetGestureProviderForConsumer(consumer)->SendSynthesizedEndEvents();
}

void GestureRecognizerImpl::OnGestureEvent(GestureConsumer* raw_input_consumer,
                                           GestureEvent* event) {
  DispatchGestureEvent(raw_input_consumer, event);
}

void GestureRecognizerImpl::OnGestureProviderAuraWillBeDestroyed(
    GestureProviderAura* gesture_provider) {
  // Clean `event_to_gesture_provider_` by removing invalid raw pointers.
  for (auto iter = event_to_gesture_provider_.begin();
       iter != event_to_gesture_provider_.end();) {
    if (iter->second == gesture_provider)
      iter = event_to_gesture_provider_.erase(iter);
    else
      ++iter;
  }
}

GestureEventHelper* GestureRecognizerImpl::FindDispatchHelperForConsumer(
    GestureConsumer* consumer) {
  for (GestureEventHelper* helper : helpers_) {
    if (helper->CanDispatchToConsumer(consumer))
      return helper;
  }
  return nullptr;
}

}  // namespace ui
