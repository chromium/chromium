// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_PROGRESS_BAR_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_PROGRESS_BAR_EXAMPLE_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/examples/example_base.h"

namespace views {
class ProgressBar;

namespace examples {

class VIEWS_EXAMPLES_EXPORT ProgressBarExample : public ExampleBase {
 public:
  ProgressBarExample();

  ProgressBarExample(const ProgressBarExample&) = delete;
  ProgressBarExample& operator=(const ProgressBarExample&) = delete;

  ~ProgressBarExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  void ButtonPressed(double step);

  raw_ptr<ProgressBar> progress_bar_ = nullptr;
  double current_percent_ = 0.0;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_PROGRESS_BAR_EXAMPLE_H_
