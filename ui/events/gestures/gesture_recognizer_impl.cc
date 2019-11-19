// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/gesture_recognizer_impl.h"

#include <stddef.h>

#include <limits>
#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_switches.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gestures/gesture_types.h"

namespace ui {

namespace {

void TransferConsumer(
    GestureConsumer* current_consumer,
    GestureConsumer* new_consumer,
    std::map<GestureConsumer*, std::unique_ptr<GestureProviderAura>>* map) {
  if (!map->empty() && base::Contains(*map, current_consumer)) {
    (*map)[new_consumer] = std::move((*map)[current_consumer]);
    (*map)[new_consumer]->set_gesture_consumer(new_consumer);
    map->erase(current_consumer);
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

GestureRecognizerImpl::~GestureRecognizerImpl() = default;

// Checks if this finger is already down, if so, returns the current target.
// Otherwise, returns NULL.
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

  for (const auto& provider_pair : consumer_gesture_provider_) {
    const MotionEventAura& pointer_state =
        provider_pair.second->pointer_state();
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
  return NULL;
}

void GestureRecognizerImpl::CancelActiveTouchesExcept(
    GestureConsumer* not_cancelled) {
  CancelActiveTouchesExceptImpl(not_cancelled);
}

void GestureRecognizerImpl::CancelActiveTouchesOn(
    const std::vector<GestureConsumer*>& consumers) {
  for (auto* consumer : consumers) {
    if (base::Contains(consumer_gesture_provider_, consumer))
      CancelActiveTouchesImpl(consumer);
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
  GestureEventHelper* helper = FindDispatchHelperForConsumer(current_consumer);

  std::vector<int> touchids_targeted_at_current;

  for (const auto& touch_id_target : touch_id_target_) {
    if (touch_id_target.second == current_consumer)
      touchids_targeted_at_current.push_back(touch_id_target.first);
  }

  CancelActiveTouchesExceptImpl(current_consumer);

  std::vector<std::unique_ptr<TouchEvent>> cancelling_touches =
      GetEventPerPointForConsumer(current_consumer, ET_TOUCH_CANCELLED);

  TransferConsumer(current_consumer, new_consumer, &consumer_gesture_provider_);

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
  if (base::Contains(consumer_gesture_provider_, new_consumer))
    GetGestureProviderForConsumer(new_consumer)->ResetGestureHandlingState();

  for (int touch_id : touchids_targeted_at_current)
    touch_id_target_[touch_id] = new_consumer;
}

std::vector<std::unique_ptr<ui::TouchEvent>>
GestureRecognizerImpl::ExtractTouches(GestureConsumer* consumer) {
  std::vector<std::unique_ptr<ui::TouchEvent>> touches =
      GetEventPerPointForConsumer(consumer, ET_TOUCH_PRESSED);
  return touches;
}

void GestureRecognizerImpl::TransferTouches(
    GestureConsumer* consumer,
    const std::vector<std::unique_ptr<ui::TouchEvent>>& touch_events) {
  GestureEventHelper* helper = FindDispatchHelperForConsumer(consumer);
  DCHECK(helper);
  for (const auto& event : touch_events) {
    helper->DispatchSyntheticTouchEvent(event.get());
  }
}

bool GestureRecognizerImpl::GetLastTouchPointForTarget(
    GestureConsumer* consumer,
    gfx::PointF* point) {
  if (consumer_gesture_provider_.empty())
    return false;
  if (!base::Contains(consumer_gesture_provider_, consumer))
    return false;
  const MotionEvent& pointer_state =
      consumer_gesture_provider_[consumer]->pointer_state();
  if (!pointer_state.GetPointerCount())
    return false;
  *point = gfx::PointF(pointer_state.GetX(), pointer_state.GetY());
  return true;
}

std::vector<std::unique_ptr<TouchEvent>>
GestureRecognizerImpl::GetEventPerPointForConsumer(GestureConsumer* consumer,
                                                   EventType type) {
  std::vector<std::unique_ptr<TouchEvent>> cancelling_touches;
  if (consumer_gesture_provider_.empty())
    return cancelling_touches;

  if (!base::Contains(consumer_gesture_provider_, consumer))
    return cancelling_touches;
  const MotionEventAura& pointer_state =
      consumer_gesture_provider_[consumer]->pointer_state();
  if (pointer_state.GetPointerCount() == 0)
    return cancelling_touches;
  for (size_t i = 0; i < pointer_state.GetPointerCount(); ++i) {
    auto touch_event = std::make_unique<TouchEvent>(
        type, gfx::Point(), EventTimeForNow(),
        PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
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
  GestureProviderAura* gesture_provider = nullptr;

  if (!consumer_gesture_provider_.empty() &&
      base::Contains(consumer_gesture_provider_, consumer)) {
    gesture_provider = consumer_gesture_provider_.at(consumer).get();
  }

  if (!gesture_provider) {
    gesture_provider = new GestureProviderAura(consumer, this);
    consumer_gesture_provider_[consumer] = base::WrapUnique(gesture_provider);
  }
  return gesture_provider;
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
  if (event.type() == ui::ET_TOUCH_RELEASED ||
      event.type() == ui::ET_TOUCH_CANCELLED) {
    touch_id_target_.erase(event.pointer_details().id);
  } else if (event.type() == ui::ET_TOUCH_PRESSED) {
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
    bool is_source_touch_event_set_non_blocking,
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
                                    is_source_touch_event_set_non_blocking);
  return gesture_provider->GetAndResetPendingGestures();
}

void GestureRecognizerImpl::CancelActiveTouchesExceptImpl(
    GestureConsumer* not_cancelled) {
  // Do not iterate directly over |consumer_gesture_provider_| because canceling
  // active touches may cause the consumer to be removed from
  // |consumer_gesture_provider_|. See https://crbug.com/651258 for more info.
  std::vector<GestureConsumer*> consumers(consumer_gesture_provider_.size());
  for (const auto& entry : consumer_gesture_provider_) {
    if (entry.first == not_cancelled)
      continue;

    consumers.push_back(entry.first);
  }

  for (auto* consumer : consumers)
    CancelActiveTouchesImpl(consumer);
}

bool GestureRecognizerImpl::CancelActiveTouchesImpl(GestureConsumer* consumer) {
  GestureEventHelper* helper = FindDispatchHelperForConsumer(consumer);

  if (!helper)
    return false;

  std::vector<std::unique_ptr<TouchEvent>> cancelling_touches =
      GetEventPerPointForConsumer(consumer, ET_TOUCH_CANCELLED);
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
  if (consumer_gesture_provider_.empty())
    return state_cleaned_up;

  auto consumer_gesture_provider_it = consumer_gesture_provider_.find(consumer);
  if (consumer_gesture_provider_it != consumer_gesture_provider_.end()) {
    // Remove gesture provider associated with the consumer from
    // |event_to_gesture_provider_| map.
    RemoveValueFromMap(&event_to_gesture_provider_,
                       consumer_gesture_provider_it->second.get());
    state_cleaned_up = true;
    consumer_gesture_provider_.erase(consumer_gesture_provider_it);
  }
  return state_cleaned_up;
}

void GestureRecognizerImpl::AddGestureEventHelper(GestureEventHelper* helper) {
  helpers_.push_back(helper);
}

void GestureRecognizerImpl::RemoveGestureEventHelper(
    GestureEventHelper* helper) {
  auto it = std::find(helpers_.begin(), helpers_.end(), helper);
  if (it != helpers_.end())
    helpers_.erase(it);
}

void GestureRecognizerImpl::OnGestureEvent(GestureConsumer* raw_input_consumer,
                                           GestureEvent* event) {
  DispatchGestureEvent(raw_input_consumer, event);
}

GestureEventHelper* GestureRecognizerImpl::FindDispatchHelperForConsumer(
    GestureConsumer* consumer) {
  std::vector<GestureEventHelper*>::iterator it;
  for (it = helpers_.begin(); it != helpers_.end(); ++it) {
    if ((*it)->CanDispatchToConsumer(consumer))
      return (*it);
  }
  return NULL;
}

}  // namespace ui
