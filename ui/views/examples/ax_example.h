// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_AX_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_AX_EXAMPLE_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/examples/example_base.h"

namespace views {

class Button;

namespace examples {

// ButtonExample simply counts the number of clicks.
class VIEWS_EXAMPLES_EXPORT AxExample : public ExampleBase {
 public:
  AxExample();

  AxExample(const AxExample&) = delete;
  AxExample& operator=(const AxExample&) = delete;

  ~AxExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  raw_ptr<Button> announce_button_ = nullptr;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_AX_EXAMPLE_H_
