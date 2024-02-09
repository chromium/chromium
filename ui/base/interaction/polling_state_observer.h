// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_POLLING_STATE_OBSERVER_H_
#define UI_BASE_INTERACTION_POLLING_STATE_OBSERVER_H_

#include <optional>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interactive_test_internal.h"
#include "ui/base/interaction/state_observer.h"

namespace ui::test {

// Observer which polls a state that cannot be observed via listeners/callbacks.
// Continues to poll for the lifetime of the test.
//
// Because this is a polling observer, the value is only valid at the moment it
// is observed, and transient values may be missed. For transient values, prefer
// deterministic state observation, custom events, etc.
//
// Designed for use with the `InteractiveTestApi::PollState()` verb.
template <typename T>
class PollingStateObserver : public StateObserver<T> {
 public:
  using PollCallback = base::RepeatingCallback<T()>;

  static constexpr base::TimeDelta kDefaultPollingInterval =
      base::Milliseconds(100);

  // Calls `poll_callback` to get an updated value every `polling_interval`.
  // `poll_callback` will also be called on initialization to get an initial
  // value.
  template <typename C>
  explicit PollingStateObserver(
      C&& poll_callback,
      base::TimeDelta polling_interval = kDefaultPollingInterval)
      : poll_callback_(
            internal::MaybeBindRepeating(std::forward<C>(poll_callback))),
        timer_(FROM_HERE,
               polling_interval,
               base::BindRepeating(&PollingStateObserver<T>::OnPoll,
                                   base::Unretained(this))) {
    timer_.Reset();
  }

  ~PollingStateObserver() override = default;

  // StateObserver:
  T GetStateObserverInitialState() const final { return poll_callback_.Run(); }

 private:
  void OnPoll() {
    StateObserver<T>::OnStateObserverStateChanged(poll_callback_.Run());
  }

  // The callback that returns the value when polled.
  PollCallback poll_callback_;

  // This timer will fire every `polling_interval`
  base::RepeatingTimer timer_;
};

// Need out-of-line declaration of static class variables on some platforms.
template <typename T>
// static
constexpr base::TimeDelta PollingStateObserver<T>::kDefaultPollingInterval;

// Observer which polls a specific element that cannot be observed via
// listeners/callbacks. If an element with `identifier` in `context` is present,
// the observed value will be updated to the result of `poll_element_callback`,
// whereas if the element is not present in the context, it will be
// std::nullopt.
//
// If `context` is not specified, then the element will be located in any
// context.
//
// Because this is a polling observer, the value is only valid at the moment it
// is observed, and transient values may be missed. For transient values, prefer
// deterministic state observation, custom events, etc.
//
// Designed for use with the `InteractiveTestApi::PollElement()` verb.
template <typename T>
class PollingElementStateObserver
    : public PollingStateObserver<std::optional<T>> {
 public:
  using PollElementCallback = base::RepeatingCallback<T(const TrackedElement*)>;

  // Calls `poll_element_callback` on the element with `identifier` in
  // `context` to update the value every `polling_interval`. If a matching
  // element is not present, the value is `std::nullopt`. The callback will be
  // called on initialization (if the element already exists) to get an initial
  // value.
  template <typename C>
  PollingElementStateObserver(
      ElementIdentifier identifier,
      std::optional<ElementContext> context,
      C&& poll_element_callback,
      base::TimeDelta polling_interval =
          PollingStateObserver<std::optional<T>>::kDefaultPollingInterval)
      : PollingStateObserver<std::optional<T>>(
            base::BindRepeating(
                [](ElementIdentifier id,
                   std::optional<ElementContext> ctx,
                   PollElementCallback cb) {
                  auto* const el = ctx.has_value()
                                       ? ElementTracker::GetElementTracker()
                                             ->GetFirstMatchingElement(id, *ctx)
                                       : ElementTracker::GetElementTracker()
                                             ->GetElementInAnyContext(id);
                  return el ? std::make_optional(cb.Run(el)) : std::nullopt;
                },
                identifier,
                context,
                internal::MaybeBindRepeating(
                    std::forward<C>(poll_element_callback))),
            polling_interval) {}

  ~PollingElementStateObserver() override = default;
};

}  // namespace ui::test

#endif  // UI_BASE_INTERACTION_POLLING_STATE_OBSERVER_H_
