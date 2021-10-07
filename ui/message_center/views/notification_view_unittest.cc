// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view.h"

#include <memory>

#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_observer.h"
#include "ui/views/animation/test/ink_drop_impl_test_api.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget_utils.h"

namespace message_center {

namespace {

// Used to fill bitmaps returned by CreateBitmap().
static const SkColor kBitmapColor = SK_ColorGREEN;

constexpr char kDefaultNotificationId[] = "notification id";

// TODO(pkasting): These hardcoded colors are fragile and should be obtained
// dynamically.
constexpr SkColor kNotificationBackgroundColor = SK_ColorWHITE;
constexpr SkColor kActionButtonBackgroundColor =
    SkColorSetRGB(0xF2, 0xF2, 0xF2);
constexpr SkColor kDarkCustomAccentColor = SkColorSetRGB(0x0D, 0x65, 0x2D);
constexpr SkColor kBrightCustomAccentColor = SkColorSetRGB(0x34, 0xA8, 0x53);

constexpr char kWebAppUrl[] = "http://example.com";

SkBitmap CreateSolidColorBitmap(int width, int height, SkColor solid_color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(solid_color);
  return bitmap;
}

std::vector<ButtonInfo> CreateButtons(int number) {
  ButtonInfo info(u"Test button.");
  return std::vector<ButtonInfo>(number, info);
}

SkColor DeriveMinContrastColor(SkColor foreground, SkColor background) {
  SkColor contrast_color =
      color_utils::BlendForMinContrast(foreground, background).color;
  float contrast_ratio =
      color_utils::GetContrastRatio(background, contrast_color);
  EXPECT_GE(contrast_ratio, color_utils::kMinimumReadableContrastRatio);
  return contrast_color;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns the same value as AshColorProvider::Get()->
// GetContentLayerColor(ContentLayerType::kIconColorPrimary).
SkColor GetAshIconColorPrimary(bool is_dark_mode) {
  return is_dark_mode ? SkColorSetRGB(0xE8, 0xEA, 0xED)
                      : SkColorSetRGB(0x5F, 0x63, 0x68);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class NotificationTestDelegate : public NotificationDelegate {
 public:
  NotificationTestDelegate() = default;
  NotificationTestDelegate(const NotificationTestDelegate&) = delete;
  NotificationTestDelegate& operator=(const NotificationTestDelegate&) = delete;

  void DisableNotification() override { disable_notification_called_ = true; }

  bool disable_notification_called() { return disable_notification_called_; }

 private:
  ~NotificationTestDelegate() override = default;

  bool disable_notification_called_ = false;
};

class DummyEvent : public ui::Event {
 public:
  DummyEvent() : Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
  ~DummyEvent() override = default;
};

}  // namespace

class NotificationViewTest : public views::ViewObserver,
                             public views::ViewsTestBase {
 public:
  NotificationViewTest() = default;
  NotificationViewTest(const NotificationViewTest&) = delete;
  NotificationViewTest& operator=(const NotificationViewTest&) = delete;
  ~NotificationViewTest() override = default;

  // views::ViewsTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    MessageCenter::Initialize();
    delegate_ = new NotificationTestDelegate();
    std::unique_ptr<Notification> notification = CreateSimpleNotification();
    UpdateNotificationViews(*notification);
  }

  void TearDown() override {
    if (notification_view_) {
      static_cast<views::View*>(notification_view_)->RemoveObserver(this);
      notification_view_->GetWidget()->Close();
      notification_view_ = nullptr;
    }
    MessageCenter::Shutdown();
    views::ViewsTestBase::TearDown();
  }

  std::unique_ptr<Notification> CreateSimpleNotification() const {
    RichNotificationData data;
    data.settings_button_handler = SettingsButtonHandler::INLINE;

    std::unique_ptr<Notification> notification = std::make_unique<Notification>(
        NOTIFICATION_TYPE_BASE_FORMAT, std::string(kDefaultNotificationId),
        u"title", u"message", CreateTestImage(80, 80), u"display source",
        GURL(), NotifierId(NotifierType::APPLICATION, "extension_id"), data,
        delegate_);
    notification->set_small_image(CreateTestImage(16, 16));
    notification->set_image(CreateTestImage(320, 240));

    return notification;
  }

  void UpdateNotificationViews(const Notification& notification) {
    MessageCenter::Get()->AddNotification(
        std::make_unique<Notification>(notification));

    if (!notification_view_) {
      auto notification_view = std::make_unique<NotificationView>(notification);
      static_cast<views::View*>(notification_view.get())->AddObserver(this);

      views::Widget::InitParams init_params(
          CreateParams(views::Widget::InitParams::TYPE_POPUP));
      // The native widget owns |widget| and |widget| owns |notification_view_|.
      auto* widget = new views::Widget();
      widget->Init(std::move(init_params));
      notification_view_ =
          widget->SetContentsView(std::move(notification_view));
      widget->SetSize(notification_view_->GetPreferredSize());
      widget->Show();
      widget->widget_delegate()->SetCanActivate(true);
      widget->Activate();
    } else {
      notification_view_->UpdateWithNotification(notification);
    }
  }

  const gfx::Image CreateTestImage(int width, int height) const {
    return gfx::Image::CreateFrom1xBitmap(CreateBitmap(width, height));
  }

  // Paints |view| and returns the size that the original image (which must have
  // been created by CreateBitmap()) was scaled to.
  gfx::Size GetImagePaintSize(ProportionalImageView* view) {
    CHECK(view);
    if (view->bounds().IsEmpty())
      return gfx::Size();

    gfx::Size canvas_size = view->bounds().size();
    gfx::Canvas canvas(canvas_size, 1.0 /* image_scale */,
                       true /* is_opaque */);
    static_assert(kBitmapColor != SK_ColorBLACK,
                  "The bitmap color must match the background color");
    canvas.DrawColor(SK_ColorBLACK);
    view->OnPaint(&canvas);

    SkBitmap bitmap = canvas.GetBitmap();
    // Incrementally inset each edge at its midpoint to find the bounds of the
    // rect containing the image's color. This assumes that the image is
    // centered in the canvas.
    const int kHalfWidth = canvas_size.width() / 2;
    const int kHalfHeight = canvas_size.height() / 2;
    gfx::Rect rect(canvas_size);
    while (rect.width() > 0 &&
           bitmap.getColor(rect.x(), kHalfHeight) != kBitmapColor)
      rect.Inset(1, 0, 0, 0);
    while (rect.height() > 0 &&
           bitmap.getColor(kHalfWidth, rect.y()) != kBitmapColor)
      rect.Inset(0, 1, 0, 0);
    while (rect.width() > 0 &&
           bitmap.getColor(rect.right() - 1, kHalfHeight) != kBitmapColor)
      rect.Inset(0, 0, 1, 0);
    while (rect.height() > 0 &&
           bitmap.getColor(kHalfWidth, rect.bottom() - 1) != kBitmapColor)
      rect.Inset(0, 0, 0, 1);

    return rect.size();
  }

  // Toggle inline settings with a dummy event.
  void ToggleInlineSettings() {
    notification_view_->ToggleInlineSettings(DummyEvent());
  }

 protected:
  NotificationView* notification_view() { return notification_view_; }
  NotificationHeaderView* header_row() {
    return notification_view_->header_row();
  }
  views::View* left_content() { return notification_view_->left_content(); }
  views::Label* title_view() { return notification_view_->title_view_; }
  views::Label* message_view() { return notification_view_->message_view(); }

  scoped_refptr<NotificationTestDelegate> delegate_;

 private:
  const SkBitmap CreateBitmap(int width, int height) const {
    return CreateSolidColorBitmap(width, height, kBitmapColor);
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override {
    EXPECT_EQ(observed_view, notification_view());
    notification_view_->GetWidget()->SetSize(
        notification_view()->GetPreferredSize());
  }

  NotificationView* notification_view_ = nullptr;
};

TEST_F(NotificationViewTest, UpdateViewsOrderingTest) {
  EXPECT_NE(nullptr, title_view());
  EXPECT_NE(nullptr, message_view());
  EXPECT_EQ(0, left_content()->GetIndexOf(title_view()));
  EXPECT_EQ(1, left_content()->GetIndexOf(message_view()));

  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_title(std::u16string());

  UpdateNotificationViews(*notification);

  EXPECT_EQ(nullptr, title_view());
  EXPECT_NE(nullptr, message_view());
  EXPECT_EQ(0, left_content()->GetIndexOf(message_view()));

  notification->set_title(u"title");

  UpdateNotificationViews(*notification);

  EXPECT_NE(nullptr, title_view());
  EXPECT_NE(nullptr, message_view());
  EXPECT_EQ(0, left_content()->GetIndexOf(title_view()));
  EXPECT_EQ(1, left_content()->GetIndexOf(message_view()));
}

TEST_F(NotificationViewTest, CreateOrUpdateTitle) {
  EXPECT_NE(nullptr, title_view());

  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_title(std::u16string());

  UpdateNotificationViews(*notification);

  EXPECT_EQ(nullptr, title_view());

  const std::u16string& expected_text = u"title";
  notification->set_title(expected_text);

  UpdateNotificationViews(*notification);

  EXPECT_EQ(expected_text, title_view()->GetText());
}

TEST_F(NotificationViewTest, TestIconSizing) {
  // TODO(tetsui): Remove duplicated integer literal in CreateOrUpdateIconView.
  const int kIconSize = 36;
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NOTIFICATION_TYPE_SIMPLE);
  ProportionalImageView* view = notification_view()->icon_view_;

  // Icons smaller than the maximum size should remain unscaled.
  notification->set_icon(CreateTestImage(kIconSize / 2, kIconSize / 4));
  UpdateNotificationViews(*notification);
  EXPECT_EQ(gfx::Size(kIconSize / 2, kIconSize / 4).ToString(),
            GetImagePaintSize(view).ToString());

  // Icons of exactly the intended icon size should remain unscaled.
  notification->set_icon(CreateTestImage(kIconSize, kIconSize));
  UpdateNotificationViews(*notification);
  EXPECT_EQ(gfx::Size(kIconSize, kIconSize).ToString(),
            GetImagePaintSize(view).ToString());

  // Icons over the maximum size should be scaled down, maintaining proportions.
  notification->set_icon(CreateTestImage(2 * kIconSize, 2 * kIconSize));
  UpdateNotificationViews(*notification);
  EXPECT_EQ(gfx::Size(kIconSize, kIconSize).ToString(),
            GetImagePaintSize(view).ToString());

  notification->set_icon(CreateTestImage(4 * kIconSize, 2 * kIconSize));
  UpdateNotificationViews(*notification);
  EXPECT_EQ(gfx::Size(kIconSize, kIconSize / 2).ToString(),
            GetImagePaintSize(view).ToString());
}

TEST_F(NotificationViewTest, LeftContentResizeForIcon) {
  const int kIconSize = 30;

  // Create a notification without an icon.
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_icon(gfx::Image());
  notification->set_image(gfx::Image());
  UpdateNotificationViews(*notification);

  // Capture the width of the left content without an icon.
  const int left_content_width = notification_view()->left_content_->width();

  // Update the notification, adding an icon.
  notification->set_icon(CreateTestImage(kIconSize, kIconSize));
  UpdateNotificationViews(*notification);

  // Left content should have less space now to show the icon.
  EXPECT_LT(notification_view()->left_content_->width(), left_content_width);
}

TEST_F(NotificationViewTest, InlineSettingsNotBlock) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NOTIFICATION_TYPE_SIMPLE);
  UpdateNotificationViews(*notification);

  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));
  gfx::Point settings_cursor_location =
      notification_view()
          ->control_buttons_view_->settings_button()
          ->GetBoundsInScreen()
          .CenterPoint();
  generator.MoveMouseTo(settings_cursor_location);
  generator.ClickLeftButton();

  // Construct a mouse click event over the done button.
  gfx::Point done_cursor_location =
      notification_view()
          ->settings_done_button_->GetBoundsInScreen()
          .CenterPoint();
  generator.MoveMouseTo(done_cursor_location);

  generator.ClickLeftButton();

  // Just clicking Done button should not change the setting.
  EXPECT_FALSE(notification_view()->settings_row_->GetVisible());
  EXPECT_FALSE(delegate_->disable_notification_called());
}

