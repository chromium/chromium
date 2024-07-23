// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Very quick HSV primer for those unfamiliar with it:
// It helps to think of HSV like this:
//   h is in (0,360) and draws a circle of colors, with r = 0, b = 120, g = 240
//   s is in (0,1) and is the distance from the center of that circle - higher
//     values are more intense, with s = 0 being white, s = 1 being full color
// and then HSV is the 3d space caused by projecting that circle into a
// cylinder, with v in (0,1) being how far along the cylinder you are; v = 0 is
// black, v = 1 is full color intensity

#include <tuple>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/background.h"
#include "ui/views/color_chooser/color_chooser_listener.h"
#include "ui/views/color_chooser/color_chooser_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_utils.h"

namespace {

class TestChooserListener : public views::ColorChooserListener {
 public:
  void OnColorChosen(SkColor color) override { color_ = color; }
  void OnColorChooserDialogClosed() override { closed_ = true; }

 private:
  SkColor color_ = SK_ColorTRANSPARENT;
  bool closed_ = false;
};

class ColorChooserTest : public views::ViewsTestBase {
 public:
  ~ColorChooserTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    chooser_ = std::make_unique<views::ColorChooser>(&listener_, SK_ColorGREEN);

    // Icky: we can't use our own WidgetDelegate for CreateTestWidget, but we
    // want to follow the production code path here regardless, so we create our
    // own delegate, pull the contents view out of it, and stick it into the
    // test widget. In production Views would handle that step itself.
    auto delegate = chooser_->MakeWidgetDelegate();
    auto* view = delegate->TransferOwnershipOfContentsView();

    view->SetBounds(0, 0, 400, 300);
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                               views::Widget::InitParams::TYPE_WINDOW);
    widget_->GetContentsView()->AddChildView(std::move(view));
    generator_ = std::make_unique<ui::test::EventGenerator>(
        views::GetRootWindow(widget_.get()), widget_->GetNativeWindow());
  }

  void TearDown() override {
    generator_.reset();
    chooser_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  views::ColorChooser* chooser() { return chooser_.get(); }
  ui::test::EventGenerator* generator() { return generator_.get(); }

  void ExpectExactHSV(float h, float s, float v) const {
    EXPECT_EQ(h, chooser_->hue());
    EXPECT_EQ(s, chooser_->saturation());
    EXPECT_EQ(v, chooser_->value());
  }

  void ExpectApproximateHSV(float h, float s, float v) const {
    // At the usual size of the hue chooser it's possible to hit within
    // roughly 5 points of hue in either direction.
    EXPECT_NEAR(chooser_->hue(), h, 10.0);
    EXPECT_NEAR(chooser_->saturation(), s, 0.1);
    EXPECT_NEAR(chooser_->value(), v, 0.1);
  }

  SkColor GetShownColor() const {
    return chooser_->selected_color_patch_for_testing()
        ->background()
        ->get_color();
  }

  SkColor GetTextualColor() const {
    std::u16string text = chooser_->textfield_for_testing()->GetText();
    if (text.empty() || text[0] != '#')
      return SK_ColorTRANSPARENT;

    uint32_t color;
    return base::HexStringToUInt(base::UTF16ToUTF8(text.substr(1)), &color)
               ? SkColorSetA(color, SK_AlphaOPAQUE)
               : SK_ColorTRANSPARENT;
  }

  void TypeColor(const std::string& color) {
    chooser_->textfield_for_testing()->SetText(base::UTF8ToUTF16(color));
    // Synthesize ContentsChanged, since Textfield normally doesn't deliver it
    // for SetText, only for user-typed text.
    chooser_->ContentsChanged(chooser_->textfield_for_testing(),
                              chooser_->textfield_for_testing()->GetText());
  }

  void PressMouseAt(views::View* view, const gfx::Point& p) {
#if 0
    // TODO(ellyjones): Why doesn't this work?
    const gfx::Point po = view->GetBoundsInScreen().origin();
    generator_->MoveMouseTo(po + p.OffsetFromOrigin());
    generator_->ClickLeftButton();
#endif
    ui::MouseEvent press(ui::EventType::kMousePressed,
                         gfx::Point(view->x() + p.x(), view->y() + p.y()),
                         gfx::Point(0, 0), base::TimeTicks::Now(), 0, 0);
    view->OnMousePressed(press);
  }

 private:
  TestChooserListener listener_;
  std::unique_ptr<views::ColorChooser> chooser_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
};

TEST_F(ColorChooserTest, ShowsInitialColor) {
  ExpectExactHSV(120, 1, 1);
  EXPECT_EQ(GetShownColor(), SK_ColorGREEN);
  EXPECT_EQ(GetTextualColor(), SK_ColorGREEN);
}

TEST_F(ColorChooserTest, AdjustingTextAdjustsShown) {
  TypeColor("#ff0000");
  ExpectExactHSV(0, 1, 1);
  EXPECT_EQ(GetShownColor(), SK_ColorRED);

  TypeColor("0000ff");
  ExpectExactHSV(240, 1, 1);
  EXPECT_EQ(GetShownColor(), SK_ColorBLUE);
}

TEST_F(ColorChooserTest, HueSliderChangesHue) {
  ExpectExactHSV(120, 1, 1);

  views::View* hv = chooser()->hue_view_for_testing();

  PressMouseAt(hv, gfx::Point(1, hv->height()));
  ExpectApproximateHSV(0, 1, 1);

  PressMouseAt(hv, gfx::Point(1, (hv->height() * 3) / 4));
  ExpectApproximateHSV(90, 1, 1);

  PressMouseAt(hv, gfx::Point(1, hv->height() / 2));
  ExpectApproximateHSV(180, 1, 1);

  PressMouseAt(hv, gfx::Point(1, hv->height() / 4));
  ExpectApproximateHSV(270, 1, 1);
}

// Missing tests, TODO:
// TEST_F(ColorChooserTest, SatValueChooserChangesSatValue)
// TEST_F(ColorChooserTest, UpdateFromWebUpdatesShownValues)
// TEST_F(ColorChooserTest, AdjustingTextAffectsHue)
// TEST_F(ColorChooserTest, AdjustingTextAffectsSatValue)

}  // namespace
