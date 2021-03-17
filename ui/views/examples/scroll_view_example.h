// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_SCROLL_VIEW_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_SCROLL_VIEW_EXAMPLE_H_

#include <string>

#include "base/macros.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/examples/example_base.h"

namespace views {
namespace examples {

class VIEWS_EXAMPLES_EXPORT ScrollViewExample : public ExampleBase {
 public:
  ScrollViewExample();
  ~ScrollViewExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  class ScrollableView;

  void ButtonPressed(gfx::Rect bounds, SkColor from, SkColor to);

  // The content of the scroll view.
  ScrollableView* scrollable_;

  // The scroll view to test.
  ScrollView* scroll_view_;

  DISALLOW_COPY_AND_ASSIGN(ScrollViewExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_SCROLL_VIEW_EXAMPLE_H_
