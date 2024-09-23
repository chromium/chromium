// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/label_button.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_switches.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/test/ink_drop_host_test_api.h"
#include "ui/views/animation/test/test_ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/buildflags.h"
#include "ui/views/controls/button/label_button_image_container.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_test_api.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget_utils.h"

namespace views {

using ::base::ASCIIToUTF16;

// Testing button that exposes protected methods.
class TestLabelButton : public LabelButton {
  METADATA_HEADER(TestLabelButton, LabelButton)

 public:
  explicit TestLabelButton(const std::u16string& text = std::u16string(),
                           int button_context = style::CONTEXT_BUTTON)
      : LabelButton(Button::PressedCallback(), text, button_context) {}

  TestLabelButton(const TestLabelButton&) = delete;
  TestLabelButton& operator=(const TestLabelButton&) = delete;

  void SetMultiLine(bool multi_line) { label()->SetMultiLine(multi_line); }

  using LabelButton::GetVisualState;
  using LabelButton::image_container_view;
  using LabelButton::label;
  using LabelButton::OnThemeChanged;
};

BEGIN_METADATA(TestLabelButton)
END_METADATA

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
    auto* button = container->AddChildView(std::make_unique<TestLabelButton>());
    button->SetID(1);

    // Establish the expected text colors for testing changes due to state.
    themed_normal_text_color_ =
        button->GetColorProvider()->GetColor(ui::kColorLabelForeground);

    // For styled buttons only, platforms other than Desktop Linux either ignore
    // ColorProvider and use a hardcoded black or (on Mac) have a ColorProvider
    // that reliably returns black.
    styled_normal_text_color_ = SK_ColorBLACK;
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    BUILDFLAG(ENABLE_DESKTOP_AURA)
    // The Linux theme provides a non-black highlight text color, but it's not
    // used for styled buttons.
    styled_highlight_text_color_ = styled_normal_text_color_ =
        button->GetColorProvider()->GetColor(ui::kColorButtonForeground);
#else
    styled_highlight_text_color_ = styled_normal_text_color_;
#endif
  }

  void TearDown() override {
    test_widget_.ExtractAsDangling()->CloseNow();
    WidgetTest::TearDown();
  }

  void UseDarkColors() {
    ui::NativeTheme* native_theme = test_widget_->GetNativeTheme();
    native_theme->set_use_dark_colors(true);
    native_theme->NotifyOnNativeThemeUpdated();
  }

 protected:
  TestLabelButton* button() {
    return static_cast<TestLabelButton*>(
        test_widget_->GetContentsView()->GetViewByID(1));
  }

  SkColor themed_normal_text_color_ = 0;
  SkColor styled_normal_text_color_ = 0;
  SkColor styled_highlight_text_color_ = 0;

 private:
  raw_ptr<Widget> test_widget_ = nullptr;
};

TEST_F(LabelButtonTest, FocusBehavior) {
  EXPECT_EQ(PlatformStyle::kDefaultFocusBehavior, button()->GetFocusBehavior());
}

TEST_F(LabelButtonTest, Init) {
  const std::u16string text(u"abc");
  button()->SetText(text);

  EXPECT_TRUE(button()->GetImage(Button::STATE_NORMAL).isNull());
  EXPECT_TRUE(button()->GetImage(Button::STATE_HOVERED).isNull());
  EXPECT_TRUE(button()->GetImage(Button::STATE_PRESSED).isNull());
  EXPECT_TRUE(button()->GetImage(Button::STATE_DISABLED).isNull());

  EXPECT_EQ(text, button()->GetText());

  ui::AXNodeData accessible_node_data;
  button()->GetViewAccessibility().GetAccessibleNodeData(&accessible_node_data);
  EXPECT_EQ(ax::mojom::Role::kButton, accessible_node_data.role);
  EXPECT_EQ(text, accessible_node_data.GetString16Attribute(
                      ax::mojom::StringAttribute::kName));

  EXPECT_FALSE(button()->GetIsDefault());
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());

  EXPECT_EQ(button()->image_container_view()->parent(), button());
  EXPECT_EQ(button()->label()->parent(), button());
}

