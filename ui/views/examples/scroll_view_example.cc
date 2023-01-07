// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/scroll_view_example.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/examples/examples_color_id.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

using l10n_util::GetStringUTF16;
using l10n_util::GetStringUTF8;

namespace views::examples {

// ScrollView's content, which draws gradient color on background.
// TODO(oshima): add child views as well.
class ScrollViewExample::ScrollableView : public BoxLayoutView {
 public:
  ScrollableView() {
    SetColor(ExamplesColorIds::kColorScrollViewExampleTallFrom,
             ExamplesColorIds::kColorScrollViewExampleTallTo);

    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetInsideBorderInsets(gfx::Insets());
    SetBetweenChildSpacing(0);

    const auto add_child = [this](std::unique_ptr<View> view) {
      auto* container = AddChildView(std::make_unique<BoxLayoutView>());
      container->SetOrientation(views::BoxLayout::Orientation::kVertical);
      container->AddChildView(std::move(view));
    };
    add_child(std::make_unique<LabelButton>(
        Button::PressedCallback(),
        GetStringUTF16(IDS_SCROLL_VIEW_BUTTON_LABEL)));
    add_child(std::make_unique<RadioButton>(
        GetStringUTF16(IDS_SCROLL_VIEW_RADIO_BUTTON_LABEL), 0));
    SetDefaultFlex(1);
  }

  ScrollableView(const ScrollableView&) = delete;
  ScrollableView& operator=(const ScrollableView&) = delete;

  void SetColor(ui::ColorId from_id, ui::ColorId to_id) {
    from_color_id_ = from_id;
    to_color_id_ = to_id;
  }

  std::pair<SkColor, SkColor> GetColors() const {
    auto* const cp = GetColorProvider();
    if (!cp)
      return {gfx::kPlaceholderColor, gfx::kPlaceholderColor};
    return {cp->GetColor(from_color_id_), cp->GetColor(to_color_id_)};
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    std::pair<SkColor, SkColor> gradient_colors = GetColors();
    flags.setShader(gfx::CreateGradientShader(
        gfx::Point(), gfx::Point(0, height()), gradient_colors.first,
        gradient_colors.second));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawRect(GetLocalBounds(), flags);
  }

 private:
  ui::ColorId from_color_id_;
  ui::ColorId to_color_id_;
};

ScrollViewExample::ScrollViewExample()
    : ExampleBase(GetStringUTF8(IDS_SCROLL_VIEW_SELECT_LABEL).c_str()) {}

ScrollViewExample::~ScrollViewExample() = default;

void ScrollViewExample::CreateExampleView(View* container) {
  auto scroll_view = std::make_unique<ScrollView>();
  scrollable_ = scroll_view->SetContents(std::make_unique<ScrollableView>());
  scrollable_->SetBounds(0, 0, 1000, 100);
  scrollable_->SetColor(ExamplesColorIds::kColorScrollViewExampleWideFrom,
                        ExamplesColorIds::kColorScrollViewExampleWideTo);

  container->SetLayoutManager(std::make_unique<FlexLayout>())
      ->SetOrientation(LayoutOrientation::kVertical);

  auto full_flex = FlexSpecification(MinimumFlexSizeRule::kScaleToZero,
                                     MaximumFlexSizeRule::kUnbounded)
                       .WithWeight(1);

  // Add scroll view.
  scroll_view_ = container->AddChildView(std::move(scroll_view));
  scroll_view_->SetProperty(views::kFlexBehaviorKey, full_flex);

  // Add control buttons.
  auto* button_panel = container->AddChildView(std::make_unique<View>());
  button_panel->SetLayoutManager(std::make_unique<FlexLayout>())
      ->SetOrientation(LayoutOrientation::kHorizontal);

  button_panel->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(&ScrollViewExample::ButtonPressed,
                          base::Unretained(this), gfx::Rect(0, 0, 1000, 100),
                          ExamplesColorIds::kColorScrollViewExampleWideFrom,
                          ExamplesColorIds::kColorScrollViewExampleWideTo),
      GetStringUTF16(IDS_SCROLL_VIEW_WIDE_LABEL)));
  button_panel->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(&ScrollViewExample::ButtonPressed,
                          base::Unretained(this), gfx::Rect(0, 0, 100, 1000),
                          ExamplesColorIds::kColorScrollViewExampleTallFrom,
                          ExamplesColorIds::kColorScrollViewExampleTallTo),
      GetStringUTF16(IDS_SCROLL_VIEW_TALL_LABEL)));
  button_panel->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(
          &ScrollViewExample::ButtonPressed, base::Unretained(this),
          gfx::Rect(0, 0, 1000, 1000),
          ExamplesColorIds::kColorScrollViewExampleBigSquareFrom,
          ExamplesColorIds::kColorScrollViewExampleBigSquareTo),
      GetStringUTF16(IDS_SCROLL_VIEW_BIG_SQUARE_LABEL)));
  button_panel->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(
          &ScrollViewExample::ButtonPressed, base::Unretained(this),
          gfx::Rect(0, 0, 100, 100),
          ExamplesColorIds::kColorScrollViewExampleSmallSquareFrom,
          ExamplesColorIds::kColorScrollViewExampleSmallSquareTo),
      GetStringUTF16(IDS_SCROLL_VIEW_SMALL_SQUARE_LABEL)));
  button_panel->AddChildView(std::make_unique<LabelButton>(
      base::BindRepeating(&View::ScrollRectToVisible,
                          base::Unretained(scroll_view_->contents()),
                          gfx::Rect(20, 500, 1000, 500)),
      GetStringUTF16(IDS_SCROLL_VIEW_SCROLL_TO_LABEL)));

  for (View* child : button_panel->children())
    child->SetProperty(views::kFlexBehaviorKey, full_flex);
}

void ScrollViewExample::ButtonPressed(gfx::Rect bounds,
                                      ui::ColorId from,
                                      ui::ColorId to) {
  scrollable_->SetBoundsRect(std::move(bounds));
  scrollable_->SetColor(from, to);
}

}  // namespace views::examples
