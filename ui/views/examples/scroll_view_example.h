// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_SCROLL_VIEW_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_SCROLL_VIEW_EXAMPLE_H_

#include "base/memory/raw_ptr.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/examples/example_base.h"

namespace views::examples {

class VIEWS_EXAMPLES_EXPORT ScrollViewExample : public ExampleBase {
 public:
  ScrollViewExample();

  ScrollViewExample(const ScrollViewExample&) = delete;
  ScrollViewExample& operator=(const ScrollViewExample&) = delete;

  ~ScrollViewExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  class ScrollableView;

  void ButtonPressed(gfx::Rect bounds, ui::ColorId from, ui::ColorId to);

  // The content of the scroll view.
  raw_ptr<ScrollableView> scrollable_;

  // The scroll view to test.
  raw_ptr<ScrollView> scroll_view_;
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_SCROLL_VIEW_EXAMPLE_H_
