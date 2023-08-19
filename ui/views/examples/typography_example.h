// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TYPOGRAPHY_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TYPOGRAPHY_EXAMPLE_H_

#include "ui/views/examples/example_base.h"

namespace views::examples {

class VIEWS_EXAMPLES_EXPORT TypographyExample : public ExampleBase {
 public:
  TypographyExample();

  TypographyExample(const TypographyExample&) = delete;
  TypographyExample& operator=(const TypographyExample&) = delete;

  ~TypographyExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_TYPOGRAPHY_EXAMPLE_H_
