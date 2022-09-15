// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_ENV_TEST_HELPER_H_
#define UI_AURA_TEST_ENV_TEST_HELPER_H_

#include <utility>

#include "base/memory/raw_ptr.h"
#include "ui/aura/env.h"
#include "ui/aura/env_input_state_controller.h"
#include "ui/aura/input_state_lookup.h"
#include "ui/events/gestures/gesture_recognizer.h"

namespace aura {
namespace test {

class EnvTestHelper {
 public:
  EnvTestHelper() : EnvTestHelper(Env::GetInstance()) {}
  explicit EnvTestHelper(Env* env) : env_(env) {}

  EnvTestHelper(const EnvTestHelper&) = delete;
  EnvTestHelper& operator=(const EnvTestHelper&) = delete;

  ~EnvTestHelper() {}

  void SetInputStateLookup(
      std::unique_ptr<InputStateLookup> input_state_lookup) {
    env_->input_state_lookup_ = std::move(input_state_lookup);
  }

  void ResetEventState() {
    env_->mouse_button_flags_ = 0;
    env_->is_touch_down_ = false;
    env_->last_mouse_location_ = gfx::Point();
    env_->env_controller_->touch_ids_down_ = 0;
  }

  void SetGestureRecognizer(
      std::unique_ptr<ui::GestureRecognizer> gesture_recognizer) {
    env_->gesture_recognizer_ = std::move(gesture_recognizer);
  }

 private:
  raw_ptr<Env> env_;
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_ENV_TEST_HELPER_H_
