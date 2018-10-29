// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_ANIMATED_IMAGE_VIEW_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_ANIMATED_IMAGE_VIEW_EXAMPLE_H_

#include "base/macros.h"
#include "ui/views/examples/example_base.h"

namespace views {
namespace examples {

class VIEWS_EXAMPLES_EXPORT AnimatedImageViewExample : public ExampleBase {
 public:
  AnimatedImageViewExample();
  ~AnimatedImageViewExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AnimatedImageViewExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_ANIMATED_IMAGE_VIEW_EXAMPLE_H_
