// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/mouse/interactive_mouse_test_internal.h"

#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interactive_test_internal.h"
#include "ui/base/interaction/safe_castable.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/interaction/mouse/interaction_test_util_mouse.h"

namespace views::test::internal {

DEFINE_SAFE_CAST_TARGET(InteractiveMouseTestPrivate)

InteractiveMouseTestPrivate::InteractiveMouseTestPrivate(
    ui::test::internal::InteractiveTestPrivate& test_impl)
    : ui::test::internal::InteractiveTestPrivateFrameworkBase(test_impl) {}

InteractiveMouseTestPrivate::~InteractiveMouseTestPrivate() = default;

void InteractiveMouseTestPrivate::OnSequenceComplete() {
  if (mouse_util_) {
    mouse_util_->CancelAllGestures();
  }
}

void InteractiveMouseTestPrivate::OnSequenceAborted(
    const ui::InteractionSequence::AbortedData& data) {
  if (mouse_util_) {
    mouse_util_->CancelAllGestures();
  }
}

void InteractiveMouseTestPrivate::OnDefaultContextSet() {
  const auto window = test_impl().GetDefaultContextWindow();
  if (window) {
    mouse_util_ = std::make_unique<InteractionTestUtilMouse>(window);
  } else {
    mouse_util_.reset();
  }
}

InteractionTestUtilMouse::GestureParams
InteractiveMouseTestPrivate::GetGestureParamsForStep(
    ui::TrackedElement* el,
    const ui::InteractionSequence* seq) {
  // Get the native window.
  gfx::NativeWindow window = test_impl().GetNativeWindowFor(el);
  return InteractionTestUtilMouse::GestureParams(
      window, seq->IsCurrentStepImmediateForTesting());
}

}  // namespace views::test::internal
