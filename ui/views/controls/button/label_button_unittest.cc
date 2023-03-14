// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ui_base_switches.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/test/ink_drop_host_test_api.h"
#include "ui/views/animation/test/test_ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/buildflags.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_test_api.h"
#include "ui/views/widget/widget_utils.h"

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
  explicit TestLabelButton(const std::u16string& text = std::u16string(),
                           int button_context = style::CONTEXT_BUTTON)
      : LabelButton(Button::PressedCallback(), text, button_context) {}

  TestLabelButton(const TestLabelButton&) = delete;
  TestLabelButton& operator=(const TestLabelButton&) = delete;

  void SetMultiLine(bool multi_line) { label()->SetMultiLine(multi_line); }

  using LabelButton::GetVisualState;
  using LabelButton::image;
  using LabelButton::label;
  using LabelButton::OnThemeChanged;
};

class LabelButtonTest : public test::WidgetTest {
 public:
  LabelButtonTest() = default;

  LabelButtonTest(const LabelButtonTest&) = delete;
  LabelButtonTest& operator=(const LabelButtonTest&) = delete;

  // testing::Test:
  void SetUp() override {
    WidgetTest::SetUp();
    // Make a Widget to host the button. This ensures appropriate borders are
    // used (which could be derived from the Widget's NativeTheme).
    test_widget_ = CreateTopLevelPlatformWidget();

    // The test code below is not prepared to handle dark mode.
    test_widget_->GetNativeTheme()->set_use_dark_colors(false);

    // Ensure the Widget is active, since LabelButton appearance in inactive
    // Windows is platform-dependent.
    test_widget_->Show();

    // Place the button into a separate container view which itself does no
    // layouts. This will isolate the button from the client view which does
    // a fill layout by default.
    auto* container =
        test_widget_->client_view()->AddChildView(std::make_unique<View>());
    button_ = container->AddChildView(std::make_unique<TestLabelButton>());

    // Establish the expected text colors for testing changes due to state.
    themed_normal_text_color_ =
        button_->GetColorProvider()->GetColor(ui::kColorLabelForeground);

    // For styled buttons only, platforms other than Desktop Linux either ignore
    // ColorProvider and use a hardcoded black or (on Mac) have a ColorProvider
    // that reliably returns black.
    styled_normal_text_color_ = SK_ColorBLACK;
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    BUILDFLAG(ENABLE_DESKTOP_AURA)
    // The Linux theme provides a non-black highlight text color, but it's not
    // used for styled buttons.
    styled_highlight_text_color_ = styled_normal_text_color_ =
        button_->GetColorProvider()->GetColor(ui::kColorButtonForeground);
#else
    styled_highlight_text_color_ = styled_normal_text_color_;
#endif
  }

  void TearDown() override {
    test_widget_->CloseNow();
    WidgetTest::TearDown();
  }

  void UseDarkColors() {
    ui::NativeTheme* native_theme = test_widget_->GetNativeTheme();
    native_theme->set_use_dark_colors(true);
    native_theme->NotifyOnNativeThemeUpdated();
  }

 protected:
  raw_ptr<TestLabelButton> button_ = nullptr;

  SkColor themed_normal_text_color_ = 0;
  SkColor styled_normal_text_color_ = 0;
  SkColor styled_highlight_text_color_ = 0;

 private:
  raw_ptr<Widget> test_widget_ = nullptr;
};

TEST_F(LabelButtonTest, FocusBehavior) {
  EXPECT_EQ(PlatformStyle::kDefaultFocusBehavior, button_->GetFocusBehavior());
}

TEST_F(LabelButtonTest, Init) {
  const std::u16string text(u"abc");
  button_->SetText(text);

  EXPECT_TRUE(button_->GetImage(Button::STATE_NORMAL).isNull());
  EXPECT_TRUE(button_->GetImage(Button::STATE_HOVERED).isNull());
  EXPECT_TRUE(button_->GetImage(Button::STATE_PRESSED).isNull());
  EXPECT_TRUE(button_->GetImage(Button::STATE_DISABLED).isNull());

  EXPECT_EQ(text, button_->GetText());

  ui::AXNodeData accessible_node_data;
  button_->GetAccessibleNodeData(&accessible_node_data);
  EXPECT_EQ(ax::mojom::Role::kButton, accessible_node_data.role);
  EXPECT_EQ(text, accessible_node_data.GetString16Attribute(
                      ax::mojom::StringAttribute::kName));

  EXPECT_FALSE(button_->GetIsDefault());
  EXPECT_EQ(Button::STATE_NORMAL, button_->GetState());

  EXPECT_EQ(button_->image()->parent(), button_);
  EXPECT_EQ(button_->label()->parent(), button_);
}

