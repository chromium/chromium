// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view_md.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/views/animation/ink_drop_observer.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget_utils.h"

namespace message_center {

/* Test fixture ***************************************************************/

// Used to fill bitmaps returned by CreateBitmap().
static const SkColor kBitmapColor = SK_ColorGREEN;

constexpr char kDefaultNotificationId[] = "notification id";

class NotificationTestDelegate : public NotificationDelegate {
 public:
  NotificationTestDelegate() = default;

  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    if (!button_index && !reply && !expecting_click_)
      ADD_FAILURE() << "Click should not be invoked with a button index.";
    if (button_index && !reply && !expecting_button_click_)
      ADD_FAILURE() << "Click should not be invoked with a button index.";
    if (button_index && reply && !expecting_reply_submission_)
      ADD_FAILURE() << "Click should not be invoked with a reply.";
    if (!button_index && reply)
      FAIL();

    clicked_ = true;
    clicked_button_index_ = button_index.value_or(false);
    submitted_reply_string_ = reply.value_or(base::string16());
  }

  void Reset() {
    clicked_ = false;
    clicked_button_index_ = -1;
    submitted_reply_string_.clear();
  }

  void DisableNotification() override { disable_notification_called_ = true; }

  bool clicked() const { return clicked_; }
  int clicked_button_index() const { return clicked_button_index_; }
  const base::string16& submitted_reply_string() const {
    return submitted_reply_string_;
  }
  bool disable_notification_called() { return disable_notification_called_; }
  void set_expecting_click(bool expecting) { expecting_click_ = expecting; }
  void set_expecting_button_click(bool expecting) {
    expecting_button_click_ = expecting;
  }
  void set_expecting_reply_submission(bool expecting) {
    expecting_reply_submission_ = expecting;
  }

 private:
  ~NotificationTestDelegate() override = default;

  bool clicked_ = false;
  int clicked_button_index_ = -1;
  base::string16 submitted_reply_string_;
  bool expecting_click_ = false;
  bool expecting_button_click_ = false;
  bool expecting_reply_submission_ = false;
  bool disable_notification_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(NotificationTestDelegate);
};

class DummyEvent : public ui::Event {
 public:
  DummyEvent() : Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
  ~DummyEvent() override = default;
};

class NotificationViewMDTest
    : public views::InkDropObserver,
      public views::ViewsTestBase,
      public views::ViewObserver,
      public message_center::MessageView::SlideObserver,
      public message_center::MessageCenterObserver {
 public:
  NotificationViewMDTest();
  ~NotificationViewMDTest() override;

  // Overridden from ViewsTestBase:
  void SetUp() override;
  void TearDown() override;

  // Overridden from views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override;

  NotificationViewMD* notification_view() const {
    return notification_view_.get();
  }
  views::Widget* widget() const {
    DCHECK_EQ(widget_, notification_view()->GetWidget());
    return widget_;
  }

  // Overridden from message_center::MessageView::Observer:
  void OnSlideChanged(const std::string& notification_id) override {}

  // Overridden from message_center::MessageCenterObserver:
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;

  // Overridden from views::InkDropObserver:
  void InkDropAnimationStarted() override;
  void InkDropRippleAnimationEnded(views::InkDropState ink_drop_state) override;

  void set_delete_on_preferred_size_changed(
      bool delete_on_preferred_size_changed) {
    delete_on_preferred_size_changed_ = delete_on_preferred_size_changed;
  }

  void set_delete_on_notification_removed(bool delete_on_notification_removed) {
    delete_on_notification_removed_ = delete_on_notification_removed;
  }

  bool ink_drop_stopped() const { return ink_drop_stopped_; }

 protected:
  const gfx::Image CreateTestImage(int width, int height) const;
  const SkBitmap CreateBitmap(int width, int height) const;
  std::vector<ButtonInfo> CreateButtons(int number);
  std::unique_ptr<Notification> CreateSimpleNotification() const;

  // Paints |view| and returns the size that the original image (which must have
  // been created by CreateBitmap()) was scaled to.
  gfx::Size GetImagePaintSize(ProportionalImageView* view);

  void UpdateNotificationViews(const Notification& notification);
  float GetNotificationSlideAmount() const;
  bool IsRemovedAfterIdle(const std::string& notification_id) const;
  void DispatchGesture(const ui::GestureEventDetails& details);
  void BeginScroll();
  void EndScroll();
  void ScrollBy(int dx);
  views::View* GetCloseButton();

  bool ink_drop_stopped_ = false;
  bool delete_on_preferred_size_changed_ = false;
  bool delete_on_notification_removed_ = false;
  std::set<std::string> removed_ids_;
  scoped_refptr<NotificationTestDelegate> delegate_;
  std::unique_ptr<NotificationViewMD> notification_view_;
  views::Widget* widget_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationViewMDTest);
};

NotificationViewMDTest::NotificationViewMDTest() = default;
NotificationViewMDTest::~NotificationViewMDTest() = default;

std::unique_ptr<Notification> NotificationViewMDTest::CreateSimpleNotification()
    const {
  RichNotificationData data;
  data.settings_button_handler = SettingsButtonHandler::INLINE;

  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      NOTIFICATION_TYPE_BASE_FORMAT, std::string(kDefaultNotificationId),
      base::UTF8ToUTF16("title"), base::UTF8ToUTF16("message"),
      CreateTestImage(80, 80), base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierType::APPLICATION, "extension_id"), data, delegate_);
  notification->set_small_image(CreateTestImage(16, 16));
  notification->set_image(CreateTestImage(320, 240));

  return notification;
}

void NotificationViewMDTest::SetUp() {
  views::ViewsTestBase::SetUp();

  MessageCenter::Initialize();

  // Create a dummy notification.
  delegate_ = new NotificationTestDelegate();

  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  UpdateNotificationViews(*notification);

  MessageCenter::Get()->AddObserver(this);
}

