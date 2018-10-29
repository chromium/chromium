// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"
#include "ui/views/animation/test/test_ink_drop.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"

using base::ASCIIToUTF16;

namespace {

gfx::ImageSkia CreateTestImage(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

}  // namespace

namespace views {

// Testing button that exposes protected methods.
class TestLabelButton : public LabelButton {
 public:
  explicit TestLabelButton(const base::string16& text = base::string16(),
                           int button_context = style::CONTEXT_BUTTON)
      : LabelButton(nullptr, text, button_context) {}

  using LabelButton::label;
  using LabelButton::image;
  using LabelButton::ResetColorsFromNativeTheme;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLabelButton);
};

class LabelButtonTest : public test::WidgetTest {
 public:
  LabelButtonTest() {}

  // Adds a LabelButton to the test Widget with the STYLE_BUTTON platform style.
  TestLabelButton* AddStyledButton(const char* label, bool is_default) {
    TestLabelButton* button = new TestLabelButton;
    button->SetText(ASCIIToUTF16(label));
    button->SetStyleDeprecated(Button::STYLE_BUTTON);
    if (is_default)
      button->SetIsDefault(true);
    button_->GetWidget()->GetContentsView()->AddChildView(button);
    button->SizeToPreferredSize();
    button->Layout();
    return button;
  }

  // testing::Test:
  void SetUp() override {
    WidgetTest::SetUp();
    // Make a Widget to host the button. This ensures appropriate borders are
    // used (which could be derived from the Widget's NativeTheme).
    test_widget_ = CreateTopLevelPlatformWidget();

    button_ = new TestLabelButton;
    test_widget_->GetContentsView()->AddChildView(button_);

    // Establish the expected text colors for testing changes due to state.
    themed_normal_text_color_ = button_->GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_LabelEnabledColor);

    // For styled buttons only, platforms other than Desktop Linux either ignore
    // NativeTheme and use a hardcoded black or (on Mac) have a NativeTheme that
    // reliably returns black.
    styled_normal_text_color_ = SK_ColorBLACK;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    // The Linux theme provides a non-black highlight text color, but it's not
    // used for styled buttons.
    styled_highlight_text_color_ = styled_normal_text_color_ =
        button_->GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_ButtonEnabledColor);
#else
    styled_highlight_text_color_ = styled_normal_text_color_;
#endif
  }

  void TearDown() override {
    test_widget_->CloseNow();
    WidgetTest::TearDown();
  }

 protected:
  TestLabelButton* button_ = nullptr;

  SkColor themed_normal_text_color_ = 0;
  SkColor styled_normal_text_color_ = 0;
  SkColor styled_highlight_text_color_ = 0;

 private:
  Widget* test_widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LabelButtonTest);
};

TEST_F(LabelButtonTest, Init) {
  const base::string16 text(ASCIIToUTF16("abc"));
  TestLabelButton button(text);

  EXPECT_TRUE(button.GetImage(Button::STATE_NORMAL).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_HOVERED).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_PRESSED).isNull());
  EXPECT_TRUE(button.GetImage(Button::STATE_DISABLED).isNull());

  EXPECT_EQ(text, button.GetText());

  ui::AXNodeData accessible_node_data;
  button.GetAccessibleNodeData(&accessible_node_data);
  EXPECT_EQ(ax::mojom::Role::kButton, accessible_node_data.role);
  EXPECT_EQ(text, accessible_node_data.GetString16Attribute(
                      ax::mojom::StringAttribute::kName));

  EXPECT_FALSE(button.is_default());
  EXPECT_EQ(button.style(), Button::STYLE_TEXTBUTTON);
  EXPECT_EQ(Button::STATE_NORMAL, button.state());

  EXPECT_EQ(button.image()->parent(), &button);
  EXPECT_EQ(button.label()->parent(), &button);
}