TEST_F(LabelButtonTest, Label) {
  EXPECT_TRUE(button_->GetText().empty());

  const gfx::FontList font_list = button_->label()->font_list();
  const std::u16string short_text(u"abcdefghijklm");
  const std::u16string long_text(u"abcdefghijklmnopqrstuvwxyz");
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
  const gfx::Size preferred_size = button_->GetPreferredSize();
  EXPECT_LE(preferred_size.width(), short_text_width);
  EXPECT_EQ(1, preferred_size.height());

  // Clamp the size to a minimum value.
  button_->SetText(short_text);
  button_->SetMaxSize(gfx::Size());
  button_->SetMinSize(gfx::Size(long_text_width, font_list.GetHeight() * 2));
  EXPECT_EQ(button_->GetPreferredSize(),
            gfx::Size(long_text_width, font_list.GetHeight() * 2));
}

// Tests LabelButton's usage of SetMaximumWidthSingleLine.
TEST_F(LabelButtonTest, LabelPreferredSizeWithMaxWidth) {
  const std::string text_cases[] = {
      {"The"},
      {"The quick"},
      {"The quick brown"},
      {"The quick brown fox"},
      {"The quick brown fox jumps"},
      {"The quick brown fox jumps over"},
      {"The quick brown fox jumps over the"},
      {"The quick brown fox jumps over the lazy"},
      {"The quick brown fox jumps over the lazy dog"},
  };

  const int width_cases[] = {
      10, 30, 50, 70, 90, 110, 130, 170, 200, 500,
  };

  for (bool is_multiline : {false, true}) {
    button_->SetMultiLine(is_multiline);
    for (bool set_image : {false, true}) {
      if (set_image)
        button_->SetImage(Button::STATE_NORMAL, CreateTestImage(16, 16));

      bool preferred_size_is_sometimes_narrower_than_max = false;
      bool preferred_height_shrinks_as_max_width_grows = false;

      for (const auto& text_case : text_cases) {
        for (int width_case : width_cases) {
          const gfx::Size old_preferred_size = button_->GetPreferredSize();

          button_->SetText(ASCIIToUTF16(text_case));
          button_->SetMaxSize(gfx::Size(width_case, 30));

          const gfx::Size preferred_size = button_->GetPreferredSize();
          EXPECT_LE(preferred_size.width(), width_case);

          if (preferred_size.width() < width_case)
            preferred_size_is_sometimes_narrower_than_max = true;

          if (preferred_size.height() < old_preferred_size.height())
            preferred_height_shrinks_as_max_width_grows = true;
        }
      }

      EXPECT_TRUE(preferred_size_is_sometimes_narrower_than_max);
      if (is_multiline)
        EXPECT_TRUE(preferred_height_shrinks_as_max_width_grows);
    }
  }
}

TEST_F(LabelButtonTest, LabelShrinkDown) {
  ASSERT_TRUE(button_->GetText().empty());

  const gfx::FontList font_list = button_->label()->font_list();
  const std::u16string text(u"abcdefghijklm");
  const int text_width = gfx::GetStringWidth(text, font_list);

  ASSERT_LT(button_->GetPreferredSize().width(), text_width);
  button_->SetText(text);
  EXPECT_GT(button_->GetPreferredSize().width(), text_width);
  button_->SetSize(button_->GetPreferredSize());

  // When shrinking, the button should report again the size with no label
  // (while keeping the label).
  button_->ShrinkDownThenClearText();
  EXPECT_EQ(button_->GetText(), text);
  EXPECT_LT(button_->GetPreferredSize().width(), text_width);

  // After the layout manager resizes the button to it's desired size, it's text
  // should be empty again.
  button_->SetSize(button_->GetPreferredSize());
  EXPECT_TRUE(button_->GetText().empty());
}

