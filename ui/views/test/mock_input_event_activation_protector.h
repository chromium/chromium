// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_MOCK_INPUT_EVENT_ACTIVATION_PROTECTOR_H_
#define UI_VIEWS_TEST_MOCK_INPUT_EVENT_ACTIVATION_PROTECTOR_H_

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/input_event_activation_protector.h"

namespace views {

class InputProtectorDelegate;
class View;

// Mock version of InputEventActivationProtector for injection during tests, to
// allow verifying that protected Views work as expected.
class MockInputEventActivationProtector : public InputEventActivationProtector {
 public:
  MockInputEventActivationProtector();
  explicit MockInputEventActivationProtector(
      std::unique_ptr<InputProtectorDelegate> delegate);
  ~MockInputEventActivationProtector() override;

  MockInputEventActivationProtector(const MockInputEventActivationProtector&) =
      delete;
  MockInputEventActivationProtector& operator=(
      const MockInputEventActivationProtector&) = delete;

  using InputEventActivationProtector::IsPossiblyUnintendedInteraction;

  MOCK_METHOD(bool,
              IsPossiblyUnintendedInteraction,
              (const ui::Event& event,
               bool allow_key_events,
               const View* target_view),
              (override));
};

}  // namespace views

#endif  // UI_VIEWS_TEST_MOCK_INPUT_EVENT_ACTIVATION_PROTECTOR_H_
