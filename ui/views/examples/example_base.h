// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_EXAMPLE_BASE_H_
#define UI_VIEWS_EXAMPLES_EXAMPLE_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/views/examples/views_examples_export.h"

namespace views {
class View;

namespace examples {

class VIEWS_EXAMPLES_EXPORT ExampleBase {
 public:
  ExampleBase(const ExampleBase&) = delete;
  ExampleBase& operator=(const ExampleBase&) = delete;

  virtual ~ExampleBase();

  // Sub-classes should creates and add the views to the given parent.
  virtual void CreateExampleView(View* parent) = 0;

  const std::string& example_title() const { return example_title_; }
  raw_ptr<View> example_view() { return container_; }
  void SetContainer(View* container) { container_ = container; }

 protected:
  explicit ExampleBase(const char* title);

 private:
  // Name of the example - used as title in the side panel.
  std::string example_title_;

  // The view that contains the views example.
  // The tab of the respective view in the tabbed pane owns the view.
  raw_ptr<View> container_;
};

using ExampleVector = std::vector<std::unique_ptr<ExampleBase>>;

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_EXAMPLE_BASE_H_