void NotificationViewMDTest::TearDown() {
  MessageCenter::Get()->RemoveObserver(this);

  DCHECK(notification_view_ || delete_on_preferred_size_changed_ ||
         delete_on_notification_removed_);
  if (notification_view_) {
    notification_view_->SetInkDropMode(MessageView::InkDropMode::OFF);
    notification_view_->RemoveObserver(this);
    widget()->Close();
    notification_view_.reset();
  }
  MessageCenter::Shutdown();
  views::ViewsTestBase::TearDown();
}

void NotificationViewMDTest::OnViewPreferredSizeChanged(
    views::View* observed_view) {
  EXPECT_EQ(observed_view, notification_view());
  if (delete_on_preferred_size_changed_) {
    widget()->CloseNow();
    notification_view_.reset();
    return;
  }
  widget()->SetSize(notification_view()->GetPreferredSize());
}

void NotificationViewMDTest::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  if (delete_on_notification_removed_) {
    widget()->CloseNow();
    notification_view_.reset();
    return;
  }
}

const gfx::Image NotificationViewMDTest::CreateTestImage(int width,
                                                         int height) const {
  return gfx::Image::CreateFrom1xBitmap(CreateBitmap(width, height));
}

const SkBitmap NotificationViewMDTest::CreateBitmap(int width,
                                                    int height) const {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(kBitmapColor);
  return bitmap;
}

std::vector<ButtonInfo> NotificationViewMDTest::CreateButtons(int number) {
  ButtonInfo info(base::ASCIIToUTF16("Test button."));
  return std::vector<ButtonInfo>(number, info);
}

gfx::Size NotificationViewMDTest::GetImagePaintSize(
    ProportionalImageView* view) {
  CHECK(view);
  if (view->bounds().IsEmpty())
    return gfx::Size();

  gfx::Size canvas_size = view->bounds().size();
  gfx::Canvas canvas(canvas_size, 1.0 /* image_scale */, true /* is_opaque */);
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

void NotificationViewMDTest::UpdateNotificationViews(
    const Notification& notification) {
  MessageCenter::Get()->AddNotification(
      std::make_unique<Notification>(notification));

  if (!notification_view_) {
    // Then create a new NotificationView with that single notification.
    // In the actual code path, this is instantiated by
    // MessageViewFactory::Create.
    // TODO(tetsui): Confirm that NotificationViewMD options are same as one
    // created by the method.
    notification_view_ = std::make_unique<NotificationViewMD>(notification);
    notification_view_->AddObserver(this);
    notification_view_->set_owned_by_client();

    views::Widget::InitParams init_params(
        CreateParams(views::Widget::InitParams::TYPE_POPUP));
    widget_ = new views::Widget();
    widget_->Init(std::move(init_params));
    widget_->SetContentsView(notification_view_.get());
    widget_->SetSize(notification_view_->GetPreferredSize());
    widget_->Show();
    widget_->widget_delegate()->SetCanActivate(true);
    widget_->Activate();
  } else {
    notification_view_->UpdateWithNotification(notification);
  }
}

float NotificationViewMDTest::GetNotificationSlideAmount() const {
  return notification_view_->GetSlideOutLayer()
      ->transform()
      .To2dTranslation()
      .x();
}

bool NotificationViewMDTest::IsRemovedAfterIdle(
    const std::string& notification_id) const {
  base::RunLoop().RunUntilIdle();
  return !MessageCenter::Get()->FindVisibleNotificationById(notification_id);
}

void NotificationViewMDTest::DispatchGesture(
    const ui::GestureEventDetails& details) {
  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));
  ui::GestureEvent event(0, 0, 0, ui::EventTimeForNow(), details);
  generator.Dispatch(&event);
}

void NotificationViewMDTest::BeginScroll() {
  DispatchGesture(ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));
}

void NotificationViewMDTest::EndScroll() {
  DispatchGesture(ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));
}

void NotificationViewMDTest::ScrollBy(int dx) {
  DispatchGesture(ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, dx, 0));
}

views::View* NotificationViewMDTest::GetCloseButton() {
  return notification_view()->GetControlButtonsView()->close_button();
}

void NotificationViewMDTest::InkDropAnimationStarted() {}

void NotificationViewMDTest::InkDropRippleAnimationEnded(
    views::InkDropState ink_drop_state) {
  ink_drop_stopped_ = true;
}

/* Unit tests *****************************************************************/

// TODO(tetsui): Following tests are not yet ported from NotificationViewTest.
// * CreateOrUpdateTestSettingsButton
// * TestLineLimits
// * TestImageSizing
// * SettingsButtonTest
// * ViewOrderingTest
// * FormatContextMessageTest

TEST_F(NotificationViewMDTest, CreateOrUpdateTest) {
  EXPECT_NE(nullptr, notification_view()->title_view_);
  EXPECT_NE(nullptr, notification_view()->message_view_);
  EXPECT_NE(nullptr, notification_view()->icon_view_);
  EXPECT_NE(nullptr, notification_view()->image_container_view_);

  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_image(gfx::Image());
  notification->set_title(base::string16());
  notification->set_message(base::string16());
  notification->set_icon(gfx::Image());

  notification_view()->CreateOrUpdateViews(*notification);

  EXPECT_EQ(nullptr, notification_view()->title_view_);
  EXPECT_EQ(nullptr, notification_view()->message_view_);
  EXPECT_EQ(nullptr, notification_view()->image_container_view_);
  EXPECT_EQ(nullptr, notification_view()->icon_view_);
}

