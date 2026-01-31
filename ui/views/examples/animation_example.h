// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_ANIMATION_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_ANIMATION_EXAMPLE_H_

#include <memory>

#include "ui/views/animation/animation_abort_handle.h"
#include "ui/views/examples/example_base.h"

namespace views::examples {

class VIEWS_EXAMPLES_EXPORT AnimationExample : public ExampleBase {
 public:
  AnimationExample();
  AnimationExample(const AnimationExample&) = delete;
  AnimationExample& operator=(const AnimationExample&) = delete;
  ~AnimationExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  void StartAnimations();
  void AbortAnimations();
  void UpdateButtonsState(bool animation_running);

  raw_ptr<View> squares_container_ = nullptr;
  raw_ptr<View> start_button_ = nullptr;
  raw_ptr<View> abort_button_ = nullptr;
  std::unique_ptr<AnimationAbortHandle> animation_abort_handle_;
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_ANIMATION_EXAMPLE_H_
