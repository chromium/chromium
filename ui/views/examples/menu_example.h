// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_MENU_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_MENU_EXAMPLE_H_

#include "ui/views/examples/example_base.h"

namespace views::examples {

// MenuExample demonstrates how to use the MenuModelAdapter and MenuRunner
// classes.
class VIEWS_EXAMPLES_EXPORT MenuExample : public ExampleBase {
 public:
  MenuExample();

  MenuExample(const MenuExample&) = delete;
  MenuExample& operator=(const MenuExample&) = delete;

  ~MenuExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_MENU_EXAMPLE_H_
