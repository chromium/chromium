// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_MOCK_INPUT_EVENT_ACTIVATION_PROTECTOR_H_
#define UI_VIEWS_TEST_MOCK_INPUT_EVENT_ACTIVATION_PROTECTOR_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/input_event_activation_protector.h"

namespace views {

// Mock version of InputEventActivationProtector for injection during tests, to
// allow verifying that protected Views work as expected.
class MockInputEventActivationProtector : public InputEventActivationProtector {
 public:
  MockInputEventActivationProtector();
  ~MockInputEventActivationProtector() override;

  MockInputEventActivationProtector(const MockInputEventActivationProtector&) =
      delete;
  MockInputEventActivationProtector& operator=(
      const MockInputEventActivationProtector&) = delete;

  MOCK_METHOD(bool,
              IsPossiblyUnintendedInteraction,
              (const ui::Event& event),
              (override));
};

}  // namespace views

#endif  // UI_VIEWS_TEST_MOCK_INPUT_EVENT_ACTIVATION_PROTECTOR_H_
