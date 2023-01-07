// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_SLIDER_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_SLIDER_EXAMPLE_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/controls/slider.h"
#include "ui/views/examples/example_base.h"

namespace views {
class Label;

namespace examples {

class VIEWS_EXAMPLES_EXPORT SliderExample : public ExampleBase,
                                            public SliderListener {
 public:
  SliderExample();

  SliderExample(const SliderExample&) = delete;
  SliderExample& operator=(const SliderExample&) = delete;

  ~SliderExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  // SliderListener:
  void SliderValueChanged(Slider* sender,
                          float value,
                          float old_value,
                          SliderChangeReason reason) override;

  raw_ptr<Slider> slider_default_ = nullptr;
  raw_ptr<Slider> slider_minimal_ = nullptr;
  raw_ptr<Label> label_default_ = nullptr;
  raw_ptr<Label> label_minimal_ = nullptr;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_SLIDER_EXAMPLE_H_