TEST_F(NotificationViewMDTest, UpdateViewsOrderingTest) {
  EXPECT_NE(nullptr, notification_view()->title_view_);
  EXPECT_NE(nullptr, notification_view()->message_view_);
  EXPECT_EQ(0, notification_view()->left_content_->GetIndexOf(
                   notification_view()->title_view_));
  EXPECT_EQ(1, notification_view()->left_content_->GetIndexOf(
                   notification_view()->message_view_));

  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_title(base::string16());

  notification_view()->CreateOrUpdateViews(*notification);

  EXPECT_EQ(nullptr, notification_view()->title_view_);
  EXPECT_NE(nullptr, notification_view()->message_view_);
  EXPECT_EQ(0, notification_view()->left_content_->GetIndexOf(
                   notification_view()->message_view_));

  notification->set_title(base::UTF8ToUTF16("title"));

  notification_view()->CreateOrUpdateViews(*notification);

  EXPECT_NE(nullptr, notification_view()->title_view_);
  EXPECT_NE(nullptr, notification_view()->message_view_);
  EXPECT_EQ(0, notification_view()->left_content_->GetIndexOf(
                   notification_view()->title_view_));
  EXPECT_EQ(1, notification_view()->left_content_->GetIndexOf(
                   notification_view()->message_view_));
}

TEST_F(NotificationViewMDTest, TestIconSizing) {
  // TODO(tetsui): Remove duplicated integer literal in CreateOrUpdateIconView.
  const int kNotificationIconSize = 36;

  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NOTIFICATION_TYPE_SIMPLE);
  ProportionalImageView* view = notification_view()->icon_view_;

  // Icons smaller than the maximum size should remain unscaled.
  notification->set_icon(
      CreateTestImage(kNotificationIconSize / 2, kNotificationIconSize / 4));
  UpdateNotificationViews(*notification);
  EXPECT_EQ(gfx::Size(kNotificationIconSize / 2, kNotificationIconSize / 4)
                .ToString(),
            GetImagePaintSize(view).ToString());

  // Icons of exactly the intended icon size should remain unscaled.
  notification->set_icon(
      CreateTestImage(kNotificationIconSize, kNotificationIconSize));
  UpdateNotificationViews(*notification);
  EXPECT_EQ(gfx::Size(kNotificationIconSize, kNotificationIconSize).ToString(),
            GetImagePaintSize(view).ToString());

  // Icons over the maximum size should be scaled down, maintaining proportions.
  notification->set_icon(
      CreateTestImage(2 * kNotificationIconSize, 2 * kNotificationIconSize));
  UpdateNotificationViews(*notification);
  EXPECT_EQ(gfx::Size(kNotificationIconSize, kNotificationIconSize).ToString(),
            GetImagePaintSize(view).ToString());

  notification->set_icon(
      CreateTestImage(4 * kNotificationIconSize, 2 * kNotificationIconSize));
  UpdateNotificationViews(*notification);
  EXPECT_EQ(
      gfx::Size(kNotificationIconSize, kNotificationIconSize / 2).ToString(),
      GetImagePaintSize(view).ToString());
}

TEST_F(NotificationViewMDTest, UpdateButtonsStateTest) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification_view()->CreateOrUpdateViews(*notification);
  widget()->Show();

  // When collapsed, new buttons are not shown.
  EXPECT_FALSE(notification_view()->expanded_);
  notification->set_buttons(CreateButtons(2));
  notification_view()->CreateOrUpdateViews(*notification);
  EXPECT_FALSE(notification_view()->actions_row_->GetVisible());

  // Adding buttons when expanded makes action buttons visible.
  // Reset back to zero buttons first.
  notification->set_buttons(CreateButtons(0));
  notification_view()->CreateOrUpdateViews(*notification);
  // Expand, and add buttons.
  notification_view()->ToggleExpanded();
  EXPECT_TRUE(notification_view()->expanded_);
  notification->set_buttons(CreateButtons(2));
  notification_view()->CreateOrUpdateViews(*notification);
  EXPECT_TRUE(notification_view()->actions_row_->GetVisible());

  EXPECT_EQ(views::Button::STATE_NORMAL,
            notification_view()->action_buttons_[0]->state());

  // Now construct a mouse move event 1 pixel inside the boundary of the action
  // button.
  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToWidget(notification_view()->action_buttons_[0],
                                    &cursor_location);
  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));
  generator.MoveMouseTo(cursor_location);

  EXPECT_EQ(views::Button::STATE_HOVERED,
            notification_view()->action_buttons_[0]->state());

  notification_view()->CreateOrUpdateViews(*notification);

  EXPECT_EQ(views::Button::STATE_HOVERED,
            notification_view()->action_buttons_[0]->state());

  // Now construct a mouse move event 1 pixel outside the boundary of the
  // widget.
  cursor_location = gfx::Point(-1, -1);
  views::View::ConvertPointToWidget(notification_view()->action_buttons_[0],
                                    &cursor_location);
  generator.MoveMouseTo(cursor_location);

  EXPECT_EQ(views::Button::STATE_NORMAL,
            notification_view()->action_buttons_[0]->state());
}

TEST_F(NotificationViewMDTest, UpdateButtonCountTest) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_buttons(CreateButtons(2));
  UpdateNotificationViews(*notification);
  widget()->Show();

  // Action buttons are hidden by collapsed state.
  if (!notification_view()->expanded_)
    notification_view()->ToggleExpanded();
  EXPECT_TRUE(notification_view()->actions_row_->GetVisible());

  EXPECT_EQ(views::Button::STATE_NORMAL,
            notification_view()->action_buttons_[0]->state());
  EXPECT_EQ(views::Button::STATE_NORMAL,
            notification_view()->action_buttons_[1]->state());

  // Now construct a mouse move event 1 pixel inside the boundary of the action
  // button.
  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(notification_view()->action_buttons_[0],
                                    &cursor_location);
  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));
  generator.MoveMouseTo(cursor_location);

  EXPECT_EQ(views::Button::STATE_HOVERED,
            notification_view()->action_buttons_[0]->state());
  EXPECT_EQ(views::Button::STATE_NORMAL,
            notification_view()->action_buttons_[1]->state());

  notification->set_buttons(CreateButtons(1));
  UpdateNotificationViews(*notification);

  EXPECT_EQ(views::Button::STATE_HOVERED,
            notification_view()->action_buttons_[0]->state());
  EXPECT_EQ(1u, notification_view()->action_buttons_.size());

  // Now construct a mouse move event 1 pixel outside the boundary of the
  // widget.
  cursor_location = gfx::Point(-1, -1);
  views::View::ConvertPointToScreen(notification_view()->action_buttons_[0],
                                    &cursor_location);
  generator.MoveMouseTo(cursor_location);

  EXPECT_EQ(views::Button::STATE_NORMAL,
            notification_view()->action_buttons_[0]->state());
}

