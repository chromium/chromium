// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_MULTILINE_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_MULTILINE_EXAMPLE_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/examples/example_base.h"

namespace views {

class Checkbox;
class Label;

namespace examples {

// An example that compares the multiline rendering of different controls.
class VIEWS_EXAMPLES_EXPORT MultilineExample : public ExampleBase,
                                               public TextfieldController {
 public:
  MultilineExample();

  MultilineExample(const MultilineExample&) = delete;
  MultilineExample& operator=(const MultilineExample&) = delete;

  ~MultilineExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  class RenderTextView;

  // TextfieldController:
  void ContentsChanged(Textfield* sender,
                       const std::u16string& new_contents) override;

  raw_ptr<RenderTextView> render_text_view_ = nullptr;
  raw_ptr<Label> label_ = nullptr;
  raw_ptr<Textfield> textfield_ = nullptr;

  // Checkbox to enable and disable text rendering in |label_|.
  raw_ptr<Checkbox> label_checkbox_ = nullptr;

  // Checkbox to toggle text elision in |render_text_view_|.
  raw_ptr<Checkbox> elision_checkbox_ = nullptr;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_MULTILINE_EXAMPLE_H_
