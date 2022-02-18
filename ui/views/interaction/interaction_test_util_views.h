// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_INTERACTION_TEST_UTIL_VIEWS_H_
#define UI_VIEWS_INTERACTION_INTERACTION_TEST_UTIL_VIEWS_H_

#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_test_util.h"

namespace ui {
class TrackedElement;
}

namespace views::test {

// Views implementation of InteractionTestUtil::Simulator.
// Add one to your InteractionTestUtil instance to get Views support.
class InteractionTestUtilSimulatorViews
    : public ui::test::InteractionTestUtil::Simulator {
 public:
  InteractionTestUtilSimulatorViews();
  ~InteractionTestUtilSimulatorViews() override;

  // ui::test::InteractionTestUtil::Simulator:
  bool PressButton(ui::TrackedElement* element, InputType input_type) override;
  bool SelectMenuItem(ui::TrackedElement* element,
                      InputType input_type) override;
};

}  // namespace views::test

#endif  // UI_VIEWS_INTERACTION_INTERACTION_TEST_UTIL_VIEWS_H_