TEST_F(LabelButtonTest, LabelShrinksDownOnManualSetBounds) {
  ASSERT_TRUE(button_->GetText().empty());
  ASSERT_GT(button_->GetPreferredSize().width(), 1);

  const std::u16string text(u"abcdefghijklm");

  button_->SetText(text);
  EXPECT_EQ(button_->GetText(), text);
  button_->SetSize(button_->GetPreferredSize());
  button_->SetBoundsRect(gfx::Rect(button_->GetPreferredSize()));

  button_->ShrinkDownThenClearText();

  // Manually setting a smaller size should also clear text.
  button_->SetBoundsRect(gfx::Rect(1, 1));
  EXPECT_TRUE(button_->GetText().empty());
}

TEST_F(LabelButtonTest, LabelShrinksDownCanceledBySettingText) {
  ASSERT_TRUE(button_->GetText().empty());

  const gfx::FontList font_list = button_->label()->font_list();
  const std::u16string text(u"abcdefghijklm");
  const int text_width = gfx::GetStringWidth(text, font_list);

  ASSERT_LT(button_->GetPreferredSize().width(), text_width);
  button_->SetText(text);
  EXPECT_GT(button_->GetPreferredSize().width(), text_width);
  button_->SetBoundsRect(gfx::Rect(button_->GetPreferredSize()));

  // When shrinking, the button should report again the size with no label
  // (while keeping the label).
  button_->ShrinkDownThenClearText();
  EXPECT_EQ(button_->GetText(), text);
  gfx::Size shrinking_size = button_->GetPreferredSize();
  EXPECT_LT(shrinking_size.width(), text_width);

  // When we SetText() again, the shrinking gets canceled.
  button_->SetText(text);
  EXPECT_GT(button_->GetPreferredSize().width(), text_width);

  // Even if the layout manager resizes the button to it's size desired for
  // shrinking, it's text does not get cleared and it still prefers having space
  // for its label.
  button_->SetSize(shrinking_size);
  EXPECT_FALSE(button_->GetText().empty());
  EXPECT_GT(button_->GetPreferredSize().width(), text_width);
}

TEST_F(
    LabelButtonTest,
    LabelShrinksDownImmediatelyIfAlreadySmallerThanPreferredSizeWithoutLabel) {
  button_->SetBoundsRect(gfx::Rect(1, 1));
  button_->SetText(u"abcdefghijklm");

  // Shrinking the text down when it's already shrunk down (its size is smaller
  // than preferred without label) should clear the text immediately.
  EXPECT_FALSE(button_->GetText().empty());
  button_->ShrinkDownThenClearText();
  EXPECT_TRUE(button_->GetText().empty());
}

// Test behavior of View::GetAccessibleNodeData() for buttons when setting a
// label.
TEST_F(LabelButtonTest, AccessibleState) {
  ui::AXNodeData accessible_node_data;

  button_->GetAccessibleNodeData(&accessible_node_data);
  EXPECT_EQ(ax::mojom::Role::kButton, accessible_node_data.role);
  EXPECT_EQ(std::u16string(), accessible_node_data.GetString16Attribute(
                                  ax::mojom::StringAttribute::kName));

  // Without a label (e.g. image-only), the accessible name should automatically
  // be set from the tooltip.
  const std::u16string tooltip_text = u"abc";
  button_->SetTooltipText(tooltip_text);
  button_->GetAccessibleNodeData(&accessible_node_data);
  EXPECT_EQ(tooltip_text, accessible_node_data.GetString16Attribute(
                              ax::mojom::StringAttribute::kName));
  EXPECT_EQ(std::u16string(), button_->GetText());

  // Setting a label overrides the tooltip text.
  const std::u16string label_text = u"def";
  button_->SetText(label_text);
  button_->GetAccessibleNodeData(&accessible_node_data);
  EXPECT_EQ(label_text, accessible_node_data.GetString16Attribute(
                            ax::mojom::StringAttribute::kName));
  EXPECT_EQ(label_text, button_->GetText());
  EXPECT_EQ(tooltip_text, button_->GetTooltipText(gfx::Point()));
}

