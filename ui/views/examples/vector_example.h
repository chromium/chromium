// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_VECTOR_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_VECTOR_EXAMPLE_H_

#include <string>

#include "ui/views/examples/example_base.h"

namespace views::examples {

// Takes a string of the format expected of .icon files and strips out comments
// and extra whitespace.
VIEWS_EXAMPLES_EXPORT std::string CleanUpContents(
    const std::string& file_content);

class VIEWS_EXAMPLES_EXPORT VectorExample : public ExampleBase {
 public:
  VectorExample();

  VectorExample(const VectorExample&) = delete;
  VectorExample& operator=(const VectorExample&) = delete;

  ~VectorExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_VECTOR_EXAMPLE_H_
