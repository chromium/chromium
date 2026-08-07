// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_ELEMENT_STATE_OBSERVERS_H_
#define UI_BASE_INTERACTION_ELEMENT_STATE_OBSERVERS_H_

#include <optional>

#include "ui/base/interaction/element_identifier.h"
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

}  // namespace ui::test::internal

#endif  // UI_BASE_INTERACTION_ELEMENT_STATE_OBSERVERS_H_