// Test View::GetAccessibleNodeData() for default buttons.
TEST_F(LabelButtonTest, AccessibleDefaultState) {
  {
    // If SetIsDefault() is not called, the ax default state should not be set.
    ui::AXNodeData ax_data;
    button_->GetViewAccessibility().GetAccessibleNodeData(&ax_data);
    EXPECT_FALSE(ax_data.HasState(ax::mojom::State::kDefault));
  }

  {
    button_->SetIsDefault(true);
    ui::AXNodeData ax_data;
    button_->GetViewAccessibility().GetAccessibleNodeData(&ax_data);
    EXPECT_TRUE(ax_data.HasState(ax::mojom::State::kDefault));
  }

  {
    button_->SetIsDefault(false);
    ui::AXNodeData ax_data;
    button_->GetViewAccessibility().GetAccessibleNodeData(&ax_data);
    EXPECT_FALSE(ax_data.HasState(ax::mojom::State::kDefault));
  }
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

TEST_F(LabelButtonTest, ImageAlignmentWithMultilineLabel) {
  const std::u16string text(
      u"Some long text that would result in multiline label");
  button_->SetText(text);

  const int max_label_width = 40;
  button_->label()->SetMultiLine(true);
  button_->label()->SetMaximumWidth(max_label_width);

  const int image_size = 16;
  const gfx::ImageSkia image = CreateTestImage(image_size, image_size);
  button_->SetImage(Button::STATE_NORMAL, image);

  button_->SetBoundsRect(gfx::Rect(button_->GetPreferredSize()));
  views::test::RunScheduledLayout(button_);
  int y_origin_centered = button_->image()->origin().y();

  button_->SetBoundsRect(gfx::Rect(button_->GetPreferredSize()));
  button_->SetImageCentered(false);
  views::test::RunScheduledLayout(button_);
  int y_origin_not_centered = button_->image()->origin().y();

  EXPECT_LT(y_origin_not_centered, y_origin_centered);
}

TEST_F(LabelButtonTest, LabelAndImage) {
  const gfx::FontList font_list = button_->label()->font_list();
  const std::u16string text(u"abcdefghijklm");
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
  views::test::RunScheduledLayout(button_);
  EXPECT_LT(button_->image()->bounds().right(), button_->label()->bounds().x());
  int left_align_label_midpoint = button_->label()->bounds().CenterPoint().x();
  button_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  views::test::RunScheduledLayout(button_);
  EXPECT_LT(button_->image()->bounds().right(), button_->label()->bounds().x());
  int center_align_label_midpoint =
      button_->label()->bounds().CenterPoint().x();
  EXPECT_LT(left_align_label_midpoint, center_align_label_midpoint);
  button_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  views::test::RunScheduledLayout(button_);
  EXPECT_LT(button_->label()->bounds().right(), button_->image()->bounds().x());

  button_->SetText(std::u16string());
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
  const std::u16string text(u"abcdefghijklm abcdefghijklm");
  const int text_wrap_width = gfx::GetStringWidth(text, font_list) / 2;
  const int image_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_LABEL_HORIZONTAL);

  button_->SetText(text);
  button_->label()->SetMultiLine(true);

  const int image_size = font_list.GetHeight();
  const gfx::ImageSkia image = CreateTestImage(image_size, image_size);
  ASSERT_EQ(font_list.GetHeight(), image.width());

  button_->SetImage(Button::STATE_NORMAL, image);
  button_->SetImageCentered(false);
  button_->SetMaxSize(
      gfx::Size(image.width() + image_spacing + text_wrap_width, 0));

  gfx::Insets button_insets = button_->GetInsets();
  gfx::Size preferred_size = button_->GetPreferredSize();
  preferred_size.set_height(button_->GetHeightForWidth(preferred_size.width()));
  button_->SetSize(preferred_size);
  views::test::RunScheduledLayout(button_);

  EXPECT_EQ(preferred_size.width(),
            image.width() + image_spacing + text_wrap_width);
  EXPECT_EQ(preferred_size.height(),
            font_list.GetHeight() * 2 + button_insets.height());

  // The image should be centered on the first line of the multi-line label
  EXPECT_EQ(button_->image()->y(),
            (font_list.GetHeight() - button_->image()->height()) / 2 +
                button_insets.top());
}

