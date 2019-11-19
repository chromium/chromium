// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_EXAMPLE_BASE_H_
#define UI_VIEWS_EXAMPLES_EXAMPLE_BASE_H_

#include <string>

#include "base/macros.h"
#include "ui/views/examples/views_examples_export.h"

namespace views {
class View;

namespace examples {

class VIEWS_EXAMPLES_EXPORT ExampleBase {
 public:
  virtual ~ExampleBase();

  // Sub-classes should creates and add the views to the given parent.
  virtual void CreateExampleView(View* parent) = 0;

  const std::string& example_title() const { return example_title_; }
  View* example_view() { return container_; }

 protected:
  explicit ExampleBase(const char* title);

  // Prints a message in the status area, at the bottom of the window.
  void PrintStatus(const char* format, ...);

 private:
  // Name of the example - used as title in the combobox list.
  std::string example_title_;

  // The view that contains the views example.
  View* container_;

  DISALLOW_COPY_AND_ASSIGN(ExampleBase);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_EXAMPLE_BASE_H_