TEST_F(LabelButtonTest, Label) {
  EXPECT_TRUE(button()->GetText().empty());

  const gfx::FontList font_list = button()->label()->font_list();
  const std::u16string short_text(u"abcdefghijklm");
  const std::u16string long_text(u"abcdefghijklmnopqrstuvwxyz");
  const int short_text_width = gfx::GetStringWidth(short_text, font_list);
  const int long_text_width = gfx::GetStringWidth(long_text, font_list);

  EXPECT_LT(button()->GetPreferredSize({}).width(), short_text_width);
  button()->SetText(short_text);
  EXPECT_GT(button()->GetPreferredSize({}).height(), font_list.GetHeight());
  EXPECT_GT(button()->GetPreferredSize({}).width(), short_text_width);
  EXPECT_LT(button()->GetPreferredSize({}).width(), long_text_width);
  button()->SetText(long_text);
  EXPECT_GT(button()->GetPreferredSize({}).width(), long_text_width);
  button()->SetText(short_text);
  EXPECT_GT(button()->GetPreferredSize({}).width(), short_text_width);
  EXPECT_LT(button()->GetPreferredSize({}).width(), long_text_width);

  // Clamp the size to a maximum value.
  button()->SetText(long_text);
  button()->SetMaxSize(gfx::Size(short_text_width, 1));
  const gfx::Size preferred_size = button()->GetPreferredSize({});
  EXPECT_LE(preferred_size.width(), short_text_width);
  EXPECT_EQ(1, preferred_size.height());

  // Clamp the size to a minimum value.
  button()->SetText(short_text);
  button()->SetMaxSize(gfx::Size());
  button()->SetMinSize(gfx::Size(long_text_width, font_list.GetHeight() * 2));
  EXPECT_EQ(button()->GetPreferredSize({}),
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
    button()->SetMultiLine(is_multiline);
    for (bool set_image : {false, true}) {
      if (set_image)
        button()->SetImageModel(Button::STATE_NORMAL,
                                ui::ImageModel::FromImageSkia(
                                    gfx::test::CreateImageSkia(/*size=*/16)));

      bool preferred_size_is_sometimes_narrower_than_max = false;
      bool preferred_height_shrinks_as_max_width_grows = false;

      for (const auto& text_case : text_cases) {
        for (int width_case : width_cases) {
          const gfx::Size old_preferred_size = button()->GetPreferredSize({});

          button()->SetText(ASCIIToUTF16(text_case));
          button()->SetMaxSize(gfx::Size(width_case, 30));

          const gfx::Size preferred_size = button()->GetPreferredSize({});
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
  ASSERT_TRUE(button()->GetText().empty());

  const gfx::FontList font_list = button()->label()->font_list();
  const std::u16string text(u"abcdefghijklm");
  const int text_width = gfx::GetStringWidth(text, font_list);

  ASSERT_LT(button()->GetPreferredSize({}).width(), text_width);
  button()->SetText(text);
  EXPECT_GT(button()->GetPreferredSize({}).width(), text_width);
  button()->SetSize(button()->GetPreferredSize({}));

  // When shrinking, the button should report again the size with no label
  // (while keeping the label).
  button()->ShrinkDownThenClearText();
  EXPECT_EQ(button()->GetText(), text);
  EXPECT_LT(button()->GetPreferredSize({}).width(), text_width);

  // After the layout manager resizes the button to it's desired size, it's text
  // should be empty again.
  button()->SetSize(button()->GetPreferredSize({}));
  EXPECT_TRUE(button()->GetText().empty());
}

TEST_F(LabelButtonTest, LabelShrinksDownOnManualSetBounds) {
  ASSERT_TRUE(button()->GetText().empty());
  ASSERT_GT(button()->GetPreferredSize({}).width(), 1);

  const std::u16string text(u"abcdefghijklm");

  button()->SetText(text);
  EXPECT_EQ(button()->GetText(), text);
  button()->SetSize(button()->GetPreferredSize({}));
  button()->SetBoundsRect(gfx::Rect(button()->GetPreferredSize({})));

  button()->ShrinkDownThenClearText();

  // Manually setting a smaller size should also clear text.
  button()->SetBoundsRect(gfx::Rect(1, 1));
  EXPECT_TRUE(button()->GetText().empty());
}

TEST_F(LabelButtonTest, LabelShrinksDownCanceledBySettingText) {
  ASSERT_TRUE(button()->GetText().empty());

  const gfx::FontList font_list = button()->label()->font_list();
  const std::u16string text(u"abcdefghijklm");
  const int text_width = gfx::GetStringWidth(text, font_list);

  ASSERT_LT(button()->GetPreferredSize({}).width(), text_width);
  button()->SetText(text);
  EXPECT_GT(button()->GetPreferredSize({}).width(), text_width);
  button()->SetBoundsRect(gfx::Rect(button()->GetPreferredSize({})));

  // When shrinking, the button should report again the size with no label
  // (while keeping the label).
  button()->ShrinkDownThenClearText();
  EXPECT_EQ(button()->GetText(), text);
  gfx::Size shrinking_size = button()->GetPreferredSize({});
  EXPECT_LT(shrinking_size.width(), text_width);

  // When we SetText() again, the shrinking gets canceled.
  button()->SetText(text);
  EXPECT_GT(button()->GetPreferredSize({}).width(), text_width);

  // Even if the layout manager resizes the button to it's size desired for
  // shrinking, it's text does not get cleared and it still prefers having space
  // for its label.
  button()->SetSize(shrinking_size);
  EXPECT_FALSE(button()->GetText().empty());
  EXPECT_GT(button()->GetPreferredSize({}).width(), text_width);
}

TEST_F(
    LabelButtonTest,
    LabelShrinksDownImmediatelyIfAlreadySmallerThanPreferredSizeWithoutLabel) {
  button()->SetBoundsRect(gfx::Rect(1, 1));
  button()->SetText(u"abcdefghijklm");

  // Shrinking the text down when it's already shrunk down (its size is smaller
  // than preferred without label) should clear the text immediately.
  EXPECT_FALSE(button()->GetText().empty());
  button()->ShrinkDownThenClearText();
  EXPECT_TRUE(button()->GetText().empty());
}

// Test behavior of ViewAccessibility::GetAccessibleNodeData() for buttons when
// setting a label.
TEST_F(LabelButtonTest, AccessibleState) {
  ui::AXNodeData accessible_node_data;

  button()->GetViewAccessibility().GetAccessibleNodeData(&accessible_node_data);
  EXPECT_EQ(ax::mojom::Role::kButton, accessible_node_data.role);
  EXPECT_EQ(std::u16string(), accessible_node_data.GetString16Attribute(
                                  ax::mojom::StringAttribute::kName));

  // Without a label (e.g. image-only), the accessible name should automatically
  // be set from the tooltip.
  const std::u16string tooltip_text = u"abc";
  button()->SetTooltipText(tooltip_text);
  button()->GetViewAccessibility().GetAccessibleNodeData(&accessible_node_data);
  EXPECT_EQ(tooltip_text, accessible_node_data.GetString16Attribute(
                              ax::mojom::StringAttribute::kName));
  EXPECT_EQ(std::u16string(), button()->GetText());

  // Setting a label overrides the tooltip text.
  const std::u16string label_text = u"def";
  button()->SetText(label_text);
  button()->GetViewAccessibility().GetAccessibleNodeData(&accessible_node_data);
  EXPECT_EQ(label_text, accessible_node_data.GetString16Attribute(
                            ax::mojom::StringAttribute::kName));
  EXPECT_EQ(label_text, button()->GetText());
  EXPECT_EQ(tooltip_text, button()->GetTooltipText(gfx::Point()));
}

// Test ViewAccessibility::GetAccessibleNodeData() for default buttons.
TEST_F(LabelButtonTest, AccessibleDefaultState) {
  {
    // If SetIsDefault() is not called, the ax default state should not be set.
    ui::AXNodeData ax_data;
    button()->GetViewAccessibility().GetAccessibleNodeData(&ax_data);
    EXPECT_FALSE(ax_data.HasState(ax::mojom::State::kDefault));
  }

  {
    button()->SetIsDefault(true);
    ui::AXNodeData ax_data;
    button()->GetViewAccessibility().GetAccessibleNodeData(&ax_data);
    EXPECT_TRUE(ax_data.HasState(ax::mojom::State::kDefault));
  }

  {
    button()->SetIsDefault(false);
    ui::AXNodeData ax_data;
    button()->GetViewAccessibility().GetAccessibleNodeData(&ax_data);
    EXPECT_FALSE(ax_data.HasState(ax::mojom::State::kDefault));
  }
}

TEST_F(LabelButtonTest, Image) {
  const int small_size = 50, large_size = 100;
  const gfx::ImageSkia small_image = gfx::test::CreateImageSkia(small_size);
  const gfx::ImageSkia large_image = gfx::test::CreateImageSkia(large_size);

  EXPECT_LT(button()->GetPreferredSize({}).width(), small_size);
  EXPECT_LT(button()->GetPreferredSize({}).height(), small_size);
  button()->SetImageModel(Button::STATE_NORMAL,
                          ui::ImageModel::FromImageSkia(small_image));
  EXPECT_GT(button()->GetPreferredSize({}).width(), small_size);
  EXPECT_GT(button()->GetPreferredSize({}).height(), small_size);
  EXPECT_LT(button()->GetPreferredSize({}).width(), large_size);
  EXPECT_LT(button()->GetPreferredSize({}).height(), large_size);
  button()->SetImageModel(Button::STATE_NORMAL,
                          ui::ImageModel::FromImageSkia(large_image));
  EXPECT_GT(button()->GetPreferredSize({}).width(), large_size);
  EXPECT_GT(button()->GetPreferredSize({}).height(), large_size);
  button()->SetImageModel(Button::STATE_NORMAL,
                          ui::ImageModel::FromImageSkia(small_image));
  EXPECT_GT(button()->GetPreferredSize({}).width(), small_size);
  EXPECT_GT(button()->GetPreferredSize({}).height(), small_size);
  EXPECT_LT(button()->GetPreferredSize({}).width(), large_size);
  EXPECT_LT(button()->GetPreferredSize({}).height(), large_size);

  // Clamp the size to a maximum value.
  button()->SetImageModel(Button::STATE_NORMAL,
                          ui::ImageModel::FromImageSkia(large_image));
  button()->SetMaxSize(gfx::Size(large_size, 1));
  EXPECT_EQ(button()->GetPreferredSize({}), gfx::Size(large_size, 1));

  // Clamp the size to a minimum value.
  button()->SetImageModel(Button::STATE_NORMAL,
                          ui::ImageModel::FromImageSkia(small_image));
  button()->SetMaxSize(gfx::Size());
  button()->SetMinSize(gfx::Size(large_size, large_size));
  EXPECT_EQ(button()->GetPreferredSize({}), gfx::Size(large_size, large_size));
}

TEST_F(LabelButtonTest, ImageAlignmentWithMultilineLabel) {
  const std::u16string text(
      u"Some long text that would result in multiline label");
  button()->SetText(text);

  const int max_label_width = 40;
  button()->label()->SetMultiLine(true);
  button()->label()->SetMaximumWidth(max_label_width);

  const int image_size = 16;
  const gfx::ImageSkia image = gfx::test::CreateImageSkia(image_size);
  button()->SetImageModel(Button::STATE_NORMAL,
                          ui::ImageModel::FromImageSkia(image));

  button()->SetBoundsRect(gfx::Rect(button()->GetPreferredSize({})));
  views::test::RunScheduledLayout(button());
  int y_origin_centered = button()->image_container_view()->origin().y();

  button()->SetBoundsRect(gfx::Rect(button()->GetPreferredSize({})));
  button()->SetImageCentered(false);
  views::test::RunScheduledLayout(button());
  int y_origin_not_centered = button()->image_container_view()->origin().y();

  EXPECT_LT(y_origin_not_centered, y_origin_centered);
}

TEST_F(LabelButtonTest, LabelAndImage) {
  const gfx::FontList font_list = button()->label()->font_list();
  const std::u16string text(u"abcdefghijklm");
  const int text_width = gfx::GetStringWidth(text, font_list);

  const int image_size = 50;
  const gfx::ImageSkia image = gfx::test::CreateImageSkia(image_size);
  ASSERT_LT(font_list.GetHeight(), image_size);

  EXPECT_LT(button()->GetPreferredSize({}).width(), text_width);
  EXPECT_LT(button()->GetPreferredSize({}).width(), image_size);
  EXPECT_LT(button()->GetPreferredSize({}).height(), image_size);
  button()->SetText(text);
  EXPECT_GT(button()->GetPreferredSize({}).width(), text_width);
  EXPECT_GT(button()->GetPreferredSize({}).height(), font_list.GetHeight());
  EXPECT_LT(button()->GetPreferredSize({}).width(), text_width + image_size);
  EXPECT_LT(button()->GetPreferredSize({}).height(), image_size);
  button()->SetImageModel(Button::STATE_NORMAL,
                          ui::ImageModel::FromImageSkia(image));
  EXPECT_GT(button()->GetPreferredSize({}).width(), text_width + image_size);
  EXPECT_GT(button()->GetPreferredSize({}).height(), image_size);

  // Layout and ensure the image is left of the label except for ALIGN_RIGHT.
  // (A proper parent view or layout manager would Layout on its invalidations).
  // Also make sure CENTER alignment moves the label compared to LEFT alignment.
  gfx::Size button_size = button()->GetPreferredSize({});
  button_size.Enlarge(50, 0);
  button()->SetSize(button_size);
  views::test::RunScheduledLayout(button());
  EXPECT_LT(button()->image_container_view()->bounds().right(),
            button()->label()->bounds().x());
  int left_align_label_midpoint = button()->label()->bounds().CenterPoint().x();
  button()->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  views::test::RunScheduledLayout(button());
  EXPECT_LT(button()->image_container_view()->bounds().right(),
            button()->label()->bounds().x());
  int center_align_label_midpoint =
      button()->label()->bounds().CenterPoint().x();
  EXPECT_LT(left_align_label_midpoint, center_align_label_midpoint);
  button()->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  views::test::RunScheduledLayout(button());
  EXPECT_LT(button()->label()->bounds().right(),
            button()->image_container_view()->bounds().x());

  button()->SetText(std::u16string());
  EXPECT_LT(button()->GetPreferredSize({}).width(), text_width + image_size);
  EXPECT_GT(button()->GetPreferredSize({}).width(), image_size);
  EXPECT_GT(button()->GetPreferredSize({}).height(), image_size);
  button()->SetImageModel(Button::STATE_NORMAL, ui::ImageModel());
  EXPECT_LT(button()->GetPreferredSize({}).width(), image_size);
  EXPECT_LT(button()->GetPreferredSize({}).height(), image_size);

  // Clamp the size to a minimum value.
  button()->SetText(text);
  button()->SetImageModel(Button::STATE_NORMAL,
                          ui::ImageModel::FromImageSkia(image));
  button()->SetMinSize(
      gfx::Size((text_width + image_size) * 2, image_size * 2));
  EXPECT_EQ(button()->GetPreferredSize({}).width(),
            (text_width + image_size) * 2);
  EXPECT_EQ(button()->GetPreferredSize({}).height(), image_size * 2);

  // Clamp the size to a maximum value.
  button()->SetMinSize(gfx::Size());
  button()->SetMaxSize(gfx::Size(1, 1));
  EXPECT_EQ(button()->GetPreferredSize({}), gfx::Size(1, 1));
}

TEST_F(LabelButtonTest, LabelWrapAndImageAlignment) {
  LayoutProvider* provider = LayoutProvider::Get();
  const gfx::FontList font_list = button()->label()->font_list();
  const std::u16string text(u"abcdefghijklm abcdefghijklm");
  const int text_wrap_width = gfx::GetStringWidth(text, font_list) / 2;
  const int image_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_LABEL_HORIZONTAL);

  button()->SetText(text);
  button()->label()->SetMultiLine(true);

  const int image_size = font_list.GetHeight();
  const gfx::ImageSkia image = gfx::test::CreateImageSkia(image_size);
  ASSERT_EQ(font_list.GetHeight(), image.width());
  gfx::Insets button_insets = button()->GetInsets();
  const int unclamped_size_without_label =
      image.width() + image_spacing + button_insets.width();

  button()->SetImageModel(Button::STATE_NORMAL,
                          ui::ImageModel::FromImageSkia(image));
  button()->SetImageCentered(false);
  button()->SetMaxSize(
      gfx::Size(unclamped_size_without_label + text_wrap_width, 0));

  gfx::Size preferred_size = button()->GetPreferredSize({});
  preferred_size.set_height(
      button()->GetHeightForWidth(preferred_size.width()));
  button()->SetSize(preferred_size);
  views::test::RunScheduledLayout(button());

  gfx::Size label_width = button()->label()->GetPreferredSize(
      views::SizeBounds(text_wrap_width, {}));
  EXPECT_EQ(preferred_size.width(),
            unclamped_size_without_label + label_width.width());
  EXPECT_EQ(preferred_size.height(),
            font_list.GetHeight() * 2 + button_insets.height());

  // The image should be centered on the first line of the multi-line label
  EXPECT_EQ(
      button()->image_container_view()->y(),
      (font_list.GetHeight() - button()->image_container_view()->height()) / 2 +
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
  const int font_height = button()->label()->font_list().GetHeight();
  // Parts of this test (accounting for label height) doesn't make sense if the
  // font is smaller than the tiny test image and insets.
  ASSERT_GT(font_height, button()->GetInsets().height() + kTinyImageSize);
  // Parts of this test (accounting for image insets) doesn't make sense if the
  // font is larger than the large test image.
  ASSERT_LT(font_height, kLargeImageSize);
  button()->SetText(text);

  for (int image_size : {kTinyImageSize, kLargeImageSize}) {
    SCOPED_TRACE(testing::Message() << "Image Size: " << image_size);
    // Set image and reset monotonic min size for every test iteration.
    const gfx::ImageSkia image = gfx::test::CreateImageSkia(image_size);
    button()->SetImageModel(Button::STATE_NORMAL,
                            ui::ImageModel::FromImageSkia(image));

    const gfx::Size preferred_button_size = button()->GetPreferredSize({});

    // The preferred button height should be the larger of image / label
    // heights + inset height.
    EXPECT_EQ(
        std::max(image_size, font_height) + button()->GetInsets().height(),
        preferred_button_size.height());

    // Make sure this preferred height is consistent with GetHeightForWidth().
    EXPECT_EQ(preferred_button_size.height(),
              button()->GetHeightForWidth(preferred_button_size.width()));
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
  button()->SetText(text);
  EXPECT_EQ(default_delta, button()->label()->font_list().GetFontSize() -
                               gfx::FontList().GetFontSize());

  TestLabelButton* alternate_button =
      new TestLabelButton(text, kAlternateContext);
  button()->parent()->AddChildView(alternate_button);
  EXPECT_EQ(alternate_delta,
            alternate_button->label()->font_list().GetFontSize() -
                gfx::FontList().GetFontSize());

  // The button size increases when the font size is increased.
  EXPECT_LT(button()->GetPreferredSize({}).width(),
            alternate_button->GetPreferredSize({}).width());
  EXPECT_LT(button()->GetPreferredSize({}).height(),
            alternate_button->GetPreferredSize({}).height());
}

TEST_F(LabelButtonTest, ChangeTextSize) {
  const std::u16string text(u"abc");
  const std::u16string longer_text(u"abcdefghijklm");
  button()->SetText(text);
  button()->SizeToPreferredSize();
  gfx::Rect bounds(button()->bounds());
  const int original_width = button()->GetPreferredSize({}).width();
  EXPECT_EQ(original_width, bounds.width());

  // Reserve more space in the button.
  bounds.set_width(bounds.width() * 10);
  button()->SetBoundsRect(bounds);

  // Label view in the button is sized to short text.
  const int original_label_width = button()->label()->bounds().width();

  // The button preferred size and the label size increase when the text size
  // is increased.
  button()->SetText(longer_text);
  EXPECT_TRUE(ViewTestApi(button()).needs_layout());
  views::test::RunScheduledLayout(button());
  EXPECT_GT(button()->label()->bounds().width(), original_label_width * 2);
  EXPECT_GT(button()->GetPreferredSize({}).width(), original_width * 2);

  // The button and the label view return to its original size when the original
  // text is restored.
  button()->SetText(text);
  EXPECT_TRUE(ViewTestApi(button()).needs_layout());
  views::test::RunScheduledLayout(button());
  EXPECT_EQ(original_label_width, button()->label()->bounds().width());
  EXPECT_EQ(original_width, button()->GetPreferredSize({}).width());
}

TEST_F(LabelButtonTest, ChangeLabelImageSpacing) {
  button()->SetText(u"abc");
  button()->SetImageModel(
      Button::STATE_NORMAL,
      ui::ImageModel::FromImageSkia(gfx::test::CreateImageSkia(/*size=*/50)));

  const int kOriginalSpacing = 5;
  button()->SetImageLabelSpacing(kOriginalSpacing);
  const int original_width = button()->GetPreferredSize({}).width();

  // Increasing the spacing between the text and label should increase the size.
  button()->SetImageLabelSpacing(2 * kOriginalSpacing);
  EXPECT_GT(button()->GetPreferredSize({}).width(), original_width);

  // The button shrinks if the original spacing is restored.
  button()->SetImageLabelSpacing(kOriginalSpacing);
  EXPECT_EQ(original_width, button()->GetPreferredSize({}).width());
}

// Ensure the label gets the correct style when pressed or becoming default.
TEST_F(LabelButtonTest, HighlightedButtonStyle) {
  // The ColorProvider might not provide SK_ColorBLACK, but it should be the
  // same for normal and pressed states.
  EXPECT_EQ(themed_normal_text_color_, button()->label()->GetEnabledColor());
  button()->SetState(Button::STATE_PRESSED);
  EXPECT_EQ(themed_normal_text_color_, button()->label()->GetEnabledColor());
}

// Ensure the label resets the enabled color after LabelButton::OnThemeChanged()
// is invoked.
TEST_F(LabelButtonTest, OnThemeChanged) {
  ASSERT_NE(button()->GetNativeTheme()->GetPlatformHighContrastColorScheme(),
            ui::NativeTheme::PlatformHighContrastColorScheme::kDark);
  ASSERT_NE(button()->label()->GetBackgroundColor(), SK_ColorBLACK);
  EXPECT_EQ(themed_normal_text_color_, button()->label()->GetEnabledColor());

  button()->label()->SetBackgroundColor(SK_ColorBLACK);
  button()->label()->SetAutoColorReadabilityEnabled(true);
  EXPECT_NE(themed_normal_text_color_, button()->label()->GetEnabledColor());

  button()->OnThemeChanged();
  EXPECT_EQ(themed_normal_text_color_, button()->label()->GetEnabledColor());
}

TEST_F(LabelButtonTest, SetEnabledTextColorsResetsToThemeColors) {
  constexpr SkColor kReplacementColor = SK_ColorCYAN;

  // This test doesn't make sense if the used colors are equal.
  EXPECT_NE(themed_normal_text_color_, kReplacementColor);

  // Initially the test should have the normal colors.
  EXPECT_EQ(themed_normal_text_color_, button()->label()->GetEnabledColor());

  // Setting the enabled text colors should replace the label's enabled color.
  button()->SetEnabledTextColors(kReplacementColor);
  EXPECT_EQ(kReplacementColor, button()->label()->GetEnabledColor());

  // Toggle dark mode. This should not replace the enabled text color as it's
  // been manually overridden above.
  UseDarkColors();
  EXPECT_EQ(kReplacementColor, button()->label()->GetEnabledColor());

  // Removing the enabled text color restore colors from the new theme, not
  // the original colors used before the theme changed.
  button()->SetEnabledTextColors(std::nullopt);
  EXPECT_NE(themed_normal_text_color_, button()->label()->GetEnabledColor());
}

TEST_F(LabelButtonTest, SetEnabledTextColorIds) {
  ASSERT_NE(ui::kColorLabelForeground, ui::kColorAccent);

  // Initially the test should have the normal colors.
  EXPECT_EQ(button()->label()->GetEnabledColorId(), ui::kColorLabelForeground);

  // Setting the enabled text colors should replace the label's enabled color.
  button()->SetEnabledTextColorIds(ui::kColorAccent);
  EXPECT_EQ(button()->label()->GetEnabledColorId(), ui::kColorAccent);

  // Toggle dark mode. This should not replace the enabled text color as it's
  // been manually overridden above.
  UseDarkColors();
  EXPECT_EQ(button()->label()->GetEnabledColorId(), ui::kColorAccent);
  EXPECT_EQ(button()->label()->GetEnabledColor(),
            button()->GetColorProvider()->GetColor(ui::kColorAccent));
}

TEST_F(LabelButtonTest, ImageOrLabelGetClipped) {
  const std::u16string text(u"abc");
  button()->SetText(text);

  const gfx::FontList font_list = button()->label()->font_list();
  const int image_size = font_list.GetHeight();
  button()->SetImageModel(
      Button::STATE_NORMAL,
      ui::ImageModel::FromImageSkia(gfx::test::CreateImageSkia(image_size)));

  button()->SetBoundsRect(gfx::Rect(button()->GetPreferredSize({})));
  // The border size + the content height is more than button's preferred size.
  button()->SetBorder(CreateEmptyBorder(
      gfx::Insets::TLBR(image_size / 2, 0, image_size / 2, 0)));
  views::test::RunScheduledLayout(button());

  // Ensure that content (image and label) doesn't get clipped by the border.
  EXPECT_GE(button()->image_container_view()->height(), image_size);
  EXPECT_GE(button()->label()->height(), image_size);
}

TEST_F(LabelButtonTest, UpdateImageAfterSettingImageModel) {
  auto is_showing_image = [&](const gfx::ImageSkia& image) {
    const auto* image_view =
        AsViewClass<ImageView>(button()->image_container_view());
    CHECK(image_view);
    return image_view->GetImage().BackedBySameObjectAs(image);
  };

  auto normal_image = gfx::test::CreateImageSkia(/*size=*/16);
  button()->SetImageModel(Button::STATE_NORMAL,
                          ui::ImageModel::FromImageSkia(normal_image));
  EXPECT_TRUE(is_showing_image(normal_image));

  // When the button has no specific disabled image, changing the normal image
  // while the button is disabled should update the currently-visible image.
  normal_image = gfx::test::CreateImageSkia(/*size=*/16);
  button()->SetState(Button::STATE_DISABLED);
  button()->SetImageModel(Button::STATE_NORMAL,
                          ui::ImageModel::FromImageSkia(normal_image));
  EXPECT_TRUE(is_showing_image(normal_image));

  // Any specific disabled image should take precedence over the normal image.
  auto disabled_image = gfx::test::CreateImageSkia(/*size=*/16);
  button()->SetImageModel(Button::STATE_DISABLED,
                          ui::ImageModel::FromImageSkia(disabled_image));
  EXPECT_TRUE(is_showing_image(disabled_image));

  // An empty disabled image should take precedence over the normal image.
  auto disabled_empty_image = gfx::test::CreateImageSkia(0);
  button()->SetImageModel(Button::STATE_DISABLED,
                          ui::ImageModel::FromImageSkia(disabled_empty_image));
  EXPECT_TRUE(is_showing_image(disabled_empty_image));

  // Removing the disabled image should result in falling back to the normal
  // image again.
  button()->SetImageModel(Button::STATE_DISABLED, std::nullopt);
  EXPECT_TRUE(is_showing_image(normal_image));
}

TEST_F(LabelButtonTest, AccessibiltyDefaultState) {
  ui::AXNodeData node_data = ui::AXNodeData();
  /// Initially, kDefault should be set to false.
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kDefault));

  button()->SetIsDefault(true);
  button()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kDefault));

  node_data = ui::AXNodeData();  // Reset the node data.
  button()->SetIsDefault(false);
  button()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kDefault));
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
    Widget::InitParams params = CreateParams(
        Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(0, 0, 20, 20);
    widget_->Init(std::move(params));
    widget_->Show();

    auto* button = widget_->SetContentsView(std::make_unique<LabelButton>(
        Button::PressedCallback(), std::u16string()));

    auto* test_ink_drop = new test::TestInkDrop();
    test::InkDropHostTestApi(InkDrop::Get(button))
        .SetInkDrop(base::WrapUnique(test_ink_drop));
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  LabelButton* button() {
    return static_cast<LabelButton*>(widget_->GetContentsView());
  }

  test::TestInkDrop* test_ink_drop() {
    return static_cast<test::TestInkDrop*>(
        InkDrop::Get(button())->GetInkDrop());
  }

  // Required to host the test target.
  std::unique_ptr<Widget> widget_;
};

