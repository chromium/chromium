// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_ELEMENT_STATE_OBSERVERS_H_
#define UI_BASE_INTERACTION_ELEMENT_STATE_OBSERVERS_H_

#include <optional>

#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/base/interaction/state_observer.h"

namespace ui::test::internal {

// Observer that waits for a specific number of elements with `id` in `context`;
// if `context` is null, the count of elements with `id` in all contexts are
// observed.
class ElementCountStateObserver : public StateObserver<size_t> {
 public:
  ElementCountStateObserver(ui::ElementIdentifier id,
                            ui::ElementContext context);
  ~ElementCountStateObserver() override;

  size_t GetStateObserverInitialState() const override;

 private:
  void OnCountChanged(bool added, ui::TrackedElement*);

  base::CallbackListSubscription shown_subscription_;
  base::CallbackListSubscription hidden_subscription_;
  size_t count_ = 0;
};

DECLARE_STATE_IDENTIFIER_VALUE(::ui::test::internal::ElementCountStateObserver,
                               kWaitForElementCountState);

// Observer that waits for an element that matches a specific `id` in `context`
// that satisfies `predicate`. If `context` is null, elements in all contexts
// are checked.
class ElementMatcherStateObserver : public StateObserver<bool> {
 public:
  using Predicate = base::RepeatingCallback<bool(const ui::TrackedElement* el)>;

  ElementMatcherStateObserver(ui::ElementIdentifier id,
                              ui::ElementContext context,
                              const Predicate& predicate);
  ~ElementMatcherStateObserver() override;

  bool GetStateObserverInitialState() const override;

  // General purpose function for trying to find a matching element.
  static ui::TrackedElement* GetMatchingElement(ui::ElementIdentifier id,
                                                ui::ElementContext context,
                                                const Predicate& predicate);

 private:
  void UpdatePresence(ui::TrackedElement*);

  const ui::ElementIdentifier id_;
  const ui::ElementContext context_;
  const Predicate predicate_;
  base::CallbackListSubscription shown_subscription_;
  base::CallbackListSubscription hidden_subscription_;
  bool present_ = false;
};

DECLARE_STATE_IDENTIFIER_VALUE(
    ::ui::test::internal::ElementMatcherStateObserver,
    kWaitForMatchingElementState);

DECLARE_STATE_IDENTIFIER_VALUE(::ui::test::PollingStateObserver<bool>,
                               kWaitForElementMatchingImplState);

}  // namespace ui::test::internal

#endif  // UI_BASE_INTERACTION_ELEMENT_STATE_OBSERVERS_H_
