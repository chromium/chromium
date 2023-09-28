// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/throbber_example.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace views::examples {

namespace {

class ThrobberView : public View {
 public:
  METADATA_HEADER(ThrobberView);
  ThrobberView() {
    throbber_ = AddChildView(std::make_unique<Throbber>());
    throbber_->Start();
  }

  ThrobberView(const ThrobberView&) = delete;
  ThrobberView& operator=(const ThrobberView&) = delete;

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
  raw_ptr<Throbber> throbber_;
  bool is_checked_ = false;
};

BEGIN_METADATA(ThrobberView, View)
END_METADATA

}  // namespace

ThrobberExample::ThrobberExample()
    : ExampleBase(l10n_util::GetStringUTF8(IDS_THROBBER_SELECT_LABEL).c_str()) {
}

ThrobberExample::~ThrobberExample() = default;

void ThrobberExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FillLayout>());
  container->AddChildView(std::make_unique<ThrobberView>());
}

}  // namespace views::examples
