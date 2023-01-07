// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interaction_test_util.h"

namespace ui::test {

InteractionTestUtil::InteractionTestUtil() = default;
InteractionTestUtil::~InteractionTestUtil() = default;

void InteractionTestUtil::PressButton(TrackedElement* element,
                                      InputType input_type) {
  for (const auto& simulator : simulators_) {
    if (simulator->PressButton(element, input_type))
      return;
  }

  // If a test has requested an invalid operation on an element, then this is
  // an error.
  NOTREACHED();
}

void InteractionTestUtil::SelectMenuItem(TrackedElement* element,
                                         InputType input_type) {
  for (const auto& simulator : simulators_) {
    if (simulator->SelectMenuItem(element, input_type))
      return;
  }

  // If a test has requested an invalid operation on an element, then this is
  // an error.
  NOTREACHED();
}

}  // namespace ui::test