TEST_F(NotificationViewMDTest, TestActionButtonClick) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  delegate_->set_expecting_button_click(true);

  notification->set_buttons(CreateButtons(2));
  UpdateNotificationViews(*notification);
  widget()->Show();

  ui::test::EventGenerator generator(GetRootWindow(widget()));

  // Action buttons are hidden by collapsed state.
  if (!notification_view()->expanded_)
    notification_view()->ToggleExpanded();
  EXPECT_TRUE(notification_view()->actions_row_->GetVisible());

  // Now construct a mouse click event 1 pixel inside the boundary of the action
  // button.
  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(notification_view()->action_buttons_[1],
                                    &cursor_location);
  generator.MoveMouseTo(cursor_location);
  generator.ClickLeftButton();

  EXPECT_EQ(1, delegate_->clicked_button_index());
}

TEST_F(NotificationViewMDTest, TestInlineReply) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  delegate_->set_expecting_reply_submission(true);

  std::vector<ButtonInfo> buttons = CreateButtons(2);
  buttons[1].placeholder = base::string16();
  notification->set_buttons(buttons);
  UpdateNotificationViews(*notification);
  widget()->Show();

  ui::test::EventGenerator generator(GetRootWindow(widget()));

  // Action buttons are hidden by collapsed state.
  if (!notification_view()->expanded_)
    notification_view()->ToggleExpanded();
  EXPECT_TRUE(notification_view()->actions_row_->GetVisible());

  // Now construct a mouse click event 1 pixel inside the boundary of the action
  // button.
  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(notification_view()->action_buttons_[1],
                                    &cursor_location);
  generator.MoveMouseTo(cursor_location);
  generator.ClickLeftButton();

  // Nothing should be submitted at this point.
  EXPECT_EQ(-1, delegate_->clicked_button_index());

  // Toggling should hide the inline textfield.
  EXPECT_TRUE(notification_view()->inline_reply_->GetVisible());
  notification_view()->ToggleExpanded();
  notification_view()->ToggleExpanded();
  EXPECT_FALSE(notification_view()->inline_reply_->GetVisible());

  // Click the button again and the inline textfield should be focused.
  generator.ClickLeftButton();
  EXPECT_TRUE(notification_view()->inline_reply_->GetVisible());
  EXPECT_TRUE(notification_view()->inline_reply_->textfield()->GetVisible());
  EXPECT_TRUE(notification_view()->inline_reply_->textfield()->HasFocus());

  // Type the text.
  ui::KeyboardCode keycodes[] = {ui::VKEY_T, ui::VKEY_E, ui::VKEY_S,
                                 ui::VKEY_T};
  for (ui::KeyboardCode keycode : keycodes) {
    generator.PressKey(keycode, ui::EF_NONE);
    generator.ReleaseKey(keycode, ui::EF_NONE);
  }

  // Submit by typing RETURN key.
  generator.PressKey(ui::VKEY_RETURN, ui::EF_NONE);
  generator.ReleaseKey(ui::VKEY_RETURN, ui::EF_NONE);
  EXPECT_EQ(1, delegate_->clicked_button_index());
  EXPECT_EQ(base::ASCIIToUTF16("test"), delegate_->submitted_reply_string());

  // Reset values.
  delegate_->Reset();

  // Now construct a mouse click event 1 pixel inside the boundary of the action
  // button.
  cursor_location = gfx::Point(1, 1);
  views::View::ConvertPointToScreen(notification_view()->action_buttons_[1],
                                    &cursor_location);
  generator.MoveMouseTo(cursor_location);
  generator.ClickLeftButton();

  // Nothing should be submitted at this point.
  EXPECT_EQ(-1, delegate_->clicked_button_index());
  EXPECT_EQ(base::string16(), delegate_->submitted_reply_string());

  // Click the button again and focus on the inline textfield.
  generator.ClickLeftButton();

  // Type the text.
  for (ui::KeyboardCode keycode : keycodes) {
    generator.PressKey(keycode, ui::EF_NONE);
    generator.ReleaseKey(keycode, ui::EF_NONE);
  }

  // Submit by clicking the reply button.
  cursor_location = gfx::Point(1, 1);
  views::View::ConvertPointToScreen(
      notification_view()->inline_reply_->button(), &cursor_location);
  generator.MoveMouseTo(cursor_location);
  generator.ClickLeftButton();
  EXPECT_EQ(1, delegate_->clicked_button_index());
  EXPECT_EQ(base::ASCIIToUTF16("test"), delegate_->submitted_reply_string());
}

TEST_F(NotificationViewMDTest, TestInlineReplyRemovedByUpdate) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();

  std::vector<ButtonInfo> buttons = CreateButtons(2);
  buttons[1].placeholder = base::string16();
  notification->set_buttons(buttons);
  UpdateNotificationViews(*notification);
  widget()->Show();

  ui::test::EventGenerator generator(GetRootWindow(widget()));

  // Action buttons are hidden by collapsed state.
  if (!notification_view()->expanded_)
    notification_view()->ToggleExpanded();
  EXPECT_TRUE(notification_view()->actions_row_->GetVisible());

  // Now construct a mouse click event 1 pixel inside the boundary of the action
  // button.
  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(notification_view()->action_buttons_[1],
                                    &cursor_location);
  generator.MoveMouseTo(cursor_location);
  generator.ClickLeftButton();

  // Nothing should be submitted at this point.
  EXPECT_EQ(-1, delegate_->clicked_button_index());

  EXPECT_TRUE(notification_view()->inline_reply_->GetVisible());
  EXPECT_FALSE(notification_view()->action_buttons_row_->GetVisible());

  buttons[1].placeholder = base::nullopt;
  notification->set_buttons(buttons);
  UpdateNotificationViews(*notification);

  EXPECT_FALSE(notification_view()->inline_reply_->GetVisible());
  EXPECT_TRUE(notification_view()->action_buttons_row_->GetVisible());

  // Now it emits click event.
  delegate_->set_expecting_button_click(true);
  generator.ClickLeftButton();
  EXPECT_EQ(1, delegate_->clicked_button_index());

  buttons.clear();
  notification->set_buttons(buttons);
  UpdateNotificationViews(*notification);

  EXPECT_FALSE(notification_view()->actions_row_->GetVisible());
}

