// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_SQUARE_INK_DROP_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_SQUARE_INK_DROP_EXAMPLE_H_

#include "ui/views/examples/ink_drop_example.h"

namespace views {
namespace examples {

class VIEWS_EXAMPLES_EXPORT SquareInkDropExample : public InkDropExample {
 public:
  SquareInkDropExample();
  SquareInkDropExample(const SquareInkDropExample&) = delete;
  SquareInkDropExample& operator=(const SquareInkDropExample&) = delete;
  ~SquareInkDropExample() override;

 protected:
  void CreateInkDrop() override;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_SQUARE_INK_DROP_EXAMPLE_H_
