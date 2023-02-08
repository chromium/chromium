// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_BADGE_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_BADGE_EXAMPLE_H_

#include <memory>

#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/examples/example_base.h"

namespace views::examples {

// BadgeExample demonstrates how to use the generic views::Badge and
// the New Badge in a MenuItemView.
class VIEWS_EXAMPLES_EXPORT BadgeExample : public ExampleBase {
 public:
  BadgeExample();

  BadgeExample(const BadgeExample&) = delete;
  BadgeExample& operator=(const BadgeExample&) = delete;

  ~BadgeExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  std::unique_ptr<MenuRunner> menu_runner_;
  MenuDelegate menu_delegate_;
  raw_ptr<View> menu_button_;
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_BADGE_EXAMPLE_H_
