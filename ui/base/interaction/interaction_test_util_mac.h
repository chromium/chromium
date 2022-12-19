// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_INTERACTION_TEST_UTIL_MAC_H_
#define UI_BASE_INTERACTION_INTERACTION_TEST_UTIL_MAC_H_

#include "ui/base/interaction/interaction_test_util.h"

namespace ui {
class TrackedElement;
}

namespace ui::test {

// Mac implementation of InteractionTestUtil::Simulator.
// Add one to your InteractionTestUtil instance to get Mac native menu support.
class InteractionTestUtilSimulatorMac : public InteractionTestUtil::Simulator {
 public:
  InteractionTestUtilSimulatorMac();
  ~InteractionTestUtilSimulatorMac() override;

  // InteractionTestUtil::Simulator:
  ActionResult SelectMenuItem(ui::TrackedElement* element,
                              InputType input_type) override;
};

}  // namespace ui::test

#endif  // UI_BASE_INTERACTION_INTERACTION_TEST_UTIL_MAC_H_
