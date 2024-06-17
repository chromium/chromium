// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_BUBBLE_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_BUBBLE_EXAMPLE_H_

#include "base/memory/raw_ptr.h"
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

  BubbleExample(const BubbleExample&) = delete;
  BubbleExample& operator=(const BubbleExample&) = delete;

  ~BubbleExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  void ShowBubble(raw_ptr<Button>* button,
                  BubbleBorder::Shadow shadow,
                  bool persistent,
                  const ui::Event& event);

  raw_ptr<Button> standard_shadow_;
  raw_ptr<Button> no_shadow_;
  raw_ptr<Button> persistent_;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_BUBBLE_EXAMPLE_H_