TEST_F(LabelButtonTest, Label) {
  EXPECT_TRUE(button_->GetText().empty());

  const gfx::FontList font_list = button_->label()->font_list();
  const base::string16 short_text(ASCIIToUTF16("abcdefghijklm"));
  const base::string16 long_text(ASCIIToUTF16("abcdefghijklmnopqrstuvwxyz"));
  const int short_text_width = gfx::GetStringWidth(short_text, font_list);
  const int long_text_width = gfx::GetStringWidth(long_text, font_list);

  EXPECT_LT(button_->GetPreferredSize().width(), short_text_width);
  button_->SetText(short_text);
  EXPECT_GT(button_->GetPreferredSize().height(), font_list.GetHeight());
  EXPECT_GT(button_->GetPreferredSize().width(), short_text_width);
  EXPECT_LT(button_->GetPreferredSize().width(), long_text_width);
  button_->SetText(long_text);
  EXPECT_GT(button_->GetPreferredSize().width(), long_text_width);
  button_->SetText(short_text);
  EXPECT_GT(button_->GetPreferredSize().width(), short_text_width);
  EXPECT_LT(button_->GetPreferredSize().width(), long_text_width);

  // Clamp the size to a maximum value.
  button_->SetText(long_text);
  button_->SetMaxSize(gfx::Size(short_text_width, 1));
  EXPECT_EQ(button_->GetPreferredSize(), gfx::Size(short_text_width, 1));

  // Clamp the size to a minimum value.
  button_->SetText(short_text);
  button_->SetMaxSize(gfx::Size());
  button_->SetMinSize(gfx::Size(long_text_width, font_list.GetHeight() * 2));
  EXPECT_EQ(button_->GetPreferredSize(),
            gfx::Size(long_text_width, font_list.GetHeight() * 2));
}

// Test behavior of View::GetAccessibleNodeData() for buttons when setting a
// label.
TEST_F(LabelButtonTest, AccessibleState) {
  ui::AXNodeData accessible_node_data;

  button_->GetAccessibleNodeData(&accessible_node_data);
  EXPECT_EQ(ax::mojom::Role::kButton, accessible_node_data.role);
  EXPECT_EQ(base::string16(), accessible_node_data.GetString16Attribute(
                                  ax::mojom::StringAttribute::kName));

  // Without a label (e.g. image-only), the accessible name should automatically
  // be set from the tooltip.
  const base::string16 tooltip_text = ASCIIToUTF16("abc");
  button_->SetTooltipText(tooltip_text);
  button_->GetAccessibleNodeData(&accessible_node_data);
  EXPECT_EQ(tooltip_text, accessible_node_data.GetString16Attribute(
                              ax::mojom::StringAttribute::kName));
  EXPECT_EQ(base::string16(), button_->GetText());

  // Setting a label overrides the tooltip text.
  const base::string16 label_text = ASCIIToUTF16("def");
  button_->SetText(label_text);
  button_->GetAccessibleNodeData(&accessible_node_data);
  EXPECT_EQ(label_text, accessible_node_data.GetString16Attribute(
                            ax::mojom::StringAttribute::kName));
  EXPECT_EQ(label_text, button_->GetText());

  base::string16 tooltip;
  EXPECT_TRUE(button_->GetTooltipText(gfx::Point(), &tooltip));
  EXPECT_EQ(tooltip_text, tooltip);
}

TEST_F(LabelButtonTest, Image) {
  const int small_size = 50, large_size = 100;
  const gfx::ImageSkia small_image = CreateTestImage(small_size, small_size);
  const gfx::ImageSkia large_image = CreateTestImage(large_size, large_size);

  EXPECT_LT(button_->GetPreferredSize().width(), small_size);
  EXPECT_LT(button_->GetPreferredSize().height(), small_size);
  button_->SetImage(Button::STATE_NORMAL, small_image);
  EXPECT_GT(button_->GetPreferredSize().width(), small_size);
  EXPECT_GT(button_->GetPreferredSize().height(), small_size);
  EXPECT_LT(button_->GetPreferredSize().width(), large_size);
  EXPECT_LT(button_->GetPreferredSize().height(), large_size);
  button_->SetImage(Button::STATE_NORMAL, large_image);
  EXPECT_GT(button_->GetPreferredSize().width(), large_size);
  EXPECT_GT(button_->GetPreferredSize().height(), large_size);
  button_->SetImage(Button::STATE_NORMAL, small_image);
  EXPECT_GT(button_->GetPreferredSize().width(), small_size);
  EXPECT_GT(button_->GetPreferredSize().height(), small_size);
  EXPECT_LT(button_->GetPreferredSize().width(), large_size);
  EXPECT_LT(button_->GetPreferredSize().height(), large_size);

  // Clamp the size to a maximum value.
  button_->SetImage(Button::STATE_NORMAL, large_image);
  button_->SetMaxSize(gfx::Size(large_size, 1));
  EXPECT_EQ(button_->GetPreferredSize(), gfx::Size(large_size, 1));

  // Clamp the size to a minimum value.
  button_->SetImage(Button::STATE_NORMAL, small_image);
  button_->SetMaxSize(gfx::Size());
  button_->SetMinSize(gfx::Size(large_size, large_size));
  EXPECT_EQ(button_->GetPreferredSize(), gfx::Size(large_size, large_size));
}