// This test was added because GetHeightForWidth and GetPreferredSize were
// inconsistent. GetPreferredSize would account for image size + insets whereas
// GetHeightForWidth wouldn't. As of writing they share a large chunk of
// logic, but this remains in place so they don't diverge as easily.
TEST_F(LabelButtonTest, GetHeightForWidthConsistentWithGetPreferredSize) {
  const std::u16string text(u"abcdefghijklm");
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
  const auto get_delta = [](auto context) {
    return TypographyProvider()
               .GetFont(context, style::STYLE_PRIMARY)
               .GetFontSize() -
           gfx::FontList().GetFontSize();
  };
  TypographyProvider typography_provider;
  int default_delta = get_delta(kDefaultContext);
  int alternate_delta = get_delta(kAlternateContext);
  EXPECT_LT(default_delta, alternate_delta);

  const std::u16string text(u"abcdefghijklm");
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
  const std::u16string text(u"abc");
  const std::u16string longer_text(u"abcdefghijklm");
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
  EXPECT_TRUE(ViewTestApi(button_).needs_layout());
  views::test::RunScheduledLayout(button_);
  EXPECT_GT(button_->label()->bounds().width(), original_label_width * 2);
  EXPECT_GT(button_->GetPreferredSize().width(), original_width * 2);

  // The button and the label view return to its original size when the original
  // text is restored.
  button_->SetText(text);
  EXPECT_TRUE(ViewTestApi(button_).needs_layout());
  views::test::RunScheduledLayout(button_);
  EXPECT_EQ(original_label_width, button_->label()->bounds().width());
  EXPECT_EQ(original_width, button_->GetPreferredSize().width());
}

