// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/bubble_example.h"

#include <array>
#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/examples/examples_color_id.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;

namespace views::examples {

namespace {

constexpr auto colors = std::to_array<ExamplesColorIds>(
    {ExamplesColorIds::kColorBubbleExampleBackground1,
     ExamplesColorIds::kColorBubbleExampleBackground2,
     ExamplesColorIds::kColorBubbleExampleBackground3,
     ExamplesColorIds::kColorBubbleExampleBackground4});

constexpr auto arrows = std::to_array<BubbleBorder::Arrow>(
    {BubbleBorder::TOP_LEFT, BubbleBorder::TOP_CENTER, BubbleBorder::TOP_RIGHT,
     BubbleBorder::RIGHT_TOP, BubbleBorder::RIGHT_CENTER,
     BubbleBorder::RIGHT_BOTTOM, BubbleBorder::BOTTOM_RIGHT,
     BubbleBorder::BOTTOM_CENTER, BubbleBorder::BOTTOM_LEFT,
     BubbleBorder::LEFT_BOTTOM, BubbleBorder::LEFT_CENTER,
     BubbleBorder::LEFT_TOP});

std::u16string GetArrowName(BubbleBorder::Arrow arrow) {
  switch (arrow) {
    case BubbleBorder::TOP_LEFT:
      return u"TOP_LEFT";
    case BubbleBorder::TOP_RIGHT:
      return u"TOP_RIGHT";
    case BubbleBorder::BOTTOM_LEFT:
      return u"BOTTOM_LEFT";
    case BubbleBorder::BOTTOM_RIGHT:
      return u"BOTTOM_RIGHT";
    case BubbleBorder::LEFT_TOP:
      return u"LEFT_TOP";
    case BubbleBorder::RIGHT_TOP:
      return u"RIGHT_TOP";
    case BubbleBorder::LEFT_BOTTOM:
      return u"LEFT_BOTTOM";
    case BubbleBorder::RIGHT_BOTTOM:
      return u"RIGHT_BOTTOM";
    case BubbleBorder::TOP_CENTER:
      return u"TOP_CENTER";
    case BubbleBorder::BOTTOM_CENTER:
      return u"BOTTOM_CENTER";
    case BubbleBorder::LEFT_CENTER:
      return u"LEFT_CENTER";
    case BubbleBorder::RIGHT_CENTER:
      return u"RIGHT_CENTER";
    case BubbleBorder::NONE:
      return u"NONE";
    case BubbleBorder::FLOAT:
      return u"FLOAT";
  }
  return u"INVALID";
}

class ExampleBubble : public BubbleDialogDelegateView {
  METADATA_HEADER(ExampleBubble, BubbleDialogDelegateView)

 public:
  ExampleBubble(View* anchor, BubbleBorder::Arrow arrow)
      : BubbleDialogDelegateView(anchor, arrow) {
    DialogDelegate::SetButtons(
        static_cast<int>(ui::mojom::DialogButton::kNone));
  }

  ExampleBubble(const ExampleBubble&) = delete;
  ExampleBubble& operator=(const ExampleBubble&) = delete;

 protected:
  void Init() override {
    SetLayoutManager(std::make_unique<BoxLayout>(
        BoxLayout::Orientation::kVertical, gfx::Insets(50)));
    AddChildView(std::make_unique<Label>(GetArrowName(arrow())));
  }
};

BEGIN_METADATA(ExampleBubble)
END_METADATA

}  // namespace

BubbleExample::BubbleExample() : ExampleBase("Bubble") {}

BubbleExample::~BubbleExample() = default;

void BubbleExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kHorizontal, gfx::Insets(), 10));

  standard_shadow_ = container->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(&BubbleExample::ShowBubble, base::Unretained(this),
                          &standard_shadow_, BubbleBorder::STANDARD_SHADOW,
                          false),
      u"Standard Shadow"));
  no_shadow_ = container->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(&BubbleExample::ShowBubble, base::Unretained(this),
                          &no_shadow_, BubbleBorder::NO_SHADOW, false),
      u"No Shadow"));
  persistent_ = container->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(&BubbleExample::ShowBubble, base::Unretained(this),
                          &persistent_, BubbleBorder::NO_SHADOW, true),
      u"Persistent"));
}

void BubbleExample::ShowBubble(raw_ptr<Button>* button,
                               BubbleBorder::Shadow shadow,
                               bool persistent,
                               const ui::Event& event) {
  static int arrow_index = 0, color_index = 0;
  static const int count = std::size(arrows);
  arrow_index = (arrow_index + count + (event.IsShiftDown() ? -1 : 1)) % count;
  BubbleBorder::Arrow arrow = arrows[arrow_index];
  if (event.IsControlDown())
    arrow = BubbleBorder::NONE;
  else if (event.IsAltDown())
    arrow = BubbleBorder::FLOAT;

  auto* provider = (*button)->GetColorProvider();
  // |bubble| will be destroyed by its widget when the widget is destroyed.
  auto bubble = std::make_unique<ExampleBubble>(*button, arrow);
  bubble->set_color(
      provider->GetColor(colors[(color_index++) % std::size(colors)]));
  bubble->set_shadow(shadow);
  if (persistent)
    bubble->set_close_on_deactivate(false);

  BubbleDialogDelegateView::CreateBubble(std::move(bubble))->Show();

  LogStatus(
      "Click with optional modifiers: [Ctrl] for set_arrow(NONE), "
      "[Alt] for set_arrow(FLOAT), or [Shift] to reverse the arrow iteration.");
}
}  // namespace views::examples
