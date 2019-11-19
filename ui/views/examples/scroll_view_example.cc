// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/scroll_view_example.h"

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

using base::ASCIIToUTF16;

namespace views {
namespace examples {

// ScrollView's content, which draws gradient color on background.
// TODO(oshima): add child views as well.
class ScrollViewExample::ScrollableView : public View {
 public:
  ScrollableView() {
    SetColor(SK_ColorRED, SK_ColorCYAN);

    auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

    const auto add_child = [this](std::unique_ptr<View> view) {
      auto* container = AddChildView(std::make_unique<View>());
      container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
      container->AddChildView(std::move(view));
    };
    add_child(std::make_unique<LabelButton>(nullptr, ASCIIToUTF16("Button")));
    add_child(std::make_unique<RadioButton>(ASCIIToUTF16("Radio Button"), 0));
    layout_manager->SetDefaultFlex(1);
  }

  void SetColor(SkColor from, SkColor to) {
    from_color_ = from;
    to_color_ = to;
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setShader(gfx::CreateGradientShader(
        gfx::Point(), gfx::Point(0, height()), from_color_, to_color_));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawRect(GetLocalBounds(), flags);
  }

 private:
  SkColor from_color_;
  SkColor to_color_;

  DISALLOW_COPY_AND_ASSIGN(ScrollableView);
};

ScrollViewExample::ScrollViewExample() : ExampleBase("Scroll View") {
}

ScrollViewExample::~ScrollViewExample() = default;

void ScrollViewExample::CreateExampleView(View* container) {
  auto scroll_view = std::make_unique<ScrollView>();
  scrollable_ = scroll_view->SetContents(std::make_unique<ScrollableView>());
  scrollable_->SetBounds(0, 0, 1000, 100);
  scrollable_->SetColor(SK_ColorYELLOW, SK_ColorCYAN);

  GridLayout* layout =
      container->SetLayoutManager(std::make_unique<views::GridLayout>());

  // Add scroll view.
  ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(1, 0);
  scroll_view_ = layout->AddView(std::move(scroll_view));

  // Add control buttons.
  column_set = layout->AddColumnSet(1);
  for (size_t i = 0; i < 5; i++) {
    column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                          GridLayout::USE_PREF, 0, 0);
  }
  layout->StartRow(0, 1);
  wide_ = layout->AddView(
      std::make_unique<LabelButton>(this, ASCIIToUTF16("Wide")));
  tall_ = layout->AddView(
      std::make_unique<LabelButton>(this, ASCIIToUTF16("Tall")));
  big_square_ = layout->AddView(
      std::make_unique<LabelButton>(this, ASCIIToUTF16("Big Square")));
  small_square_ = layout->AddView(
      std::make_unique<LabelButton>(this, ASCIIToUTF16("Small Square")));
  scroll_to_ = layout->AddView(
      std::make_unique<LabelButton>(this, ASCIIToUTF16("Scroll to")));
}

void ScrollViewExample::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == wide_) {
    scrollable_->SetBounds(0, 0, 1000, 100);
    scrollable_->SetColor(SK_ColorYELLOW, SK_ColorCYAN);
  } else if (sender == tall_) {
    scrollable_->SetBounds(0, 0, 100, 1000);
    scrollable_->SetColor(SK_ColorRED, SK_ColorCYAN);
  } else if (sender == big_square_) {
    scrollable_->SetBounds(0, 0, 1000, 1000);
    scrollable_->SetColor(SK_ColorRED, SK_ColorGREEN);
  } else if (sender == small_square_) {
    scrollable_->SetBounds(0, 0, 100, 100);
    scrollable_->SetColor(SK_ColorYELLOW, SK_ColorGREEN);
  } else if (sender == scroll_to_) {
    scroll_view_->contents()->ScrollRectToVisible(
        gfx::Rect(20, 500, 1000, 500));
  }
  scroll_view_->InvalidateLayout();
}

}  // namespace examples
}  // namespace views
