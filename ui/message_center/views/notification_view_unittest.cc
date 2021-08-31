// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view.h"

#include <memory>

#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"

namespace message_center {

namespace {

// Used to fill bitmaps returned by CreateBitmap().
static const SkColor kBitmapColor = SK_ColorGREEN;

constexpr char kDefaultNotificationId[] = "notification id";

SkBitmap CreateSolidColorBitmap(int width, int height, SkColor solid_color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(solid_color);
  return bitmap;
}

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
        nullptr /* delegate */);
    notification->set_small_image(CreateTestImage(16, 16));
    notification->set_image(CreateTestImage(320, 240));

    return notification;
  }

  void UpdateNotificationViews(const Notification& notification) {
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

 protected:
  NotificationView* notification_view() { return notification_view_; }

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

}  // namespace message_center