TEST_F(LabelButtonTest, LabelAndImage) {
  const gfx::FontList font_list = button_->label()->font_list();
  const base::string16 text(ASCIIToUTF16("abcdefghijklm"));
  const int text_width = gfx::GetStringWidth(text, font_list);

  const int image_size = 50;
  const gfx::ImageSkia image = CreateTestImage(image_size, image_size);
  ASSERT_LT(font_list.GetHeight(), image_size);

  EXPECT_LT(button_->GetPreferredSize().width(), text_width);
  EXPECT_LT(button_->GetPreferredSize().width(), image_size);
  EXPECT_LT(button_->GetPreferredSize().height(), image_size);
  button_->SetText(text);
  EXPECT_GT(button_->GetPreferredSize().width(), text_width);
  EXPECT_GT(button_->GetPreferredSize().height(), font_list.GetHeight());
  EXPECT_LT(button_->GetPreferredSize().width(), text_width + image_size);
  EXPECT_LT(button_->GetPreferredSize().height(), image_size);
  button_->SetImage(Button::STATE_NORMAL, image);
  EXPECT_GT(button_->GetPreferredSize().width(), text_width + image_size);
  EXPECT_GT(button_->GetPreferredSize().height(), image_size);

  // Layout and ensure the image is left of the label except for ALIGN_RIGHT.
  // (A proper parent view or layout manager would Layout on its invalidations).
  // Also make sure CENTER alignment moves the label compared to LEFT alignment.
  gfx::Size button_size = button_->GetPreferredSize();
  button_size.Enlarge(50, 0);
  button_->SetSize(button_size);
  button_->Layout();
  EXPECT_LT(button_->image()->bounds().right(), button_->label()->bounds().x());
  int left_align_label_midpoint = button_->label()->bounds().CenterPoint().x();
  button_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  button_->Layout();
  EXPECT_LT(button_->image()->bounds().right(), button_->label()->bounds().x());
  int center_align_label_midpoint =
      button_->label()->bounds().CenterPoint().x();
  EXPECT_LT(left_align_label_midpoint, center_align_label_midpoint);
  button_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  button_->Layout();
  EXPECT_LT(button_->label()->bounds().right(), button_->image()->bounds().x());

  button_->SetText(base::string16());
  EXPECT_LT(button_->GetPreferredSize().width(), text_width + image_size);
  EXPECT_GT(button_->GetPreferredSize().width(), image_size);
  EXPECT_GT(button_->GetPreferredSize().height(), image_size);
  button_->SetImage(Button::STATE_NORMAL, gfx::ImageSkia());
  EXPECT_LT(button_->GetPreferredSize().width(), image_size);
  EXPECT_LT(button_->GetPreferredSize().height(), image_size);

  // Clamp the size to a minimum value.
  button_->SetText(text);
  button_->SetImage(Button::STATE_NORMAL, image);
  button_->SetMinSize(gfx::Size((text_width + image_size) * 2, image_size * 2));
  EXPECT_EQ(button_->GetPreferredSize().width(), (text_width + image_size) * 2);
  EXPECT_EQ(button_->GetPreferredSize().height(), image_size * 2);

  // Clamp the size to a maximum value.
  button_->SetMinSize(gfx::Size());
  button_->SetMaxSize(gfx::Size(1, 1));
  EXPECT_EQ(button_->GetPreferredSize(), gfx::Size(1, 1));
}