TEST_F(NotificationViewTest, InlineSettingsBlockAll) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NOTIFICATION_TYPE_SIMPLE);
  UpdateNotificationViews(*notification);

  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));
  gfx::Point settings_cursor_location =
      notification_view()
          ->control_buttons_view_->settings_button()
          ->GetBoundsInScreen()
          .CenterPoint();
  generator.MoveMouseTo(settings_cursor_location);
  generator.ClickLeftButton();

  gfx::Point block_cursor_location =
      notification_view()->block_all_button_->GetBoundsInScreen().CenterPoint();
  gfx::Point done_cursor_location =
      notification_view()
          ->settings_done_button_->GetBoundsInScreen()
          .CenterPoint();

  // Construct a mouse click event inside the block all button.
  generator.MoveMouseTo(block_cursor_location);
  generator.ClickLeftButton();
  generator.MoveMouseTo(done_cursor_location);
  generator.ClickLeftButton();
  MessageCenter::Get()->DisableNotification(notification->id());

  EXPECT_FALSE(notification_view()->settings_row_->GetVisible());
  EXPECT_TRUE(delegate_->disable_notification_called());
}

TEST_F(NotificationViewTest, TestAccentColor) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_buttons(CreateButtons(2));

  // The code below is not prepared to deal with dark mode.
  notification_view()->GetWidget()->GetNativeTheme()->set_use_dark_colors(
      false);
  UpdateNotificationViews(*notification);

  notification_view()->GetWidget()->Show();

  // Action buttons are hidden by collapsed state.
  if (!notification_view()->expanded_)
    notification_view()->ToggleExpanded();
  EXPECT_TRUE(notification_view()->actions_row_->GetVisible());

  const auto* color_provider = notification_view()->GetColorProvider();
  auto app_icon_color_matches = [&](SkColor color) {
    SkBitmap expected =
        notification
            ->GenerateMaskedSmallIcon(
                kSmallImageSizeMD, color,
                color_provider->GetColor(ui::kColorNotificationIconBackground),
                color_provider->GetColor(ui::kColorNotificationIconForeground))
            .AsBitmap();
    SkBitmap actual = *notification_view()
                           ->header_row_->app_icon_view_for_testing()
                           ->GetImage()
                           .bitmap();
    return gfx::test::AreBitmapsEqual(expected, actual);
  };

  // By default, header does not have accent color (default grey), and
  // buttons have default accent color.
  const SkColor kActionButtonTextColor =
      DeriveMinContrastColor(gfx::kGoogleBlue600, kActionButtonBackgroundColor);
  EXPECT_FALSE(
      notification_view()->header_row_->color_for_testing().has_value());
  EXPECT_EQ(kActionButtonTextColor,
            notification_view()->action_buttons_[0]->GetCurrentTextColor());
  EXPECT_EQ(kActionButtonTextColor,
            notification_view()->action_buttons_[1]->GetCurrentTextColor());
  EXPECT_TRUE(
      app_icon_color_matches(notification_view()->GetColorProvider()->GetColor(
          ui::kColorNotificationHeaderForeground)));

  // If custom accent color is set, the header and the buttons should have
  // the same accent color.
  notification->set_accent_color(kDarkCustomAccentColor);
  UpdateNotificationViews(*notification);
  auto accent_color = notification_view()->header_row_->color_for_testing();
  ASSERT_TRUE(accent_color.has_value());
  EXPECT_EQ(kDarkCustomAccentColor, accent_color.value());
  EXPECT_EQ(kDarkCustomAccentColor,
            notification_view()->action_buttons_[0]->GetCurrentTextColor());
  EXPECT_EQ(kDarkCustomAccentColor,
            notification_view()->action_buttons_[1]->GetCurrentTextColor());
  EXPECT_TRUE(app_icon_color_matches(kDarkCustomAccentColor));

  // If the custom accent color is too bright, we expect it to be darkened so
  // text and icons are still readable.
  SkColor expected_color_title = DeriveMinContrastColor(
      kBrightCustomAccentColor, kNotificationBackgroundColor);
  // Action buttons have a darker background.
  SkColor expected_color_actions = DeriveMinContrastColor(
      kBrightCustomAccentColor, kActionButtonBackgroundColor);

  notification->set_accent_color(kBrightCustomAccentColor);
  UpdateNotificationViews(*notification);
  accent_color = notification_view()->header_row_->color_for_testing();
  ASSERT_TRUE(accent_color.has_value());
  EXPECT_EQ(kBrightCustomAccentColor, accent_color.value());
  EXPECT_EQ(expected_color_actions,
            notification_view()->action_buttons_[0]->GetCurrentTextColor());
  EXPECT_EQ(expected_color_actions,
            notification_view()->action_buttons_[1]->GetCurrentTextColor());
  EXPECT_TRUE(app_icon_color_matches(expected_color_title));
}