TEST_F(NotificationViewMDTest, TestInlineReplyActivateWithKeyPress) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();

  std::vector<ButtonInfo> buttons = CreateButtons(2);
  buttons[1].placeholder = base::string16();
  notification->set_buttons(buttons);
  UpdateNotificationViews(*notification);
  widget()->Show();

  // Action buttons are hidden by collapsed state.
  if (!notification_view()->expanded_)
    notification_view()->ToggleExpanded();

  ui::test::EventGenerator generator(GetRootWindow(widget()));

  // Press and release space key to open inline reply text field.
  // Note: VKEY_RETURN should work too, but triggers a click on MacOS.
  notification_view()->action_buttons_[1]->RequestFocus();
  generator.PressKey(ui::VKEY_SPACE, ui::EF_NONE);
  generator.ReleaseKey(ui::VKEY_SPACE, ui::EF_NONE);

  EXPECT_TRUE(notification_view()->inline_reply_->GetVisible());
}

// Synthetic scroll events are not supported on Mac in the views
// test framework.
#if defined(OS_MACOSX)
#define MAYBE_SlideOut DISABLED_SlideOut
#else
#define MAYBE_SlideOut SlideOut
#endif
TEST_F(NotificationViewMDTest, MAYBE_SlideOut) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  EXPECT_FALSE(IsRemovedAfterIdle(kDefaultNotificationId));

  BeginScroll();
  ScrollBy(-10);
  EXPECT_FALSE(IsRemovedAfterIdle(kDefaultNotificationId));
  EXPECT_EQ(-10.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_FALSE(IsRemovedAfterIdle(kDefaultNotificationId));
  EXPECT_EQ(0.f, GetNotificationSlideAmount());

  BeginScroll();
  ScrollBy(-200);
  EXPECT_FALSE(IsRemovedAfterIdle(kDefaultNotificationId));
  EXPECT_EQ(-200.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_TRUE(IsRemovedAfterIdle(kDefaultNotificationId));
}

#if defined(OS_MACOSX)
#define MAYBE_SlideOutNested DISABLED_SlideOutNested
#else
#define MAYBE_SlideOutNested SlideOutNested
#endif
TEST_F(NotificationViewMDTest, MAYBE_SlideOutNested) {
  notification_view()->SetIsNested();
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  BeginScroll();
  ScrollBy(-10);
  EXPECT_FALSE(IsRemovedAfterIdle(kDefaultNotificationId));
  EXPECT_EQ(-10.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_FALSE(IsRemovedAfterIdle(kDefaultNotificationId));
  EXPECT_EQ(0.f, GetNotificationSlideAmount());

  BeginScroll();
  ScrollBy(-200);
  EXPECT_FALSE(IsRemovedAfterIdle(kDefaultNotificationId));
  EXPECT_EQ(-200.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_TRUE(IsRemovedAfterIdle(kDefaultNotificationId));
}

#if defined(OS_MACOSX)
#define MAYBE_DisableSlideForcibly DISABLED_DisableSlideForcibly
#else
#define MAYBE_DisableSlideForcibly DisableSlideForcibly
#endif
TEST_F(NotificationViewMDTest, MAYBE_DisableSlideForcibly) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  notification_view()->DisableSlideForcibly(true);

  BeginScroll();
  ScrollBy(-10);
  EXPECT_FALSE(IsRemovedAfterIdle(kDefaultNotificationId));
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_FALSE(IsRemovedAfterIdle(kDefaultNotificationId));
  EXPECT_EQ(0.f, GetNotificationSlideAmount());

  notification_view()->DisableSlideForcibly(false);

  BeginScroll();
  ScrollBy(-10);
  EXPECT_FALSE(IsRemovedAfterIdle(kDefaultNotificationId));
  EXPECT_EQ(-10.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_FALSE(IsRemovedAfterIdle(kDefaultNotificationId));
}

// Pinning notification is ChromeOS only feature.
#if defined(OS_CHROMEOS)

TEST_F(NotificationViewMDTest, SlideOutPinned) {
  notification_view()->SetIsNested();
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_pinned(true);
  UpdateNotificationViews(*notification);

  BeginScroll();
  ScrollBy(-200);
  EXPECT_FALSE(IsRemovedAfterIdle(kDefaultNotificationId));
  EXPECT_LT(-200.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_FALSE(IsRemovedAfterIdle(kDefaultNotificationId));
}

TEST_F(NotificationViewMDTest, Pinned) {
  notification_view()->SetIsNested();
  std::unique_ptr<Notification> notification = CreateSimpleNotification();

  // Visible at the initial state.
  EXPECT_TRUE(GetCloseButton());
  EXPECT_TRUE(GetCloseButton()->GetVisible());

  // Pin.
  notification->set_pinned(true);
  UpdateNotificationViews(*notification);
  EXPECT_FALSE(GetCloseButton());

  // Unpin.
  notification->set_pinned(false);
  UpdateNotificationViews(*notification);
  EXPECT_TRUE(GetCloseButton());
  EXPECT_TRUE(GetCloseButton()->GetVisible());

  // Pin again.
  notification->set_pinned(true);
  UpdateNotificationViews(*notification);
  EXPECT_FALSE(GetCloseButton());
}

TEST_F(NotificationViewMDTest, FixedViewMode) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification_view()->SetSettingMode(true);
  UpdateNotificationViews(*notification);
  std::string notification_id = notification->id();

  BeginScroll();
  ScrollBy(-200);
  EXPECT_FALSE(IsRemovedAfterIdle(notification_id));
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_FALSE(IsRemovedAfterIdle(notification_id));

  EXPECT_EQ(MessageView::Mode::SETTING, notification_view()->GetMode());
}

TEST_F(NotificationViewMDTest, SnoozeButton) {
  // Create notification to replace the current one with itself.
  message_center::RichNotificationData rich_data;
  rich_data.settings_button_handler = SettingsButtonHandler::INLINE;
  rich_data.pinned = true;
  rich_data.should_show_snooze_button = true;
  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      message_center::NOTIFICATION_TYPE_CUSTOM, kDefaultNotificationId,
      base::UTF8ToUTF16("title"), base::UTF8ToUTF16("message"), gfx::Image(),
      base::UTF8ToUTF16("display source"), GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "test_app_id"),
      rich_data, nullptr);

  UpdateNotificationViews(*notification);

  EXPECT_NE(nullptr,
            notification_view()->GetControlButtonsView()->snooze_button());
}

#endif  // defined(OS_CHROMEOS)

TEST_F(NotificationViewMDTest, ExpandLongMessage) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NotificationType::NOTIFICATION_TYPE_SIMPLE);
  // Test in a case where left_content_ does not have views other than
  // message_view_.
  // Without doing this, inappropriate fix such as
  // message_view_->GetPreferredSize() returning gfx::Size() can pass.
  notification->set_title(base::string16());
  notification->set_message(base::ASCIIToUTF16(
      "consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore "
      "et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud "
      "exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat."));

  UpdateNotificationViews(*notification);
  EXPECT_FALSE(notification_view()->expanded_);
  const int collapsed_height = notification_view()->message_view_->height();
  const int collapsed_preferred_height =
      notification_view()->GetPreferredSize().height();
  EXPECT_LT(0, collapsed_height);
  EXPECT_LT(0, collapsed_preferred_height);

  notification_view()->ToggleExpanded();
  EXPECT_TRUE(notification_view()->expanded_);
  EXPECT_LT(collapsed_height, notification_view()->message_view_->height());
  EXPECT_LT(collapsed_preferred_height,
            notification_view()->GetPreferredSize().height());

  notification_view()->ToggleExpanded();
  EXPECT_FALSE(notification_view()->expanded_);
  EXPECT_EQ(collapsed_height, notification_view()->message_view_->height());
  EXPECT_EQ(collapsed_preferred_height,
            notification_view()->GetPreferredSize().height());

  // Test |manually_expanded_or_collapsed| being set when the toggle is done by
  // user interaction.
  EXPECT_FALSE(notification_view()->IsManuallyExpandedOrCollapsed());

  // Construct a mouse click event 1 pixel inside the header.
  gfx::Point done_cursor_location(1, 1);
  views::View::ConvertPointToScreen(notification_view()->header_row_,
                                    &done_cursor_location);
  ui::test::EventGenerator generator(GetRootWindow(widget()));
  generator.MoveMouseTo(done_cursor_location);
  generator.ClickLeftButton();

  EXPECT_TRUE(notification_view()->IsManuallyExpandedOrCollapsed());
}

