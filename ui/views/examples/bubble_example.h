// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_BUBBLE_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_BUBBLE_EXAMPLE_H_

#include "base/macros.h"
#include "ui/events/event.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/views_examples_export.h"

namespace views {

class Button;

namespace examples {

// A Bubble example.
class VIEWS_EXAMPLES_EXPORT BubbleExample : public ExampleBase {
 public:
  BubbleExample();
  ~BubbleExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  void ShowBubble(Button** button,
                  BubbleBorder::Shadow shadow,
                  bool persistent,
                  const ui::Event& event);

  Button* no_shadow_legacy_;
  Button* standard_shadow_;
  Button* no_shadow_;
  Button* persistent_;

  DISALLOW_COPY_AND_ASSIGN(BubbleExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_BUBBLE_EXAMPLE_H_
