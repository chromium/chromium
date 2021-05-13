// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_LAYER_ANIMATION_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_LAYER_ANIMATION_EXAMPLE_H_

#include "base/macros.h"
#include "ui/views/examples/example_base.h"

namespace views {

namespace examples {

class VIEWS_EXAMPLES_EXPORT LayerAnimationExample : public ExampleBase {
 public:
  LayerAnimationExample();
  ~LayerAnimationExample() override;

 protected:
  // ExampleBase:
  void CreateExampleView(View* container) final;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_LAYER_ANIMATION_EXAMPLE_H_
