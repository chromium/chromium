// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/ink_drop_ripple_test_api.h"

namespace views {
namespace test {

InkDropRippleTestApi::InkDropRippleTestApi(InkDropRipple* ink_drop_ripple)
    : ui::test::MultiLayerAnimatorTestController(this),
      ink_drop_ripple_(ink_drop_ripple) {}

InkDropRippleTestApi::~InkDropRippleTestApi() = default;

std::vector<ui::LayerAnimator*> InkDropRippleTestApi::GetLayerAnimators() {
  return std::vector<ui::LayerAnimator*>();
}

}  // namespace test
}  // namespace views
