// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/textarea_example.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace views {
namespace examples {

TextareaExample::TextareaExample() : ExampleBase("Textarea") {}

void TextareaExample::CreateExampleView(View* container) {
  constexpr char kLongText[] =
      "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod"
      " tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
      "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
      "commodo consequat.\nDuis aute irure dolor in reprehenderit in voluptate "
      "velit esse cillum dolore eu fugiat nulla pariatur.\n\nExcepteur sint "
      "occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
      "mollit anim id est laborum.";
  auto textarea = std::make_unique<Textarea>();
  textarea->SetText(base::UTF8ToUTF16(kLongText));
  container->SetLayoutManager(std::make_unique<views::FillLayout>());
  container->AddChildView(std::move(textarea));
}

}  // namespace examples
}  // namespace views