TEST_F(NotificationViewMDTest, TestAccentColor) {
  constexpr SkColor kActionButtonTextColor = gfx::kGoogleBlue600;
  constexpr SkColor kCustomAccentColor = gfx::kGoogleYellow900;

  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_buttons(CreateButtons(2));
  UpdateNotificationViews(*notification);
  widget()->Show();

  // Action buttons are hidden by collapsed state.
  if (!notification_view()->expanded_)
    notification_view()->ToggleExpanded();
  EXPECT_TRUE(notification_view()->actions_row_->GetVisible());

  // By default, header does not have accent color (default grey), and
  // buttons have default accent color.
  EXPECT_EQ(kNotificationDefaultAccentColor,
            notification_view()->header_row_->accent_color_for_testing());
  EXPECT_EQ(
      kActionButtonTextColor,
      notification_view()->action_buttons_[0]->enabled_color_for_testing());
  EXPECT_EQ(
      kActionButtonTextColor,
      notification_view()->action_buttons_[1]->enabled_color_for_testing());

  // If custom accent color is set, the header and the buttons should have the
  // same accent color.
  notification->set_accent_color(kCustomAccentColor);
  UpdateNotificationViews(*notification);
  EXPECT_EQ(kCustomAccentColor,
            notification_view()->header_row_->accent_color_for_testing());
  EXPECT_EQ(
      kCustomAccentColor,
      notification_view()->action_buttons_[0]->enabled_color_for_testing());
  EXPECT_EQ(
      kCustomAccentColor,
      notification_view()->action_buttons_[1]->enabled_color_for_testing());
}

TEST_F(NotificationViewMDTest, UseImageAsIcon) {
  // TODO(tetsui): Remove duplicated integer literal in CreateOrUpdateIconView.
  const int kNotificationIconSize = 30;

  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NotificationType::NOTIFICATION_TYPE_IMAGE);
  notification->set_icon(
      CreateTestImage(kNotificationIconSize, kNotificationIconSize));

  // Test normal notification.
  UpdateNotificationViews(*notification);
  EXPECT_FALSE(notification_view()->expanded_);
  EXPECT_TRUE(notification_view()->icon_view_->GetVisible());
  EXPECT_TRUE(notification_view()->right_content_->GetVisible());

  // Icon on the right side is still visible when expanded.
  notification_view()->ToggleExpanded();
  EXPECT_TRUE(notification_view()->expanded_);
  EXPECT_TRUE(notification_view()->icon_view_->GetVisible());
  EXPECT_TRUE(notification_view()->right_content_->GetVisible());

  notification_view()->ToggleExpanded();
  EXPECT_FALSE(notification_view()->expanded_);

  // Test notification with |use_image_for_icon| e.g. screenshot preview.
  notification->set_icon(gfx::Image());
  UpdateNotificationViews(*notification);
  EXPECT_TRUE(notification_view()->icon_view_->GetVisible());
  EXPECT_TRUE(notification_view()->right_content_->GetVisible());

  // Icon on the right side is not visible when expanded.
  notification_view()->ToggleExpanded();
  EXPECT_TRUE(notification_view()->expanded_);
  EXPECT_TRUE(notification_view()->icon_view_->GetVisible());
  EXPECT_FALSE(notification_view()->right_content_->GetVisible());
}

