// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_NATIVE_THEME_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_NATIVE_THEME_EXAMPLE_H_

#include "base/macros.h"
#include "ui/views/examples/example_base.h"

namespace views {
namespace examples {

class VIEWS_EXAMPLES_EXPORT NativeThemeExample : public ExampleBase {
 public:
  NativeThemeExample();
  ~NativeThemeExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeThemeExample);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_NATIVE_THEME_EXAMPLE_H_
