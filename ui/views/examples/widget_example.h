// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_WIDGET_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_WIDGET_EXAMPLE_H_

#include <string>

#include "ui/views/examples/example_base.h"
#include "ui/views/widget/widget.h"

namespace views {

class LabelButton;

namespace examples {

// WidgetExample demonstrates how to create a popup widget.
class VIEWS_EXAMPLES_EXPORT WidgetExample : public ExampleBase {
 public:
  WidgetExample();

  WidgetExample(const WidgetExample&) = delete;
  WidgetExample& operator=(const WidgetExample&) = delete;

  ~WidgetExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

 private:
  // Construct a button with the specified |label| in |container|.
  LabelButton* BuildButton(View* container, const std::u16string& label);

  void CreateDialogWidget(View* sender, bool modal);

  // Construct a Widget for |sender|, initialize with |type|, and call Show().
  void ShowWidget(View* sender, Widget::InitParams::Type type);
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_WIDGET_EXAMPLE_H_
