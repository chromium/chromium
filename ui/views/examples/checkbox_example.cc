// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/checkbox_example.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/layout/fill_layout.h"

namespace views::examples {

CheckboxExample::CheckboxExample() : ExampleBase("Checkbox") {}

CheckboxExample::~CheckboxExample() = default;

void CheckboxExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FillLayout>());
  container->AddChildView(
      views::Builder<Checkbox>()
          .SetText(u"Checkbox")
          .SetCallback(base::BindRepeating(
              [](int* count) { PrintStatus("Pressed! count: %d", ++(*count)); },
              &count_))
          .Build());
}

}  // namespace views::examples