TEST_F(NotificationViewTest, InkDropClipRect) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NotificationType::NOTIFICATION_TYPE_IMAGE);
  UpdateNotificationViews(*notification);

  // Toggle inline settings to show ink drop background.
  ToggleInlineSettings();

  auto* ink_drop = static_cast<views::InkDropImpl*>(
      views::InkDrop::Get(notification_view())->GetInkDrop());
  views::test::InkDropImplTestApi ink_drop_test_api(ink_drop);
  gfx::Rect clip_rect = ink_drop_test_api.GetRootLayer()->clip_rect();

  // Expect clip rect to honor the insets to draw the shadow.
  gfx::Insets insets = notification_view()->GetInsets();
  EXPECT_EQ(notification_view()->GetPreferredSize() - insets.size(),
            clip_rect.size());
  EXPECT_EQ(gfx::Point(insets.left(), insets.top()), clip_rect.origin());
}

TEST_F(NotificationViewTest, AppIconWebAppNotification) {
  const GURL web_app_url(kWebAppUrl);

  NotifierId notifier_id(web_app_url, /*title=*/u"web app title");

  SkBitmap small_bitmap = CreateSolidColorBitmap(16, 16, SK_ColorYELLOW);
  // Makes the center area transparent.
  small_bitmap.eraseArea(SkIRect::MakeXYWH(4, 4, 8, 8), SK_ColorTRANSPARENT);

  RichNotificationData data;
  data.settings_button_handler = SettingsButtonHandler::INLINE;

  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      NOTIFICATION_TYPE_BASE_FORMAT, std::string(kDefaultNotificationId),
      u"title", u"message", CreateTestImage(80, 80), u"display source", GURL(),
      notifier_id, data, delegate_);
  notification->set_small_image(gfx::Image::CreateFrom1xBitmap(small_bitmap));
  notification->set_image(CreateTestImage(320, 240));

  notification->set_origin_url(web_app_url);

  UpdateNotificationViews(*notification);

  const SkBitmap* app_icon_view =
      header_row()->app_icon_view_for_testing()->GetImage().bitmap();

  EXPECT_EQ(color_utils::SkColorToRgbaString(SK_ColorTRANSPARENT),
            color_utils::SkColorToRgbaString(app_icon_view->getColor(8, 8)));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(color_utils::SkColorToRgbaString(
                GetAshIconColorPrimary(/*is_dark_mode=*/false)),
            color_utils::SkColorToRgbaString(app_icon_view->getColor(0, 0)));
#else
  EXPECT_EQ(color_utils::SkColorToRgbaString(SK_ColorYELLOW),
            color_utils::SkColorToRgbaString(app_icon_view->getColor(0, 0)));
#endif
}

}  // namespace message_center
