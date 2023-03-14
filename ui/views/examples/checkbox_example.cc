// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/checkbox_example.h"

#include "base/functional/bind.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views::examples {

CheckboxExample::CheckboxExample() : ExampleBase("Checkbox") {}

CheckboxExample::~CheckboxExample() = default;

void CheckboxExample::CreateExampleView(View* container) {
  Builder<View>(container)
      .SetUseDefaultFillLayout(true)
      .AddChild(Builder<FlexLayoutView>()
                    .SetOrientation(LayoutOrientation::kVertical)
                    .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
                    .AddChildren(Builder<Checkbox>()
                                     .SetText(u"Checkbox")
                                     .SetCallback(base::BindRepeating(
                                         [](int* count) {
                                           PrintStatus("Pressed! count: %d",
                                                       ++(*count));
                                         },
                                         &count_)),
                                 Builder<Checkbox>()
                                     .SetText(u"Disabled Unchecked")
                                     .SetState(Button::STATE_DISABLED),
                                 Builder<Checkbox>()
                                     .SetText(u"Disabled Checked")
                                     .SetChecked(true)
                                     .SetState(Button::STATE_DISABLED)))
      .BuildChildren();
}

}  // namespace views::examples
