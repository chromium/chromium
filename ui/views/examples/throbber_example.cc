// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/throbber_example.h"

#include <memory>

#include "base/macros.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace views {
namespace examples {

namespace {

class ThrobberView : public View {
 public:
  ThrobberView() {
    throbber_ = AddChildView(std::make_unique<Throbber>());
    throbber_->Start();
  }

  // View::
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(width(), height());
  }

  void Layout() override {
    int diameter = 16;
    throbber_->SetBounds((width() - diameter) / 2, (height() - diameter) / 2,
                         diameter, diameter);
    SizeToPreferredSize();
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (GetEventHandlerForPoint(event.location()) != throbber_)
      return false;

    if (is_checked_)
      throbber_->Start();
    else
      throbber_->Stop();
    throbber_->SetChecked(!is_checked_);
    is_checked_ = !is_checked_;
    return true;
  }

 private:
  Throbber* throbber_;
  bool is_checked_ = false;

  DISALLOW_COPY_AND_ASSIGN(ThrobberView);
};

}  // namespace

ThrobberExample::ThrobberExample()
    : ExampleBase(l10n_util::GetStringUTF8(IDS_THROBBER_SELECT_LABEL).c_str()) {
}

ThrobberExample::~ThrobberExample() = default;

void ThrobberExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FillLayout>());
  container->AddChildView(std::make_unique<ThrobberView>());
}

}  // namespace examples
}  // namespace views