TEST_F(LabelButtonTest, LabelWrapAndImageAlignment) {
  LayoutProvider* provider = LayoutProvider::Get();
  const gfx::FontList font_list = button_->label()->font_list();
  const base::string16 text(ASCIIToUTF16("abcdefghijklm abcdefghijklm"));
  const int text_wrap_width = gfx::GetStringWidth(text, font_list) / 2;
  const int image_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_LABEL_HORIZONTAL);

  button_->SetText(text);
  button_->label()->SetMultiLine(true);

  const int image_size = font_list.GetHeight();
  const gfx::ImageSkia image = CreateTestImage(image_size, image_size);
  ASSERT_EQ(font_list.GetHeight(), image.width());

  button_->SetImage(Button::STATE_NORMAL, image);
  button_->SetMaxSize(
      gfx::Size(image.width() + image_spacing + text_wrap_width, 0));

  gfx::Insets button_insets = button_->GetInsets();
  gfx::Size preferred_size = button_->GetPreferredSize();
  preferred_size.set_height(button_->GetHeightForWidth(preferred_size.width()));
  button_->SetSize(preferred_size);
  button_->Layout();

  EXPECT_EQ(preferred_size.width(),
            image.width() + image_spacing + text_wrap_width);
  EXPECT_EQ(preferred_size.height(),
            font_list.GetHeight() * 2 + button_insets.height());

  // The image should be centered on the first line of the multi-line label.
  EXPECT_EQ(button_->image()->y(),
            (font_list.GetHeight() - button_->image()->height()) / 2 +
                button_insets.top());
}

// This test was added because GetHeightForWidth and GetPreferredSize were
// inconsistent. GetPreferredSize would account for image size + insets whereas
// GetHeightForWidth wouldn't. As of writing they share a large chunk of
// logic, but this remains in place so they don't diverge as easily.
TEST_F(LabelButtonTest, GetHeightForWidthConsistentWithGetPreferredSize) {
  const base::string16 text(ASCIIToUTF16("abcdefghijklm"));
  constexpr int kTinyImageSize = 2;
  constexpr int kLargeImageSize = 50;
  const int font_height = button_->label()->font_list().GetHeight();
  // Parts of this test (accounting for label height) doesn't make sense if the
  // font is smaller than the tiny test image and insets.
  ASSERT_GT(font_height, button_->GetInsets().height() + kTinyImageSize);
  // Parts of this test (accounting for image insets) doesn't make sense if the
  // font is larger than the large test image.
  ASSERT_LT(font_height, kLargeImageSize);
  button_->SetText(text);

  for (int image_size : {kTinyImageSize, kLargeImageSize}) {
    SCOPED_TRACE(testing::Message() << "Image Size: " << image_size);
    // Set image and reset monotonic min size for every test iteration.
    const gfx::ImageSkia image = CreateTestImage(image_size, image_size);
    button_->SetImage(Button::STATE_NORMAL, image);

    const gfx::Size preferred_button_size = button_->GetPreferredSize();

    // The preferred button height should be the larger of image / label
    // heights + inset height.
    EXPECT_EQ(std::max(image_size, font_height) + button_->GetInsets().height(),
              preferred_button_size.height());

    // Make sure this preferred height is consistent with GetHeightForWidth().
    EXPECT_EQ(preferred_button_size.height(),
              button_->GetHeightForWidth(preferred_button_size.width()));
  }
}

// Ensure that the text used for button labels correctly adjusts in response
// to provided style::TextContext values.
TEST_F(LabelButtonTest, TextSizeFromContext) {
  constexpr style::TextContext kDefaultContext = style::CONTEXT_BUTTON;

  // Although CONTEXT_DIALOG_TITLE isn't used for buttons, picking a style with
  // a small delta risks finding a font with a different point-size but with the
  // same maximum glyph height.
  constexpr style::TextContext kAlternateContext = style::CONTEXT_DIALOG_TITLE;

  // First sanity that the TextConstants used in the test give different sizes.
  int default_delta, alternate_delta;
  gfx::Font::Weight default_weight, alternate_weight;
  DefaultTypographyProvider::GetDefaultFont(
      kDefaultContext, style::STYLE_PRIMARY, &default_delta, &default_weight);
  DefaultTypographyProvider::GetDefaultFont(
      kAlternateContext, style::STYLE_PRIMARY, &alternate_delta,
      &alternate_weight);
  EXPECT_LT(default_delta, alternate_delta);

  const base::string16 text(ASCIIToUTF16("abcdefghijklm"));
  button_->SetText(text);
  EXPECT_EQ(default_delta, button_->label()->font_list().GetFontSize() -
                               gfx::FontList().GetFontSize());

  TestLabelButton* alternate_button =
      new TestLabelButton(text, kAlternateContext);
  button_->parent()->AddChildView(alternate_button);
  EXPECT_EQ(alternate_delta,
            alternate_button->label()->font_list().GetFontSize() -
                gfx::FontList().GetFontSize());

  // The button size increases when the font size is increased.
  EXPECT_LT(button_->GetPreferredSize().width(),
            alternate_button->GetPreferredSize().width());
  EXPECT_LT(button_->GetPreferredSize().height(),
            alternate_button->GetPreferredSize().height());
}

