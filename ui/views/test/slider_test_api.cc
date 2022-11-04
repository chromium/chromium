// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/slider_test_api.h"

#include "ui/views/controls/slider.h"

namespace views::test {

SliderTestApi::SliderTestApi(Slider* slider) : slider_(slider) {}

SliderTestApi::~SliderTestApi() = default;

void SliderTestApi::SetListener(SliderListener* listener) {
  slider_->set_listener(listener);
}
int SliderTestApi::initial_button_offset() const {
  return slider_->initial_button_offset_;
}

}  // namespace views::test