TEST_F(InkDropLabelButtonTest, HoverStateAfterMouseEnterAndExitEvents) {
  ui::test::EventGenerator event_generator(GetRootWindow(widget_.get()));
  const gfx::Point out_of_bounds_point(
      button()->GetBoundsInScreen().bottom_right() + gfx::Vector2d(1, 1));
  const gfx::Point in_bounds_point(button()->GetBoundsInScreen().CenterPoint());

  event_generator.MoveMouseTo(out_of_bounds_point);
  EXPECT_FALSE(test_ink_drop()->is_hovered());

  event_generator.MoveMouseTo(in_bounds_point);
  EXPECT_TRUE(test_ink_drop()->is_hovered());

  event_generator.MoveMouseTo(out_of_bounds_point);
  EXPECT_FALSE(test_ink_drop()->is_hovered());
}

// Verifies the target event handler View is the |LabelButton| and not any of
// the child Views.
TEST_F(InkDropLabelButtonTest, TargetEventHandler) {
  View* target_view = widget_->GetRootView()->GetEventHandlerForPoint(
      button()->bounds().CenterPoint());
  EXPECT_EQ(button(), target_view);
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

    MakeButtonAsContent(test_widget_)->SetID(1);

    style_of_inactive_widget_ =
        PlatformStyle::kInactiveWidgetControlsAppearDisabled
            ? Button::STATE_DISABLED
            : Button::STATE_NORMAL;
  }

  void TearDown() override {
    test_widget_.ExtractAsDangling()->CloseNow();
    dummy_widget_.ExtractAsDangling()->CloseNow();
    WidgetTest::TearDown();
  }

 protected:
  std::unique_ptr<Widget> CreateActivatableChildWidget(Widget* parent) {
    auto child = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(
        Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
    params.parent = parent->GetNativeView();
    params.activatable = Widget::InitParams::Activatable::kYes;
    child->Init(std::move(params));
    child->SetContentsView(std::make_unique<View>());
    return child;
  }

  TestLabelButton* MakeButtonAsContent(Widget* widget) {
    return widget->GetContentsView()->AddChildView(
        std::make_unique<TestLabelButton>());
  }

  TestLabelButton* button() {
    return static_cast<TestLabelButton*>(
        test_widget_->GetContentsView()->GetViewByID(1));
  }

  raw_ptr<Widget> test_widget_ = nullptr;
  raw_ptr<Widget> dummy_widget_ = nullptr;
  Button::ButtonState style_of_inactive_widget_;
};