TEST_F(LabelButtonTest, ChangeTextSize) {
  const base::string16 text(ASCIIToUTF16("abc"));
  const base::string16 longer_text(ASCIIToUTF16("abcdefghijklm"));
  button_->SetText(text);
  button_->SizeToPreferredSize();
  gfx::Rect bounds(button_->bounds());
  const int original_width = button_->GetPreferredSize().width();
  EXPECT_EQ(original_width, bounds.width());

  // Reserve more space in the button.
  bounds.set_width(bounds.width() * 10);
  button_->SetBoundsRect(bounds);

  // Label view in the button is sized to short text.
  const int original_label_width = button_->label()->bounds().width();

  // The button preferred size and the label size increase when the text size
  // is increased.
  button_->SetText(longer_text);
  EXPECT_GT(button_->label()->bounds().width(), original_label_width * 2);
  EXPECT_GT(button_->GetPreferredSize().width(), original_width * 2);

  // The button and the label view return to its original size when the original
  // text is restored.
  button_->SetText(text);
  EXPECT_EQ(original_label_width, button_->label()->bounds().width());
  EXPECT_EQ(original_width, button_->GetPreferredSize().width());
}

TEST_F(LabelButtonTest, ChangeLabelImageSpacing) {
  button_->SetText(ASCIIToUTF16("abc"));
  button_->SetImage(Button::STATE_NORMAL, CreateTestImage(50, 50));

  const int kOriginalSpacing = 5;
  button_->SetImageLabelSpacing(kOriginalSpacing);
  const int original_width = button_->GetPreferredSize().width();

  // Increasing the spacing between the text and label should increase the size.
  button_->SetImageLabelSpacing(2 * kOriginalSpacing);
  EXPECT_GT(button_->GetPreferredSize().width(), original_width);

  // The button shrinks if the original spacing is restored.
  button_->SetImageLabelSpacing(kOriginalSpacing);
  EXPECT_EQ(original_width, button_->GetPreferredSize().width());
}

// Ensure the label gets the correct style for default buttons (e.g. bolding)
// and button size updates correctly. Regression test for crbug.com/578722.
// Disabled on Mac. The system bold font on 10.10 doesn't get wide enough to
// change the size, but we don't use styled buttons on Mac, just MdTextButton.
#if defined(OS_MACOSX)
#define MAYBE_ButtonStyleIsDefaultStyle DISABLED_ButtonStyleIsDefaultStyle
#else
#define MAYBE_ButtonStyleIsDefaultStyle ButtonStyleIsDefaultStyle
#endif
TEST_F(LabelButtonTest, MAYBE_ButtonStyleIsDefaultStyle) {
  TestLabelButton* button = AddStyledButton("Save", false);
  gfx::Size non_default_size = button->label()->size();
  EXPECT_EQ(button->label()->GetPreferredSize().width(),
            non_default_size.width());
  EXPECT_EQ(button->label()->font_list().GetFontWeight(),
            gfx::Font::Weight::NORMAL);
  EXPECT_EQ(styled_normal_text_color_, button->label()->enabled_color());
  button->SetIsDefault(true);
  button->SizeToPreferredSize();
  button->Layout();
  EXPECT_EQ(styled_highlight_text_color_, button->label()->enabled_color());
  EXPECT_NE(non_default_size, button->label()->size());
  EXPECT_EQ(button->label()->font_list().GetFontWeight(),
            gfx::Font::Weight::BOLD);
}

