// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_animation_sequence.h"

#include <algorithm>
#include <iterator>

#include "base/check.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "cc/animation/animation_id_provider.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"

namespace ui {

LayerAnimationSequence::LayerAnimationSequence()
    : properties_(LayerAnimationElement::UNKNOWN),
      is_repeating_(false),
      last_element_(0),
      waiting_for_group_start_(false),
      animation_group_id_(0),
      last_progressed_fraction_(0.0) {}

LayerAnimationSequence::LayerAnimationSequence(
    std::unique_ptr<LayerAnimationElement> element)
    : properties_(LayerAnimationElement::UNKNOWN),
      is_repeating_(false),
      last_element_(0),
      waiting_for_group_start_(false),
      animation_group_id_(0),
      last_progressed_fraction_(0.0) {
  AddElement(std::move(element));
}

LayerAnimationSequence::~LayerAnimationSequence() {
  observers_.Notify(&LayerAnimationObserver::DetachedFromSequence, this, true);
}

void LayerAnimationSequence::Start(LayerAnimationDelegate* delegate) {
  DCHECK(start_time_ != base::TimeTicks());

  last_progressed_fraction_ = 0.0;
  if (elements_.empty())
    return;

  // TODO(b/352744702): Convert to CHECK after https://crrev.com/c/5713998
  // has rolled out and any cases like this have been removed.
  DUMP_WILL_BE_CHECK(
      !(is_repeating_ && GetTotalDurationOfAllElements().is_zero()))
      << "A repeating animation with zero duration is not a supported "
         "combination. It unnecessarily consumes CPU resources for an "
         "indefinite amount of time without any actual animated content";

  elements_[0]->set_requested_start_time(start_time_);
  elements_[0]->Start(delegate, animation_group_id_);

  NotifyStarted();

  // This may have been aborted.
}

void LayerAnimationSequence::Progress(base::TimeTicks now,
                                      LayerAnimationDelegate* delegate) {
  DCHECK(start_time_ != base::TimeTicks());
  bool redraw_required = false;

  if (elements_.empty())
    return;

  if (last_element_ == 0)
    last_start_ = start_time_;

  const base::TimeDelta total_duration = GetTotalDurationOfAllElements();
  const auto animation_should_progress = [this, total_duration]() {
    // A repeating animation with zero total duration results in an infinite
    // `while` loop below, so this corner case must be checked explicitly.
    // In this case, the ui should immediately render the properties' target
    // values.
    return (is_repeating_ && !total_duration.is_zero()) ||
           last_element_ < elements_.size();
  };
  size_t current_index = last_element_ % elements_.size();
  base::TimeDelta element_duration;
  bool just_completed_sequence = total_duration.is_zero();
  while (animation_should_progress()) {
    elements_[current_index]->set_requested_start_time(last_start_);
    if (!elements_[current_index]->IsFinished(now, &element_duration))
      break;

    // Let the element we're passing finish.
    if (elements_[current_index]->ProgressToEnd(delegate))
      redraw_required = true;
    last_start_ += element_duration;
    ++last_element_;
    last_progressed_fraction_ =
        elements_[current_index]->last_progressed_fraction();
    current_index = last_element_ % elements_.size();
    DCHECK_GT(last_element_, 0u);
    just_completed_sequence = current_index == 0;
  }

  if (animation_should_progress()) {
    if (!elements_[current_index]->Started()) {
      animation_group_id_ = cc::AnimationIdProvider::NextGroupId();
      elements_[current_index]->Start(delegate, animation_group_id_);
    }
    base::WeakPtr<LayerAnimationSequence> alive(AsWeakPtr());
    if (elements_[current_index]->Progress(now, delegate))
      redraw_required = true;
    if (!alive)
      return;
    last_progressed_fraction_ =
        elements_[current_index]->last_progressed_fraction();
  }

  // Since the delegate may be deleted due to the notifications below, it is
  // important that we schedule a draw before sending them.
  if (redraw_required)
    delegate->ScheduleDrawForAnimation();

  if (just_completed_sequence) {
    if (!is_repeating_) {
      last_element_ = 0;
      waiting_for_group_start_ = false;
      animation_group_id_ = 0;
      NotifyEnded();
    } else {
      NotifyWillRepeat();
    }
  }
}

bool LayerAnimationSequence::IsFinished(base::TimeTicks time) {
  if (is_repeating_ || waiting_for_group_start_)
    return false;

  if (elements_.empty())
    return true;

  if (last_element_ == 0)
    last_start_ = start_time_;

  base::TimeTicks current_start = last_start_;
  size_t current_index = last_element_;
  base::TimeDelta element_duration;
  while (current_index < elements_.size()) {
    elements_[current_index]->set_requested_start_time(current_start);
    if (!elements_[current_index]->IsFinished(time, &element_duration))
      break;

    current_start += element_duration;
    ++current_index;
  }

  return (current_index == elements_.size());
}

void LayerAnimationSequence::ProgressToEnd(LayerAnimationDelegate* delegate) {
  bool redraw_required = false;

  if (elements_.empty())
    return;

  size_t current_index = last_element_ % elements_.size();
  while (current_index < elements_.size()) {
    if (elements_[current_index]->ProgressToEnd(delegate))
      redraw_required = true;
    last_progressed_fraction_ =
        elements_[current_index]->last_progressed_fraction();
    ++current_index;
    ++last_element_;
  }

  if (redraw_required)
    delegate->ScheduleDrawForAnimation();

  if (!is_repeating_) {
    last_element_ = 0;
    waiting_for_group_start_ = false;
    animation_group_id_ = 0;
    NotifyEnded();
  } else {
    NotifyWillRepeat();
  }
}

void LayerAnimationSequence::GetTargetValue(
    LayerAnimationElement::TargetValue* target) const {
  if (is_repeating_)
    return;

  for (size_t i = last_element_; i < elements_.size(); ++i)
    elements_[i]->GetTargetValue(target);
}

void LayerAnimationSequence::Abort(LayerAnimationDelegate* delegate) {
  size_t current_index = last_element_ % elements_.size();
  while (current_index < elements_.size()) {
    elements_[current_index]->Abort(delegate);
    ++current_index;
  }
  last_element_ = 0;
  waiting_for_group_start_ = false;
  NotifyAborted();
}

void LayerAnimationSequence::AddElement(
    std::unique_ptr<LayerAnimationElement> element) {
  properties_ |= element->properties();
  elements_.push_back(std::move(element));
}

bool LayerAnimationSequence::HasConflictingProperty(
    LayerAnimationElement::AnimatableProperties other) const {
  return (properties_ & other) != LayerAnimationElement::UNKNOWN;
}

bool LayerAnimationSequence::IsFirstElementThreaded(
    LayerAnimationDelegate* delegate) const {
  if (!elements_.empty())
    return elements_[0]->IsThreaded(delegate);

  return false;
}

void LayerAnimationSequence::AddObserver(LayerAnimationObserver* observer) {
  if (!observers_.HasObserver(observer)) {
    observers_.AddObserver(observer);
    observer->AttachedToSequence(this);
  }
}

void LayerAnimationSequence::RemoveObserver(LayerAnimationObserver* observer) {
  observers_.RemoveObserver(observer);
  observer->DetachedFromSequence(this, true);
}

void LayerAnimationSequence::OnThreadedAnimationStarted(
    base::TimeTicks monotonic_time,
    cc::TargetProperty::Type target_property,
    int group_id) {
  if (elements_.empty() || group_id != animation_group_id_)
    return;

  size_t current_index = last_element_ % elements_.size();
  LayerAnimationElement::AnimatableProperties element_properties =
    elements_[current_index]->properties();
  LayerAnimationElement::AnimatableProperty event_property =
      LayerAnimationElement::ToAnimatableProperty(target_property);
  DCHECK(element_properties & event_property);
  elements_[current_index]->set_effective_start_time(monotonic_time);
}

void LayerAnimationSequence::OnScheduled() {
  NotifyScheduled();
}

void LayerAnimationSequence::OnAnimatorDestroyed() {
  for (LayerAnimationObserver& observer : observers_) {
    if (!observer.RequiresNotificationWhenAnimatorDestroyed()) {
      // Remove the observer, but do not allow notifications to be sent.
      observers_.RemoveObserver(&observer);
      observer.DetachedFromSequence(this, false);
    }
  }
}

void LayerAnimationSequence::OnAnimatorAttached(
    LayerAnimationDelegate* delegate) {
  observers_.Notify(&LayerAnimationObserver::OnAnimatorAttachedToTimeline);
}

void LayerAnimationSequence::OnAnimatorDetached() {
  observers_.Notify(&LayerAnimationObserver::OnAnimatorDetachedFromTimeline);
}

size_t LayerAnimationSequence::size() const {
  return elements_.size();
}

LayerAnimationElement* LayerAnimationSequence::FirstElement() const {
  if (elements_.empty()) {
    return nullptr;
  }

  return elements_[0].get();
}

void LayerAnimationSequence::NotifyScheduled() {
  observers_.Notify(&LayerAnimationObserver::OnLayerAnimationScheduled, this);
}

void LayerAnimationSequence::NotifyStarted() {
  observers_.Notify(&LayerAnimationObserver::OnLayerAnimationStarted, this);
}

void LayerAnimationSequence::NotifyEnded() {
  observers_.Notify(&LayerAnimationObserver::OnLayerAnimationEnded, this);
}

void LayerAnimationSequence::NotifyWillRepeat() {
  observers_.Notify(&LayerAnimationObserver::OnLayerAnimationWillRepeat, this);
}

void LayerAnimationSequence::NotifyAborted() {
  observers_.Notify(&LayerAnimationObserver::OnLayerAnimationAborted, this);
}

LayerAnimationElement* LayerAnimationSequence::CurrentElement() const {
  if (elements_.empty())
    return NULL;

  size_t current_index = last_element_ % elements_.size();
  return elements_[current_index].get();
}

base::TimeDelta LayerAnimationSequence::GetTotalDurationOfAllElements() const {
  base::TimeDelta total_duration;
  for (const std::unique_ptr<LayerAnimationElement>& element : elements_) {
    total_duration += element->duration();
  }
  return total_duration;
}

std::string LayerAnimationSequence::ElementsToString() const {
  std::string str;
  for (size_t i = 0; i < elements_.size(); i++) {
    if (i > 0)
      str.append(", ");
    str.append(elements_[i]->ToString());
  }
  return str;
}

std::string LayerAnimationSequence::ToString() const {
  return base::StringPrintf(
      "LayerAnimationSequence{size=%zu, properties=%s, "
      "elements=[%s], is_repeating=%d, group_id=%d}",
      size(),
      LayerAnimationElement::AnimatablePropertiesToString(properties_).c_str(),
      ElementsToString().c_str(), is_repeating_, animation_group_id_);
}

}  // namespace ui
