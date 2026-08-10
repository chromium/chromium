// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_state_observers.h"

#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/base/interaction/state_observer.h"

namespace ui::test::internal {

DEFINE_STATE_IDENTIFIER_VALUE(::ui::test::internal::ElementCountStateObserver,
                              kWaitForElementCountState);

ElementCountStateObserver::ElementCountStateObserver(
    ui::ElementIdentifier id,
    ui::ElementContext context) {
  auto* const tracker = ui::ElementTracker::GetElementTracker();

  if (context) {
    count_ = tracker->GetAllMatchingElements(id, context).size();
    shown_subscription_ = tracker->AddElementShownCallback(
        id, context,
        base::BindRepeating(&ElementCountStateObserver::OnCountChanged,
                            base::Unretained(this), true));
    hidden_subscription_ = tracker->AddElementHiddenCallback(
        id, context,
        base::BindRepeating(&ElementCountStateObserver::OnCountChanged,
                            base::Unretained(this), false));
  } else {
    count_ = tracker->GetAllMatchingElementsInAnyContext(id).size();
    shown_subscription_ = tracker->AddElementShownInAnyContextCallback(
        id, base::BindRepeating(&ElementCountStateObserver::OnCountChanged,
                                base::Unretained(this), true));
    hidden_subscription_ = tracker->AddElementHiddenInAnyContextCallback(
        id, base::BindRepeating(&ElementCountStateObserver::OnCountChanged,
                                base::Unretained(this), false));
  }
}

ElementCountStateObserver::~ElementCountStateObserver() = default;

size_t ElementCountStateObserver::GetStateObserverInitialState() const {
  return count_;
}

void ElementCountStateObserver::OnCountChanged(bool added,
                                               ui::TrackedElement*) {
  if (added) {
    ++count_;
  } else {
    CHECK_GT(count_, 0U);
    --count_;
  }
  OnStateObserverStateChanged(count_);
}

DEFINE_STATE_IDENTIFIER_VALUE(::ui::test::internal::ElementMatcherStateObserver,
                              kWaitForMatchingElementState);

DEFINE_STATE_IDENTIFIER_VALUE(::ui::test::PollingStateObserver<bool>,
                              kWaitForElementMatchingImplState);

ElementMatcherStateObserver::ElementMatcherStateObserver(
    ui::ElementIdentifier id,
    ui::ElementContext context,
    const Predicate& predicate)
    : id_(id), context_(context), predicate_(predicate) {
  UpdatePresence(nullptr);

  auto* const tracker = ui::ElementTracker::GetElementTracker();
  if (context) {
    shown_subscription_ = tracker->AddElementShownCallback(
        id, context,
        base::BindRepeating(&ElementMatcherStateObserver::UpdatePresence,
                            base::Unretained(this)));
    hidden_subscription_ = tracker->AddElementHiddenCallback(
        id, context,
        base::BindRepeating(&ElementMatcherStateObserver::UpdatePresence,
                            base::Unretained(this)));
  } else {
    shown_subscription_ = tracker->AddElementShownInAnyContextCallback(
        id, base::BindRepeating(&ElementMatcherStateObserver::UpdatePresence,
                                base::Unretained(this)));
    hidden_subscription_ = tracker->AddElementHiddenInAnyContextCallback(
        id, base::BindRepeating(&ElementMatcherStateObserver::UpdatePresence,
                                base::Unretained(this)));
  }
}

ElementMatcherStateObserver::~ElementMatcherStateObserver() = default;

bool ElementMatcherStateObserver::GetStateObserverInitialState() const {
  return present_;
}

// static
ui::TrackedElement* ElementMatcherStateObserver::GetMatchingElement(
    ui::ElementIdentifier id,
    ui::ElementContext context,
    const Predicate& predicate) {
  auto* const tracker = ui::ElementTracker::GetElementTracker();
  ElementTracker::ElementList elements;
  if (context) {
    elements = tracker->GetAllMatchingElements(id, context);
  } else {
    elements = tracker->GetAllMatchingElementsInAnyContext(id);
  }
  for (auto* const element : elements) {
    if (predicate.Run(element)) {
      return element;
    }
  }
  return nullptr;
}

void ElementMatcherStateObserver::UpdatePresence(ui::TrackedElement*) {
  const bool was_present = present_;
  present_ = GetMatchingElement(id_, context_, predicate_);
  if (present_ != was_present) {
    OnStateObserverStateChanged(present_);
  }
}

}  // namespace ui::test::internal