TEST_F(LabelButtonVisualStateTest, IndependentWidget) {
  test_widget_->ShowInactive();
  EXPECT_EQ(button()->GetVisualState(), style_of_inactive_widget_);

  test_widget_->Activate();
  EXPECT_EQ(button()->GetVisualState(), Button::STATE_NORMAL);

  auto paint_as_active_lock = test_widget_->LockPaintAsActive();
  dummy_widget_->Show();
  EXPECT_EQ(button()->GetVisualState(), Button::STATE_NORMAL);
}

TEST_F(LabelButtonVisualStateTest, ChildWidget) {
  std::unique_ptr<Widget> child_widget =
      CreateActivatableChildWidget(test_widget_);
  TestLabelButton* child_button = MakeButtonAsContent(child_widget.get());

  test_widget_->Show();
  EXPECT_EQ(button()->GetVisualState(), Button::STATE_NORMAL);
  EXPECT_EQ(child_button->GetVisualState(), Button::STATE_NORMAL);

  dummy_widget_->Show();
  EXPECT_EQ(button()->GetVisualState(), style_of_inactive_widget_);
  EXPECT_EQ(child_button->GetVisualState(), style_of_inactive_widget_);

  child_widget->Show();
#if BUILDFLAG(IS_MAC)
  // Child widget is in a key window and it will lock its parent.
  // See crrev.com/c/2048144.
  EXPECT_EQ(button()->GetVisualState(), Button::STATE_NORMAL);
#else
  EXPECT_EQ(button()->GetVisualState(), style_of_inactive_widget_);
#endif
  EXPECT_EQ(child_button->GetVisualState(), Button::STATE_NORMAL);
}

using LabelButtonActionViewInterfaceTest = ViewsTestBase;

TEST_F(LabelButtonActionViewInterfaceTest, TestActionChanged) {
  auto label_button = std::make_unique<LabelButton>();
  const std::u16string test_string = u"test_string";
  std::unique_ptr<actions::ActionItem> action_item =
      actions::ActionItem::Builder()
          .SetText(test_string)
          .SetActionId(0)
          .SetEnabled(false)
          .Build();
  label_button->GetActionViewInterface()->ActionItemChangedImpl(
      action_item.get());
  // Test some properties to ensure that the right ActionViewInterface is linked
  // to the view.
  EXPECT_EQ(test_string, label_button->GetText());
  EXPECT_FALSE(label_button->GetEnabled());
}

}  // namespace views