TEST_F(NotificationViewMDTest, NotificationWithoutIcon) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_icon(gfx::Image());
  notification->set_image(gfx::Image());
  UpdateNotificationViews(*notification);

  // If the notification has no icon, |icon_view_| shouldn't be created.
  EXPECT_FALSE(notification_view()->icon_view_);
  EXPECT_FALSE(notification_view()->right_content_->GetVisible());

  // Toggling should not affect the icon.
  notification_view()->ToggleExpanded();
  EXPECT_FALSE(notification_view()->icon_view_);
  EXPECT_FALSE(notification_view()->right_content_->GetVisible());
}

TEST_F(NotificationViewMDTest, UpdateAddingIcon) {
  const int kNotificationIconSize = 30;

  // Create a notification without an icon.
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_icon(gfx::Image());
  notification->set_image(gfx::Image());
  UpdateNotificationViews(*notification);

  // Capture the width of the left content without an icon.
  const int left_content_width = notification_view()->left_content_->width();

  // Update the notification, adding an icon.
  notification->set_icon(
      CreateTestImage(kNotificationIconSize, kNotificationIconSize));
  UpdateNotificationViews(*notification);

  // Notification should now have an icon.
  EXPECT_TRUE(notification_view()->icon_view_->GetVisible());
  EXPECT_TRUE(notification_view()->right_content_->GetVisible());

  // There should be some space now to show the icon.
  EXPECT_LT(notification_view()->left_content_->width(), left_content_width);
}

TEST_F(NotificationViewMDTest, UpdateInSettings) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NOTIFICATION_TYPE_SIMPLE);
  UpdateNotificationViews(*notification);

  ui::test::EventGenerator generator(GetRootWindow(widget()));

  // Inline settings will be shown by clicking settings button.
  EXPECT_FALSE(notification_view()->settings_row_->GetVisible());
  gfx::Point settings_cursor_location(1, 1);
  views::View::ConvertPointToTarget(
      notification_view()->control_buttons_view_->settings_button(),
      notification_view(), &settings_cursor_location);
  generator.MoveMouseTo(settings_cursor_location);
  generator.ClickLeftButton();
  EXPECT_TRUE(notification_view()->settings_row_->GetVisible());

  // Trigger an update event.
  UpdateNotificationViews(*notification);

  // The close button should be visible.
  views::Button* close_button =
      notification_view()->control_buttons_view_->close_button();
  ASSERT_NE(nullptr, close_button);
  EXPECT_TRUE(close_button->GetVisible());
}

TEST_F(NotificationViewMDTest, InlineSettings) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NOTIFICATION_TYPE_SIMPLE);
  UpdateNotificationViews(*notification);

  ui::test::EventGenerator generator(GetRootWindow(widget()));

  // Inline settings will be shown by clicking settings button.
  EXPECT_FALSE(notification_view()->settings_row_->GetVisible());
  gfx::Point settings_cursor_location(1, 1);
  views::View::ConvertPointToTarget(
      notification_view()->control_buttons_view_->settings_button(),
      notification_view(), &settings_cursor_location);
  generator.MoveMouseTo(settings_cursor_location);
  generator.ClickLeftButton();
  EXPECT_TRUE(notification_view()->settings_row_->GetVisible());

#if !defined(OS_CHROMEOS)
  // By clicking settings button again, it will toggle. Skip this on ChromeOS as
  // the control_buttons_view gets hidden when the inline settings are shown.
  generator.ClickLeftButton();
  EXPECT_FALSE(notification_view()->settings_row_->GetVisible());

  // Show inline settings again.
  generator.ClickLeftButton();
  EXPECT_TRUE(notification_view()->settings_row_->GetVisible());
#endif

  // Construct a mouse click event 1 pixel inside the done button.
  gfx::Point done_cursor_location(1, 1);
  views::View::ConvertPointToTarget(notification_view()->settings_done_button_,
                                    notification_view(), &done_cursor_location);
  generator.MoveMouseTo(done_cursor_location);
  generator.ClickLeftButton();

  // Just clicking Done button should not change the setting.
  EXPECT_FALSE(notification_view()->settings_row_->GetVisible());
  EXPECT_FALSE(delegate_->disable_notification_called());

  generator.MoveMouseTo(settings_cursor_location);
  generator.ClickLeftButton();
  EXPECT_TRUE(notification_view()->settings_row_->GetVisible());

  // Construct a mouse click event 1 pixel inside the block all button.
  gfx::Point block_cursor_location(1, 1);
  views::View::ConvertPointToTarget(notification_view()->block_all_button_,
                                    notification_view(),
                                    &block_cursor_location);
  generator.MoveMouseTo(block_cursor_location);
  generator.ClickLeftButton();
  generator.MoveMouseTo(done_cursor_location);
  generator.ClickLeftButton();

  EXPECT_FALSE(notification_view()->settings_row_->GetVisible());
  EXPECT_TRUE(delegate_->disable_notification_called());
}