// Ensure the label gets the correct style when pressed or becoming default.
TEST_F(LabelButtonTest, HighlightedButtonStyle) {
  // For STYLE_TEXTBUTTON, the NativeTheme might not provide SK_ColorBLACK, but
  // it should be the same for normal and pressed states.
  EXPECT_EQ(themed_normal_text_color_, button_->label()->enabled_color());
  button_->SetState(Button::STATE_PRESSED);
  EXPECT_EQ(themed_normal_text_color_, button_->label()->enabled_color());

  // Add a non-default button.
  TestLabelButton* styled_button = AddStyledButton("OK", false);
  EXPECT_EQ(styled_normal_text_color_, styled_button->label()->enabled_color());
  styled_button->SetState(Button::STATE_PRESSED);
  EXPECT_EQ(styled_highlight_text_color_,
            styled_button->label()->enabled_color());

  // If there's an explicit color set for STATE_PRESSED, that should be used.
  styled_button->SetEnabledTextColors(SK_ColorRED);
  EXPECT_EQ(SK_ColorRED, styled_button->label()->enabled_color());

  // Test becoming default after adding to the Widget.
  TestLabelButton* default_after = AddStyledButton("OK", false);
  EXPECT_EQ(styled_normal_text_color_, default_after->label()->enabled_color());
  default_after->SetIsDefault(true);
  EXPECT_EQ(styled_highlight_text_color_,
            default_after->label()->enabled_color());

  // Test becoming default before adding to the Widget.
  TestLabelButton* default_before = AddStyledButton("OK", true);
  EXPECT_EQ(styled_highlight_text_color_,
            default_before->label()->enabled_color());
}

// Ensure the label gets the correct enabled color after
// LabelButton::ResetColorsFromNativeTheme() is invoked.
TEST_F(LabelButtonTest, ResetColorsFromNativeTheme) {
  ASSERT_FALSE(color_utils::IsInvertedColorScheme());
  ASSERT_NE(button_->label()->background_color(), SK_ColorBLACK);
  EXPECT_EQ(themed_normal_text_color_, button_->label()->enabled_color());

  button_->label()->SetBackgroundColor(SK_ColorBLACK);
  button_->label()->SetAutoColorReadabilityEnabled(true);
  EXPECT_NE(themed_normal_text_color_, button_->label()->enabled_color());

  button_->ResetColorsFromNativeTheme();
  EXPECT_EQ(themed_normal_text_color_, button_->label()->enabled_color());
}

// Test fixture for a LabelButton that has an ink drop configured.
class InkDropLabelButtonTest : public ViewsTestBase {
 public:
  InkDropLabelButtonTest() {}

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create a widget so that the Button can query the hover state
    // correctly.
    widget_.reset(new Widget);
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 20, 20);
    widget_->Init(params);
    widget_->Show();

    button_ = new LabelButton(nullptr, base::string16());

    test_ink_drop_ = new test::TestInkDrop();
    test::InkDropHostViewTestApi(button_).SetInkDrop(
        base::WrapUnique(test_ink_drop_));

    widget_->SetContentsView(button_);
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  // Required to host the test target.
  std::unique_ptr<Widget> widget_;

  // The test target.
  LabelButton* button_ = nullptr;

  // Weak ptr, |button_| owns the instance.
  test::TestInkDrop* test_ink_drop_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropLabelButtonTest);
};

TEST_F(InkDropLabelButtonTest, HoverStateAfterMouseEnterAndExitEvents) {
  ui::test::EventGenerator event_generator(widget_->GetNativeWindow());
  const gfx::Point out_of_bounds_point(button_->bounds().bottom_right() +
                                       gfx::Vector2d(1, 1));
  const gfx::Point in_bounds_point(button_->bounds().CenterPoint());

  event_generator.MoveMouseTo(out_of_bounds_point);
  EXPECT_FALSE(test_ink_drop_->is_hovered());

  event_generator.MoveMouseTo(in_bounds_point);
  EXPECT_TRUE(test_ink_drop_->is_hovered());

  event_generator.MoveMouseTo(out_of_bounds_point);
  EXPECT_FALSE(test_ink_drop_->is_hovered());
}

// Verifies the target event handler View is the |LabelButton| and not any of
// the child Views.
TEST_F(InkDropLabelButtonTest, TargetEventHandler) {
  View* target_view = widget_->GetRootView()->GetEventHandlerForPoint(
      button_->bounds().CenterPoint());
  EXPECT_EQ(button_, target_view);
}

}  // namespace views
