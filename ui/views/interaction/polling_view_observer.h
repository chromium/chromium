// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_POLLING_VIEW_OBSERVER_H_
#define UI_VIEWS_INTERACTION_POLLING_VIEW_OBSERVER_H_

#include <optional>
#include <type_traits>
#include <utility>

#include "base/time/time.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interactive_views_test_internal.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace views::test {

// Polling static observer that repeatedly calls `poll_view_callback` on a View
// of type `V` with `view_id` in `context` (or any context if not specified) to
// determine the value of the state, which is of `type std::optional<T>`.
// The state value `std::nullopt` corresponds to no matching view present.
//
// Designed for use with the `InteractiveViewsTestApi::PollView()` testing verb.
//
// See `PollingElementStateObserver<T>` for more information on usage.
template <typename T,
          typename V,
          typename = std::enable_if<std::is_base_of_v<View, V>>>
class PollingViewObserver : public ui::test::PollingElementStateObserver<T> {
 public:
  using PollViewCallback = base::RepeatingCallback<T(const V*)>;

  template <typename C>
  PollingViewObserver(
      ui::ElementIdentifier view_id,
      std::optional<ui::ElementContext> context,
      C&& poll_view_callback,
      base::TimeDelta polling_interval = ui::test::PollingStateObserver<
          std::optional<T>>::kDefaultPollingInterval)
      : ui::test::PollingElementStateObserver<T>(
            view_id,
            context,
            base::BindRepeating(
                [](PollViewCallback callback, const ui::TrackedElement* el) {
                  const auto* const view_el = el->AsA<TrackedElementViews>();
                  CHECK(view_el);
                  const auto* const view =
                      views::AsViewClass<V>(view_el->view());
                  CHECK(view);
                  return callback.Run(view);
                },
                ui::test::internal::MaybeBindRepeating(
                    std::forward<C>(poll_view_callback))),
            polling_interval) {}

  ~PollingViewObserver() = default;
};

// Polling static observer that repeatedly calls `property` on a View of type
// of type `V` with `view_id` in `context` (or any context if not specified) to
// determine the value of the state, which is of `type std::optional<T>`.
// The state value `std::nullopt` corresponds to no matching view present.
//
// Designed for use with the `InteractiveViewsTestApi::PollViewProperty()`
// testing verb.
//
// See `PollingElementStateObserver<T>` for more information on usage.
template <typename T,
          typename V,
          typename = std::enable_if<std::is_base_of_v<View, V>>>
class PollingViewPropertyObserver : public PollingViewObserver<T, V> {
 public:
  template <typename R>
  PollingViewPropertyObserver(
      ui::ElementIdentifier view_id,
      std::optional<ui::ElementContext> context,
      R (V::*property)() const,
      base::TimeDelta polling_interval = ui::test::PollingStateObserver<
          std::optional<T>>::kDefaultPollingInterval)
      : PollingViewObserver<T, V>(
            view_id,
            context,
            base::BindRepeating([](R (V::*property)() const, const V* view)
                                    -> T { return (view->*property)(); },
                                property),
            polling_interval) {}

  ~PollingViewPropertyObserver() override = default;
};

}  // namespace views::test

#endif  // UI_VIEWS_INTERACTION_POLLING_VIEW_OBSERVER_H_
