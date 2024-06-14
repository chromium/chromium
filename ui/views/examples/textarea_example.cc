// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/textarea_example.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace views::examples {

TextareaExample::TextareaExample()
    : ExampleBase(l10n_util::GetStringUTF8(IDS_TEXTAREA_SELECT_LABEL).c_str()) {
}

void TextareaExample::CreateExampleView(View* container) {
  constexpr char16_t kLongText[] =
      u"Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do "
      u"eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad "
      u"minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
      u"aliquip ex ea commodo consequat.\nDuis aute irure dolor in "
      u"reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
      u"pariatur.\n\nExcepteur sint occaecat cupidatat non proident, sunt in "
      u"culpa qui officia deserunt mollit anim id est laborum.";
  auto textarea = std::make_unique<Textarea>();
  textarea->SetText(kLongText);
  textarea->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_TEXTAREA_NAME));
  container->SetLayoutManager(std::make_unique<views::FillLayout>());
  container->AddChildView(std::move(textarea));
}

}  // namespace views::examples