TEST_F(LabelButtonTest, ChangeLabelImageSpacing) {
  button_->SetText(u"abc");
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

// Ensure the label gets the correct style when pressed or becoming default.
TEST_F(LabelButtonTest, HighlightedButtonStyle) {
  // The ColorProvider might not provide SK_ColorBLACK, but it should be the
  // same for normal and pressed states.
  EXPECT_EQ(themed_normal_text_color_, button_->label()->GetEnabledColor());
  button_->SetState(Button::STATE_PRESSED);
  EXPECT_EQ(themed_normal_text_color_, button_->label()->GetEnabledColor());
}

// Ensure the label resets the enabled color after LabelButton::OnThemeChanged()
// is invoked.
TEST_F(LabelButtonTest, OnThemeChanged) {
  ASSERT_NE(button_->GetNativeTheme()->GetPlatformHighContrastColorScheme(),
            ui::NativeTheme::PlatformHighContrastColorScheme::kDark);
  ASSERT_NE(button_->label()->GetBackgroundColor(), SK_ColorBLACK);
  EXPECT_EQ(themed_normal_text_color_, button_->label()->GetEnabledColor());

  button_->label()->SetBackgroundColor(SK_ColorBLACK);
  button_->label()->SetAutoColorReadabilityEnabled(true);
  EXPECT_NE(themed_normal_text_color_, button_->label()->GetEnabledColor());

  button_->OnThemeChanged();
  EXPECT_EQ(themed_normal_text_color_, button_->label()->GetEnabledColor());
}

TEST_F(LabelButtonTest, SetEnabledTextColorsResetsToThemeColors) {
  constexpr SkColor kReplacementColor = SK_ColorCYAN;

  // This test doesn't make sense if the used colors are equal.
  EXPECT_NE(themed_normal_text_color_, kReplacementColor);

  // Initially the test should have the normal colors.
  EXPECT_EQ(themed_normal_text_color_, button_->label()->GetEnabledColor());

  // Setting the enabled text colors should replace the label's enabled color.
  button_->SetEnabledTextColors(kReplacementColor);
  EXPECT_EQ(kReplacementColor, button_->label()->GetEnabledColor());

  // Toggle dark mode. This should not replace the enabled text color as it's
  // been manually overridden above.
  UseDarkColors();
  EXPECT_EQ(kReplacementColor, button_->label()->GetEnabledColor());

  // Removing the enabled text color restore colors from the new theme, not
  // the original colors used before the theme changed.
  button_->SetEnabledTextColors(absl::nullopt);
  EXPECT_NE(themed_normal_text_color_, button_->label()->GetEnabledColor());
}

TEST_F(LabelButtonTest, SetEnabledTextColorIds) {
  ASSERT_NE(ui::kColorLabelForeground, ui::kColorAccent);

  // Initially the test should have the normal colors.
  EXPECT_EQ(button_->label()->GetEnabledColorId(), ui::kColorLabelForeground);

  // Setting the enabled text colors should replace the label's enabled color.
  button_->SetEnabledTextColorIds(ui::kColorAccent);
  EXPECT_EQ(button_->label()->GetEnabledColorId(), ui::kColorAccent);

  // Toggle dark mode. This should not replace the enabled text color as it's
  // been manually overridden above.
  UseDarkColors();
  EXPECT_EQ(button_->label()->GetEnabledColorId(), ui::kColorAccent);
  EXPECT_EQ(button_->label()->GetEnabledColor(),
            button_->GetColorProvider()->GetColor(ui::kColorAccent));
}

TEST_F(LabelButtonTest, ImageOrLabelGetClipped) {
  const std::u16string text(u"abc");
  button_->SetText(text);

  const gfx::FontList font_list = button_->label()->font_list();
  const int image_size = font_list.GetHeight();
  button_->SetImage(Button::STATE_NORMAL,
                    CreateTestImage(image_size, image_size));

  button_->SetBoundsRect(gfx::Rect(button_->GetPreferredSize()));
  // The border size + the content height is more than button's preferred size.
  button_->SetBorder(CreateEmptyBorder(
      gfx::Insets::TLBR(image_size / 2, 0, image_size / 2, 0)));
  views::test::RunScheduledLayout(button_);

  // Ensure that content (image and label) doesn't get clipped by the border.
  EXPECT_GE(button_->image()->height(), image_size);
  EXPECT_GE(button_->label()->height(), image_size);
}

TEST_F(LabelButtonTest, UpdateImageAfterSettingImageModel) {
  auto is_showing_image = [&](const gfx::ImageSkia& image) {
    return button_->image()->GetImage().BackedBySameObjectAs(image);
  };

  auto normal_image = CreateTestImage(16, 16);
  button_->SetImageModel(Button::STATE_NORMAL,
                         ui::ImageModel::FromImageSkia(normal_image));
  EXPECT_TRUE(is_showing_image(normal_image));

  // When the button has no specific disabled image, changing the normal image
  // while the button is disabled should update the currently-visible image.
  normal_image = CreateTestImage(16, 16);
  button_->SetState(Button::STATE_DISABLED);
  button_->SetImageModel(Button::STATE_NORMAL,
                         ui::ImageModel::FromImageSkia(normal_image));
  EXPECT_TRUE(is_showing_image(normal_image));

  // Any specific disabled image should take precedence over the normal image.
  auto disabled_image = CreateTestImage(16, 16);
  button_->SetImageModel(Button::STATE_DISABLED,
                         ui::ImageModel::FromImageSkia(disabled_image));
  EXPECT_TRUE(is_showing_image(disabled_image));

  // Removing the disabled image should result in falling back to the normal
  // image again.
  button_->SetImageModel(Button::STATE_DISABLED, ui::ImageModel());
  EXPECT_TRUE(is_showing_image(normal_image));
}

// Test fixture for a LabelButton that has an ink drop configured.
class InkDropLabelButtonTest : public ViewsTestBase {
 public:
  InkDropLabelButtonTest() = default;

  InkDropLabelButtonTest(const InkDropLabelButtonTest&) = delete;
  InkDropLabelButtonTest& operator=(const InkDropLabelButtonTest&) = delete;

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create a widget so that the Button can query the hover state
    // correctly.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 20, 20);
    widget_->Init(std::move(params));
    widget_->Show();

    button_ = widget_->SetContentsView(std::make_unique<LabelButton>(
        Button::PressedCallback(), std::u16string()));

    test_ink_drop_ = new test::TestInkDrop();
    test::InkDropHostTestApi(InkDrop::Get(button_))
        .SetInkDrop(base::WrapUnique(test_ink_drop_.get()));
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  // Required to host the test target.
  std::unique_ptr<Widget> widget_;

  // The test target.
  raw_ptr<LabelButton> button_ = nullptr;

  // Weak ptr, |button_| owns the instance.
  raw_ptr<test::TestInkDrop> test_ink_drop_ = nullptr;
};

TEST_F(InkDropLabelButtonTest, HoverStateAfterMouseEnterAndExitEvents) {
  ui::test::EventGenerator event_generator(GetRootWindow(widget_.get()));
  const gfx::Point out_of_bounds_point(
      button_->GetBoundsInScreen().bottom_right() + gfx::Vector2d(1, 1));
  const gfx::Point in_bounds_point(button_->GetBoundsInScreen().CenterPoint());

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

class LabelButtonVisualStateTest : public test::WidgetTest {
 public:
  LabelButtonVisualStateTest() = default;
  LabelButtonVisualStateTest(const LabelButtonVisualStateTest&) = delete;
  LabelButtonVisualStateTest& operator=(const LabelButtonVisualStateTest&) =
      delete;

  // testing::Test:
  void SetUp() override {
    WidgetTest::SetUp();
    test_widget_ = CreateTopLevelPlatformWidget();
    dummy_widget_ = CreateTopLevelPlatformWidget();

    button_ = MakeButtonAsContent(test_widget_);

    style_of_inactive_widget_ =
        PlatformStyle::kInactiveWidgetControlsAppearDisabled
            ? Button::STATE_DISABLED
            : Button::STATE_NORMAL;
  }

  void TearDown() override {
    test_widget_->CloseNow();
    dummy_widget_->CloseNow();
    WidgetTest::TearDown();
  }

 protected:
  std::unique_ptr<Widget> CreateActivatableChildWidget(Widget* parent) {
    auto child = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.parent = parent->GetNativeView();
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.activatable = Widget::InitParams::Activatable::kYes;
    child->Init(std::move(params));
    child->SetContentsView(std::make_unique<View>());
    return child;
  }

  TestLabelButton* MakeButtonAsContent(Widget* widget) {
    return widget->GetContentsView()->AddChildView(
        std::make_unique<TestLabelButton>());
  }

  raw_ptr<TestLabelButton> button_ = nullptr;
  raw_ptr<Widget> test_widget_ = nullptr;
  raw_ptr<Widget> dummy_widget_ = nullptr;
  Button::ButtonState style_of_inactive_widget_;
};

TEST_F(LabelButtonVisualStateTest, IndependentWidget) {
  test_widget_->ShowInactive();
  EXPECT_EQ(button_->GetVisualState(), style_of_inactive_widget_);

  test_widget_->Activate();
  EXPECT_EQ(button_->GetVisualState(), Button::STATE_NORMAL);

  auto paint_as_active_lock = test_widget_->LockPaintAsActive();
  dummy_widget_->Show();
  EXPECT_EQ(button_->GetVisualState(), Button::STATE_NORMAL);
}

TEST_F(LabelButtonVisualStateTest, ChildWidget) {
  std::unique_ptr<Widget> child_widget =
      CreateActivatableChildWidget(test_widget_);
  TestLabelButton* child_button = MakeButtonAsContent(child_widget.get());

  test_widget_->Show();
  EXPECT_EQ(button_->GetVisualState(), Button::STATE_NORMAL);
  EXPECT_EQ(child_button->GetVisualState(), Button::STATE_NORMAL);

  dummy_widget_->Show();
  EXPECT_EQ(button_->GetVisualState(), style_of_inactive_widget_);
  EXPECT_EQ(child_button->GetVisualState(), style_of_inactive_widget_);

  child_widget->Show();
#if BUILDFLAG(IS_MAC)
  // Child widget is in a key window and it will lock its parent.
  // See crrev.com/c/2048144.
  EXPECT_EQ(button_->GetVisualState(), Button::STATE_NORMAL);
#else
  EXPECT_EQ(button_->GetVisualState(), style_of_inactive_widget_);
#endif
  EXPECT_EQ(child_button->GetVisualState(), Button::STATE_NORMAL);
}

}  // namespace views