TEST_F(NotificationViewMDTest, InlineSettingsInkDropAnimation) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NOTIFICATION_TYPE_SIMPLE);
  UpdateNotificationViews(*notification);

  ui::test::EventGenerator generator(GetRootWindow(widget()));

  // Inline settings will be shown by clicking settings button.
  EXPECT_FALSE(notification_view()->settings_row_->GetVisible());
  gfx::Point settings_cursor_location(1, 1);
  views::View::ConvertPointToTarget(
      notification_view()->control_buttons_view_->settings_button(),
      notification_view(), &settings_cursor_location);
  generator.MoveMouseTo(settings_cursor_location);
  generator.ClickLeftButton();
  EXPECT_TRUE(notification_view()->settings_row_->GetVisible());

  notification_view()->GetInkDrop()->AddObserver(this);

  // Resize the widget by 1px to simulate the expand animation.
  gfx::Rect size = widget()->GetWindowBoundsInScreen();
  size.Inset(0, 0, 0, 1);
  widget()->SetBounds(size);

  notification_view()->GetInkDrop()->RemoveObserver(this);

  // The ink drop animation should still be running.
  EXPECT_FALSE(ink_drop_stopped());
}

TEST_F(NotificationViewMDTest, TestClick) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  delegate_->set_expecting_click(true);

  UpdateNotificationViews(*notification);
  widget()->Show();

  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));

  // Collapse the notification if it's expanded.
  if (notification_view()->expanded_)
    notification_view()->ToggleExpanded();
  EXPECT_FALSE(notification_view()->actions_row_->GetVisible());

  // Now construct a mouse click event 2 pixel inside from the bottom.
  gfx::Point cursor_location(notification_view()->size().width() / 2,
                             notification_view()->size().height() - 2);
  generator.MoveMouseTo(cursor_location);
  generator.ClickLeftButton();

  EXPECT_TRUE(delegate_->clicked());
}

TEST_F(NotificationViewMDTest, TestClickExpanded) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  delegate_->set_expecting_click(true);

  UpdateNotificationViews(*notification);
  widget()->Show();

  ui::test::EventGenerator generator(GetRootWindow(widget()));

  // Expand the notification if it's collapsed.
  if (!notification_view()->expanded_)
    notification_view()->ToggleExpanded();
  EXPECT_FALSE(notification_view()->actions_row_->GetVisible());

  // Now construct a mouse click event 2 pixel inside from the bottom.
  gfx::Point cursor_location(notification_view()->size().width() / 2,
                             notification_view()->size().height() - 2);
  generator.MoveMouseTo(cursor_location);
  generator.ClickLeftButton();

  EXPECT_TRUE(delegate_->clicked());
}

TEST_F(NotificationViewMDTest, TestDeleteOnToggleExpanded) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NotificationType::NOTIFICATION_TYPE_SIMPLE);
  notification->set_title(base::string16());
  notification->set_message(base::ASCIIToUTF16(
      "consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore "
      "et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud "
      "exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat."));
  UpdateNotificationViews(*notification);
  EXPECT_FALSE(notification_view()->expanded_);

  // The view can be deleted by PreferredSizeChanged(). https://crbug.com/918933
  set_delete_on_preferred_size_changed(true);
  notification_view()->ButtonPressed(notification_view()->header_row_,
                                     DummyEvent());
}

TEST_F(NotificationViewMDTest, TestDeleteOnDisableNotification) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NOTIFICATION_TYPE_SIMPLE);
  UpdateNotificationViews(*notification);

  notification_view()->OnSettingsButtonPressed(DummyEvent());
  notification_view()->block_all_button_->NotifyClick(DummyEvent());

  // After DisableNotification() is called, |notification_view| can be deleted.
  // https://crbug.com/924922
  set_delete_on_notification_removed(true);
  notification_view()->ButtonPressed(notification_view()->settings_done_button_,
                                     DummyEvent());
}

TEST_F(NotificationViewMDTest, TestLongTitleAndMessage) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NotificationType::NOTIFICATION_TYPE_SIMPLE);
  notification->set_title(base::ASCIIToUTF16("title"));
  notification->set_message(base::ASCIIToUTF16(
      "consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore "
      "et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud "
      "exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat."));
  UpdateNotificationViews(*notification);
  notification_view()->ToggleExpanded();

  // Get the height of the message view with a short title.
  const int message_height = notification_view()->message_view_->height();

  notification->set_title(base::ASCIIToUTF16(
      "consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore "
      "et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud "
      "exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat."));
  UpdateNotificationViews(*notification);

  // The height of the message view should stay the same with a long title.
  EXPECT_EQ(message_height, notification_view()->message_view_->height());
}

TEST_F(NotificationViewMDTest, AppNameExtension) {
  base::string16 app_name = base::UTF8ToUTF16("extension name");
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_context_message(app_name);

  UpdateNotificationViews(*notification);

  EXPECT_EQ(app_name, notification_view()->header_row_->app_name_for_testing());
}

TEST_F(NotificationViewMDTest, AppNameSystemNotification) {
  base::string16 app_name = base::UTF8ToUTF16("system notification");
  message_center::MessageCenter::Get()->SetSystemNotificationAppName(app_name);
  RichNotificationData data;
  data.settings_button_handler = SettingsButtonHandler::INLINE;
  auto notification = std::make_unique<Notification>(
      NOTIFICATION_TYPE_BASE_FORMAT, std::string(kDefaultNotificationId),
      base::UTF8ToUTF16("title"), base::UTF8ToUTF16("message"), gfx::Image(),
      base::string16(), GURL(),
      NotifierId(NotifierType::SYSTEM_COMPONENT, "system"), data, nullptr);

  UpdateNotificationViews(*notification);

  EXPECT_EQ(app_name, notification_view()->header_row_->app_name_for_testing());
}

TEST_F(NotificationViewMDTest, AppNameWebNotification) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_origin_url(GURL("http://example.com"));

  UpdateNotificationViews(*notification);

  EXPECT_EQ(base::UTF8ToUTF16("example.com"),
            notification_view()->header_row_->app_name_for_testing());
}

}  // namespace message_center
