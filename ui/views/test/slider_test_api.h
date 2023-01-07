// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_SLIDER_TEST_API_H_
#define UI_VIEWS_TEST_SLIDER_TEST_API_H_

#include "base/memory/raw_ptr.h"

namespace views {

class Slider;
class SliderListener;

namespace test {

// Can be used to update the private state of a views::Slider instance during a
// test.  Updating the private state of an already created instance reduces
// the amount of test setup and test fixture code required.
class SliderTestApi {
 public:
  explicit SliderTestApi(Slider* slider);

  SliderTestApi(const SliderTestApi&) = delete;
  SliderTestApi& operator=(const SliderTestApi&) = delete;

  virtual ~SliderTestApi();

  // Set the SliderListener on the Slider.
  void SetListener(SliderListener* listener);

  int initial_button_offset() const;

 private:
  raw_ptr<Slider> slider_;
};

}  // namespace test

}  // namespace views

#endif  // UI_VIEWS_TEST_SLIDER_TEST_API_H_
