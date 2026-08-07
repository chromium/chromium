// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_state_observers.h"

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

}  // namespace ui::test::internal
