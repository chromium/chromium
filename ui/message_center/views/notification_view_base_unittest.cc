// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view_base.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/test_event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget_utils.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/notifier_catalogs.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace message_center {

namespace {

constexpr char kDefaultNotificationId[] = "notification id";

class TestNotificationView : public NotificationViewBase {
 public:
  explicit TestNotificationView(const Notification& notification)
      : NotificationViewBase(notification) {
    // Instantiate view instances and add them to a view hierarchy to prevent
    // memory leak.
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));
    AddChildView(CreateHeaderRowBuilder().Build());
    AddChildView(CreateControlButtonsBuilder().Build());
    AddChildView(CreateContentRowBuilder().Build());
    AddChildView(CreateLeftContentBuilder().Build());
    AddChildView(CreateRightContentBuilder().Build());
    AddChildView(CreateImageContainerBuilder().Build());
    AddChildView(CreateInlineSettingsBuilder().Build());
    AddChildView(CreateActionsRow());

    CreateOrUpdateViews(notification);
    UpdateControlButtonsVisibilityWithNotification(notification);
  }
  TestNotificationView(const TestNotificationView&) = delete;
  TestNotificationView& operator=(const TestNotificationView&) = delete;
  ~TestNotificationView() override = default;

  // NotificationViewBase:
  void CreateOrUpdateTitleView(const Notification& notification) override {}
  gfx::Size GetIconViewSize() const override { return gfx::Size(); }
  int GetLargeImageViewMaxWidth() const override { return 0; }
  void CreateOrUpdateSmallIconView(const Notification& notification) override {}
  void CreateOrUpdateInlineSettingsViews(
      const Notification& notification) override {
    set_inline_settings_enabled(
        notification.rich_notification_data().settings_button_handler ==
        message_center::SettingsButtonHandler::INLINE);
  }
  void CreateOrUpdateSnoozeSettingsViews(
      const Notification& notification) override {
    set_snooze_settings_enabled(notification.notifier_id().type ==
                                message_center::NotifierType::ARC_APPLICATION);
  }
  bool IsExpandable() const override { return true; }
  std::unique_ptr<views::LabelButton> GenerateNotificationLabelButton(
      views::Button::PressedCallback callback,
      const std::u16string& label) override {
    return std::make_unique<views::LabelButton>(std::move(callback), label);
  }
};

class NotificationTestDelegate : public NotificationDelegate {
 public:
  NotificationTestDelegate() = default;
  NotificationTestDelegate(const NotificationTestDelegate&) = delete;
  NotificationTestDelegate& operator=(const NotificationTestDelegate&) = delete;

  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
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
    submitted_reply_string_ = reply.value_or(std::u16string());
  }

  void Reset() {
    clicked_ = false;
    clicked_button_index_ = -1;
    submitted_reply_string_.clear();
  }

  bool clicked() const { return clicked_; }
  int clicked_button_index() const { return clicked_button_index_; }
  const std::u16string& submitted_reply_string() const {
    return submitted_reply_string_;
  }
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
  std::u16string submitted_reply_string_;
  bool expecting_click_ = false;
  bool expecting_button_click_ = false;
  bool expecting_reply_submission_ = false;
};

}  // namespace

class NotificationViewBaseTest : public views::ViewsTestBase,
                                 public views::ViewObserver {
 public:
  NotificationViewBaseTest();
  NotificationViewBaseTest(const NotificationViewBaseTest&) = delete;
  NotificationViewBaseTest& operator=(const NotificationViewBaseTest&) = delete;
  ~NotificationViewBaseTest() override;

  // Overridden from ViewsTestBase:
  void SetUp() override;
  void TearDown() override;

  // Overridden from views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override;

  NotificationViewBase* notification_view() const { return notification_view_; }

  void set_delete_on_preferred_size_changed(
      bool delete_on_preferred_size_changed) {
    delete_on_preferred_size_changed_ = delete_on_preferred_size_changed;
  }

  void SetHasMessageCenterView(bool has_message_center_view) {
    MessageCenter::Get()->SetHasMessageCenterView(has_message_center_view);
  }

  void ToggleExpanded() {
    notification_view_->SetExpanded(!notification_view_->IsExpanded());
  }

 protected:
  std::vector<ButtonInfo> CreateButtons(int number);
  std::unique_ptr<Notification> CreateSimpleNotification() const;
  std::unique_ptr<Notification> CreateSimpleNotificationWithRichData(
      const RichNotificationData& optional_fields) const;

  void UpdateNotificationViews(const Notification& notification);
  float GetNotificationSlideAmount() const;
  bool IsRemovedAfterIdle(const std::string& notification_id) const;
  bool IsPopupRemovedAfterIdle(const std::string& notification_id) const;
  void DispatchGesture(const ui::GestureEventDetails& details);
  void BeginScroll();
  void EndScroll();
  void ScrollBy(int dx);
  views::View* GetCloseButton();

  bool delete_on_preferred_size_changed_ = false;
  std::set<std::string> removed_ids_;
  scoped_refptr<NotificationTestDelegate> delegate_;
  raw_ptr<NotificationViewBase> notification_view_ = nullptr;
};

NotificationViewBaseTest::NotificationViewBaseTest() = default;
NotificationViewBaseTest::~NotificationViewBaseTest() = default;

std::unique_ptr<Notification>
NotificationViewBaseTest::CreateSimpleNotification() const {
  RichNotificationData data;
  data.settings_button_handler = SettingsButtonHandler::INLINE;
  return CreateSimpleNotificationWithRichData(data);
}

std::unique_ptr<Notification>
NotificationViewBaseTest::CreateSimpleNotificationWithRichData(
    const RichNotificationData& data) const {
  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, std::string(kDefaultNotificationId), u"title",
      u"message",
      ui::ImageModel::FromImage(gfx::test::CreateImage(/*size=*/80)),
      u"display source", GURL(),
      NotifierId(NotifierType::APPLICATION, "extension_id"), data, delegate_);
  notification->SetSmallImage(gfx::test::CreateImage(/*size=*/16));
  notification->SetImage(gfx::test::CreateImage(320, 240));

  return notification;
}

void NotificationViewBaseTest::SetUp() {
  views::ViewsTestBase::SetUp();

  MessageCenter::Initialize();

  // Create a dummy notification.
  delegate_ = new NotificationTestDelegate();

  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  UpdateNotificationViews(*notification);
}

void NotificationViewBaseTest::TearDown() {
  DCHECK(notification_view_ || delete_on_preferred_size_changed_);
  if (notification_view_) {
    static_cast<views::View*>(notification_view_)->RemoveObserver(this);
    notification_view_->GetWidget()->Close();
    notification_view_ = nullptr;
  }
  MessageCenter::Shutdown();
  views::ViewsTestBase::TearDown();
}

void NotificationViewBaseTest::OnViewPreferredSizeChanged(
    views::View* observed_view) {
  EXPECT_EQ(observed_view, notification_view());
  if (delete_on_preferred_size_changed_) {
    notification_view_->GetWidget()->CloseNow();
    notification_view_ = nullptr;
    return;
  }
  notification_view_->GetWidget()->SetSize(
      notification_view()->GetPreferredSize({}));
}

std::vector<ButtonInfo> NotificationViewBaseTest::CreateButtons(int number) {
  ButtonInfo info(u"Test button.");
  return std::vector<ButtonInfo>(number, info);
}

void NotificationViewBaseTest::UpdateNotificationViews(
    const Notification& notification) {
  MessageCenter::Get()->AddNotification(
      std::make_unique<Notification>(notification));

  if (!notification_view_) {
    // Then create a new NotificationViewBase with that single notification.
    // In the actual code path, this is instantiated by
    // MessageViewFactory::Create.
    // TODO(tetsui): Confirm that NotificationViewBase options are same as one
    // created by the method.
    auto notification_view =
        std::make_unique<TestNotificationView>(notification);
    static_cast<views::View*>(notification_view.get())->AddObserver(this);

    views::Widget::InitParams init_params(
        CreateParams(views::Widget::InitParams::TYPE_POPUP));
    // The native widget owns |widget| and |widget| owns |notification_view_|.
    auto* widget = new views::Widget();
    widget->Init(std::move(init_params));
    notification_view_ = widget->SetContentsView(std::move(notification_view));
    widget->SetSize(notification_view_->GetPreferredSize({}));
    widget->Show();
    widget->widget_delegate()->SetCanActivate(true);
    widget->Activate();
  } else {
    notification_view_->UpdateWithNotification(notification);
  }
}

float NotificationViewBaseTest::GetNotificationSlideAmount() const {
  return notification_view_->GetSlideOutLayer()
      ->transform()
      .To2dTranslation()
      .x();
}

bool NotificationViewBaseTest::IsRemovedAfterIdle(
    const std::string& notification_id) const {
  base::RunLoop().RunUntilIdle();
  return !MessageCenter::Get()->FindVisibleNotificationById(notification_id);
}

bool NotificationViewBaseTest::IsPopupRemovedAfterIdle(
    const std::string& notification_id) const {
  base::RunLoop().RunUntilIdle();
  return !MessageCenter::Get()->FindPopupNotificationById(notification_id);
}

void NotificationViewBaseTest::DispatchGesture(
    const ui::GestureEventDetails& details) {
  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));
  ui::GestureEvent event(0, 0, 0, ui::EventTimeForNow(), details);
  generator.Dispatch(&event);
}

void NotificationViewBaseTest::BeginScroll() {
  DispatchGesture(ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));
}

void NotificationViewBaseTest::EndScroll() {
  DispatchGesture(ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));
}

void NotificationViewBaseTest::ScrollBy(int dx) {
  DispatchGesture(
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, dx, 0));
}

views::View* NotificationViewBaseTest::GetCloseButton() {
  return notification_view()->GetControlButtonsView()->close_button();
}

/* Unit tests *****************************************************************/

// TODO(tetsui): Following tests are not yet ported from
// NotificationViewBaseTest.
// * CreateOrUpdateTestSettingsButton
// * TestLineLimits
// * TestImageSizing
// * SettingsButtonTest
// * ViewOrderingTest
// * FormatContextMessageTest

TEST_F(NotificationViewBaseTest, CreateOrUpdateTest) {
  EXPECT_NE(nullptr, notification_view()->message_label_);
  EXPECT_NE(nullptr, notification_view()->icon_view_);
  EXPECT_NE(nullptr, notification_view()->image_container_view_);

  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->SetImage(gfx::Image());
  notification->set_title(std::u16string());
  notification->set_message(std::u16string());
  notification->set_icon(ui::ImageModel());

  notification_view()->CreateOrUpdateViews(*notification);

  EXPECT_EQ(nullptr, notification_view()->message_label_.get());
  EXPECT_TRUE(notification_view()->image_container_view_->children().empty());
  EXPECT_EQ(nullptr, notification_view()->icon_view_.get());
}

TEST_F(NotificationViewBaseTest, UpdateButtonsStateTest) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification_view()->CreateOrUpdateViews(*notification);
  notification_view()->GetWidget()->Show();

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
  ToggleExpanded();
  EXPECT_TRUE(notification_view()->expanded_);
  notification->set_buttons(CreateButtons(2));
  notification_view()->CreateOrUpdateViews(*notification);
  EXPECT_TRUE(notification_view()->actions_row_->GetVisible());

  EXPECT_EQ(views::Button::STATE_NORMAL,
            notification_view()->action_buttons_[0]->GetState());

  // Now construct a mouse move event inside the boundary of the action button.
  gfx::Point cursor_location = notification_view()
                                   ->action_buttons_[0]
                                   ->GetBoundsInScreen()
                                   .CenterPoint();
  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));
  generator.MoveMouseTo(cursor_location);

  EXPECT_EQ(views::Button::STATE_HOVERED,
            notification_view()->action_buttons_[0]->GetState());

  notification_view()->CreateOrUpdateViews(*notification);

  EXPECT_EQ(views::Button::STATE_HOVERED,
            notification_view()->action_buttons_[0]->GetState());

  // Now construct a mouse move event outside the boundary of the widget.
  cursor_location =
      notification_view()->action_buttons_[0]->GetBoundsInScreen().origin() +
      gfx::Vector2d(-1, -1);
  generator.MoveMouseTo(cursor_location);

  EXPECT_EQ(views::Button::STATE_NORMAL,
            notification_view()->action_buttons_[0]->GetState());
}

TEST_F(NotificationViewBaseTest, UpdateButtonCountTest) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_buttons(CreateButtons(2));
  UpdateNotificationViews(*notification);
  notification_view()->GetWidget()->Show();

  // Action buttons are hidden by collapsed state.
  if (!notification_view()->expanded_)
    ToggleExpanded();
  EXPECT_TRUE(notification_view()->actions_row_->GetVisible());

  EXPECT_EQ(views::Button::STATE_NORMAL,
            notification_view()->action_buttons_[0]->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL,
            notification_view()->action_buttons_[1]->GetState());

  // Now construct a mouse move event inside the boundary of the action button.
  gfx::Point cursor_location = notification_view()
                                   ->action_buttons_[0]
                                   ->GetBoundsInScreen()
                                   .CenterPoint();
  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));
  generator.MoveMouseTo(cursor_location);

  EXPECT_EQ(views::Button::STATE_HOVERED,
            notification_view()->action_buttons_[0]->GetState());
  EXPECT_EQ(views::Button::STATE_NORMAL,
            notification_view()->action_buttons_[1]->GetState());

  notification->set_buttons(CreateButtons(1));
  UpdateNotificationViews(*notification);

  EXPECT_EQ(views::Button::STATE_HOVERED,
            notification_view()->action_buttons_[0]->GetState());
  EXPECT_EQ(1u, notification_view()->action_buttons_.size());

  // Now construct a mouse move event outside the boundary of the widget.
  cursor_location =
      notification_view()->action_buttons_[0]->GetBoundsInScreen().origin() +
      gfx::Vector2d(-1, -1);
  generator.MoveMouseTo(cursor_location);

  EXPECT_EQ(views::Button::STATE_NORMAL,
            notification_view()->action_buttons_[0]->GetState());
}

TEST_F(NotificationViewBaseTest, TestActionButtonClick) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  delegate_->set_expecting_button_click(true);

  notification->set_buttons(CreateButtons(2));
  UpdateNotificationViews(*notification);
  notification_view()->GetWidget()->Show();

  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));

  // Action buttons are hidden by collapsed state.
  if (!notification_view()->expanded_)
    ToggleExpanded();
  EXPECT_TRUE(notification_view()->actions_row_->GetVisible());

  // Now construct a mouse click event inside the boundary of the action button.
  gfx::Point cursor_location = notification_view()
                                   ->action_buttons_[1]
                                   ->GetBoundsInScreen()
                                   .CenterPoint();
  generator.MoveMouseTo(cursor_location);
  generator.ClickLeftButton();

  EXPECT_EQ(1, delegate_->clicked_button_index());
}

// TODO(crbug.com/40780100): Test failing on linux-lacros-tester-rel and ozone.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_OZONE)
#define MAYBE_TestInlineReply DISABLED_TestInlineReply
#else
#define MAYBE_TestInlineReply TestInlineReply
#endif
TEST_F(NotificationViewBaseTest, MAYBE_TestInlineReply) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  delegate_->set_expecting_reply_submission(true);

  std::vector<ButtonInfo> buttons = CreateButtons(2);
  buttons[1].placeholder = std::u16string();
  notification->set_buttons(buttons);
  UpdateNotificationViews(*notification);
  notification_view()->GetWidget()->Show();

  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));

  // Action buttons are hidden by collapsed state.
  if (!notification_view()->expanded_)
    ToggleExpanded();
  EXPECT_TRUE(notification_view()->actions_row_->GetVisible());

  // Now construct a mouse click event inside the boundary of the action button.
  gfx::Point cursor_location = notification_view()
                                   ->action_buttons_[1]
                                   ->GetBoundsInScreen()
                                   .CenterPoint();
  generator.MoveMouseTo(cursor_location);
  generator.ClickLeftButton();

  // Nothing should be submitted at this point.
  EXPECT_EQ(-1, delegate_->clicked_button_index());

  // Toggling should hide the inline textfield.
  EXPECT_TRUE(notification_view()->inline_reply_->GetVisible());
  ToggleExpanded();
  ToggleExpanded();
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
  EXPECT_EQ(u"test", delegate_->submitted_reply_string());

  // Reset values.
  delegate_->Reset();

  // Now construct a mouse click event inside the boundary of the action button.
  cursor_location = notification_view()
                        ->action_buttons_[0]
                        ->GetBoundsInScreen()
                        .CenterPoint();
  generator.MoveMouseTo(cursor_location);
  generator.ClickLeftButton();

  // Nothing should be submitted at this point.
  EXPECT_EQ(-1, delegate_->clicked_button_index());
  EXPECT_EQ(std::u16string(), delegate_->submitted_reply_string());

  // Click the button again and focus on the inline textfield.
  generator.ClickLeftButton();

  // Type the text.
  for (ui::KeyboardCode keycode : keycodes) {
    generator.PressKey(keycode, ui::EF_NONE);
    generator.ReleaseKey(keycode, ui::EF_NONE);
  }

  // Submit by clicking the reply button.
  cursor_location = notification_view()
                        ->inline_reply_->button()
                        ->GetBoundsInScreen()
                        .CenterPoint();
  generator.MoveMouseTo(cursor_location);
  generator.ClickLeftButton();
  EXPECT_EQ(1, delegate_->clicked_button_index());
  EXPECT_EQ(u"test", delegate_->submitted_reply_string());
}

TEST_F(NotificationViewBaseTest, TestInlineReplyRemovedByUpdate) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();

  std::vector<ButtonInfo> buttons = CreateButtons(2);
  buttons[1].placeholder = std::u16string();
  notification->set_buttons(buttons);
  UpdateNotificationViews(*notification);
  notification_view()->GetWidget()->Show();

  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));

  // Action buttons are hidden by collapsed state.
  if (!notification_view()->expanded_)
    ToggleExpanded();
  EXPECT_TRUE(notification_view()->actions_row_->GetVisible());

  // Now construct a mouse click event inside the boundary of the action button.
  gfx::Point cursor_location = notification_view()
                                   ->action_buttons_[1]
                                   ->GetBoundsInScreen()
                                   .CenterPoint();
  generator.MoveMouseTo(cursor_location);
  generator.ClickLeftButton();

  // Nothing should be submitted at this point.
  EXPECT_EQ(-1, delegate_->clicked_button_index());

  EXPECT_TRUE(notification_view()->inline_reply_->GetVisible());
  EXPECT_FALSE(notification_view()->action_buttons_row_->GetVisible());

  buttons[1].placeholder = std::nullopt;
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

TEST_F(NotificationViewBaseTest, TestInlineReplyActivateWithKeyPress) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();

  std::vector<ButtonInfo> buttons = CreateButtons(2);
  buttons[1].placeholder = std::u16string();
  notification->set_buttons(buttons);
  UpdateNotificationViews(*notification);
  notification_view()->GetWidget()->Show();

  // Action buttons are hidden by collapsed state.
  if (!notification_view()->expanded_)
    ToggleExpanded();

  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));

  // Press and release space key to open inline reply text field.
  // Note: VKEY_RETURN should work too, but triggers a click on MacOS.
  notification_view()->action_buttons_[1]->RequestFocus();
  generator.PressKey(ui::VKEY_SPACE, ui::EF_NONE);
  generator.ReleaseKey(ui::VKEY_SPACE, ui::EF_NONE);

  EXPECT_TRUE(notification_view()->inline_reply_->GetVisible());
}

// Synthetic scroll events are not supported on Mac in the views
// test framework.
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
#define MAYBE_SlideOut DISABLED_SlideOut
#else
#define MAYBE_SlideOut SlideOut
#endif
TEST_F(NotificationViewBaseTest, MAYBE_SlideOut) {
  SetHasMessageCenterView(/*has_message_center_view=*/false);

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

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
#define MAYBE_SlideOutNested DISABLED_SlideOutNested
#else
#define MAYBE_SlideOutNested SlideOutNested
#endif
TEST_F(NotificationViewBaseTest, MAYBE_SlideOutNested) {
  SetHasMessageCenterView(/*has_message_center_view=*/false);

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

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
#define MAYBE_DisableSlideForcibly DISABLED_DisableSlideForcibly
#else
#define MAYBE_DisableSlideForcibly DisableSlideForcibly
#endif
TEST_F(NotificationViewBaseTest, MAYBE_DisableSlideForcibly) {
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
#if BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(NotificationViewBaseTest, SlideOutPinned) {
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

TEST_F(NotificationViewBaseTest, Pinned) {
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

TEST_F(NotificationViewBaseTest, FixedViewMode) {
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

TEST_F(NotificationViewBaseTest, SnoozeButton) {
  MessageCenter::Get()->RemoveAllNotifications(/*by_user=*/false,
                                               MessageCenter::RemoveType::ALL);

  // Create notification to replace the current one with itself.
  message_center::RichNotificationData rich_data;
  rich_data.settings_button_handler = SettingsButtonHandler::INLINE;
  rich_data.pinned = true;
  rich_data.should_show_snooze_button = true;
  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      message_center::NOTIFICATION_TYPE_CUSTOM, kDefaultNotificationId,
      u"title", u"message", ui::ImageModel(), u"display source", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "test_app_id"),
      rich_data, nullptr);

  UpdateNotificationViews(*notification);

  EXPECT_NE(nullptr,
            notification_view()->GetControlButtonsView()->snooze_button());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(NotificationViewBaseTest, UseImageAsIcon) {
  // TODO(tetsui): Remove duplicated integer literal in CreateOrUpdateIconView.
  const int kIconSize = 30;

  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NotificationType::NOTIFICATION_TYPE_IMAGE);
  notification->set_icon(
      ui::ImageModel::FromImage(gfx::test::CreateImage(kIconSize)));

  // Test normal notification.
  UpdateNotificationViews(*notification);
  EXPECT_FALSE(notification_view()->expanded_);
  EXPECT_TRUE(notification_view()->icon_view_->GetVisible());
  EXPECT_TRUE(notification_view()->right_content_->GetVisible());

  // Icon on the right side is still visible when expanded.
  ToggleExpanded();
  EXPECT_TRUE(notification_view()->expanded_);
  EXPECT_TRUE(notification_view()->icon_view_->GetVisible());
  EXPECT_TRUE(notification_view()->right_content_->GetVisible());

  ToggleExpanded();
  EXPECT_FALSE(notification_view()->expanded_);

  // Test notification with |use_image_for_icon| e.g. screenshot preview.
  notification->set_icon(ui::ImageModel());
  UpdateNotificationViews(*notification);
  EXPECT_TRUE(notification_view()->icon_view_->GetVisible());
  EXPECT_TRUE(notification_view()->right_content_->GetVisible());

  // Icon on the right side is not visible when expanded.
  ToggleExpanded();
  EXPECT_TRUE(notification_view()->expanded_);
  EXPECT_TRUE(notification_view()->icon_view_->GetVisible());
  EXPECT_FALSE(notification_view()->right_content_->GetVisible());
}

TEST_F(NotificationViewBaseTest, NotificationWithoutIcon) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_icon(ui::ImageModel());
  notification->SetImage(gfx::Image());
  UpdateNotificationViews(*notification);

  // If the notification has no icon, |icon_view_| shouldn't be created.
  EXPECT_FALSE(notification_view()->icon_view_);
  EXPECT_FALSE(notification_view()->right_content_->GetVisible());

  // Toggling should not affect the icon.
  ToggleExpanded();
  EXPECT_FALSE(notification_view()->icon_view_);
  EXPECT_FALSE(notification_view()->right_content_->GetVisible());
}

TEST_F(NotificationViewBaseTest, UpdateAddingIcon) {
  const int kIconSize = 30;

  // Create a notification without an icon.
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_icon(ui::ImageModel());
  notification->SetImage(gfx::Image());
  UpdateNotificationViews(*notification);

  // Update the notification, adding an icon.
  notification->set_icon(
      ui::ImageModel::FromImage(gfx::test::CreateImage(kIconSize)));
  UpdateNotificationViews(*notification);

  // Notification should now have an icon.
  EXPECT_TRUE(notification_view()->icon_view_->GetVisible());
  EXPECT_TRUE(notification_view()->right_content_->GetVisible());
}

TEST_F(NotificationViewBaseTest, UpdateInSettings) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NOTIFICATION_TYPE_SIMPLE);
  UpdateNotificationViews(*notification);

  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));

  // Inline settings will be shown by clicking settings button.
  EXPECT_FALSE(notification_view()->settings_row_->GetVisible());
  gfx::Point settings_cursor_location =
      notification_view()
          ->control_buttons_view_->settings_button()
          ->GetBoundsInScreen()
          .CenterPoint();
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

TEST_F(NotificationViewBaseTest, InlineSettings) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NOTIFICATION_TYPE_SIMPLE);
  UpdateNotificationViews(*notification);

  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));

  // Inline settings will be shown by clicking settings button.
  EXPECT_FALSE(notification_view()->settings_row_->GetVisible());
  gfx::Point settings_cursor_location =
      notification_view()
          ->control_buttons_view_->settings_button()
          ->GetBoundsInScreen()
          .CenterPoint();
  generator.MoveMouseTo(settings_cursor_location);
  generator.ClickLeftButton();
  EXPECT_TRUE(notification_view()->settings_row_->GetVisible());

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // By clicking settings button again, it will toggle. Skip this on ChromeOS as
  // the control_buttons_view gets hidden when the inline settings are shown.
  generator.ClickLeftButton();
  EXPECT_FALSE(notification_view()->settings_row_->GetVisible());

  // Show inline settings again.
  generator.ClickLeftButton();
  EXPECT_TRUE(notification_view()->settings_row_->GetVisible());
#endif
}

TEST_F(NotificationViewBaseTest, TestClick) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  delegate_->set_expecting_click(true);

  UpdateNotificationViews(*notification);
  notification_view()->GetWidget()->Show();

  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));

  // Collapse the notification if it's expanded.
  if (notification_view()->expanded_)
    ToggleExpanded();
  EXPECT_FALSE(notification_view()->actions_row_->GetVisible());

  // Now construct a mouse click event 2 pixel inside from the bottom.
  gfx::Point cursor_location(notification_view()->size().width() / 2,
                             notification_view()->size().height() - 2);
  gfx::Point cursor_in_screen =
      views::View::ConvertPointToScreen(notification_view(), cursor_location);
  generator.MoveMouseTo(cursor_in_screen);
  generator.ClickLeftButton();

  EXPECT_TRUE(delegate_->clicked());
}

TEST_F(NotificationViewBaseTest, TestClickExpanded) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  delegate_->set_expecting_click(true);

  UpdateNotificationViews(*notification);
  notification_view()->GetWidget()->Show();

  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));

  // Expand the notification if it's collapsed.
  if (!notification_view()->expanded_)
    ToggleExpanded();
  EXPECT_FALSE(notification_view()->actions_row_->GetVisible());

  // Now construct a mouse click event 2 pixel inside from the bottom.
  gfx::Point cursor_location(notification_view()->size().width() / 2,
                             notification_view()->size().height() - 2);
  gfx::Point cursor_in_screen =
      views::View::ConvertPointToScreen(notification_view(), cursor_location);
  generator.MoveMouseTo(cursor_in_screen);
  generator.ClickLeftButton();

  EXPECT_TRUE(delegate_->clicked());
}

TEST_F(NotificationViewBaseTest, TestDeleteOnToggleExpanded) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NotificationType::NOTIFICATION_TYPE_SIMPLE);
  notification->set_title(std::u16string());
  notification->set_message(
      u"consectetur adipiscing elit, sed do eiusmod tempor incididunt ut "
      u"labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud "
      u"exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
  UpdateNotificationViews(*notification);
  EXPECT_FALSE(notification_view()->expanded_);

  // The view can be deleted by PreferredSizeChanged(). https://crbug.com/918933
  set_delete_on_preferred_size_changed(true);
  views::test::ButtonTestApi(notification_view()->header_row_)
      .NotifyClick(ui::test::TestEvent());
}

TEST_F(NotificationViewBaseTest, TestLongTitleAndMessage) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NotificationType::NOTIFICATION_TYPE_SIMPLE);
  notification->set_title(u"title");
  notification->set_message(
      u"consectetur adipiscing elit, sed do eiusmod tempor incididunt ut "
      u"labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud "
      u"exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
  UpdateNotificationViews(*notification);
  ToggleExpanded();

  // Get the height of the message view with a short title.
  const int message_height = notification_view()->message_label_->height();

  notification->set_title(
      u"consectetur adipiscing elit, sed do eiusmod tempor incididunt ut "
      u"labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud "
      u"exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
  UpdateNotificationViews(*notification);

  // The height of the message view should stay the same with a long title.
  EXPECT_EQ(message_height, notification_view()->message_label_->height());
}

TEST_F(NotificationViewBaseTest, AppNameExtension) {
  std::u16string app_name = u"extension name";
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_context_message(app_name);

  UpdateNotificationViews(*notification);

  EXPECT_EQ(app_name, notification_view()->header_row_->app_name_for_testing());
}

TEST_F(NotificationViewBaseTest, AppNameSystemNotification) {
  MessageCenter::Get()->RemoveAllNotifications(/*by_user=*/false,
                                               MessageCenter::RemoveType::ALL);

  std::u16string app_name = u"system notification";
  MessageCenter::Get()->SetSystemNotificationAppName(app_name);
  RichNotificationData data;
  data.settings_button_handler = SettingsButtonHandler::INLINE;
  auto notification = std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, std::string(kDefaultNotificationId), u"title",
      u"message", ui::ImageModel(), std::u16string(), GURL(),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      NotifierId(NotifierType::SYSTEM_COMPONENT, "system",
                 ash::NotificationCatalogName::kTestCatalogName),
#else
      NotifierId(NotifierType::SYSTEM_COMPONENT, "system"),
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      data, nullptr);

  UpdateNotificationViews(*notification);

  EXPECT_EQ(app_name, notification_view()->header_row_->app_name_for_testing());
}

TEST_F(NotificationViewBaseTest, AppNameWebNotification) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_origin_url(GURL("http://example.com"));

  UpdateNotificationViews(*notification);

  EXPECT_EQ(u"example.com",
            notification_view()->header_row_->app_name_for_testing());
}

TEST_F(NotificationViewBaseTest, AppNameWebAppNotification) {
  MessageCenter::Get()->RemoveAllNotifications(/*by_user=*/false,
                                               MessageCenter::RemoveType::ALL);

  const GURL web_app_url("http://example.com");

  NotifierId notifier_id(web_app_url, /*title=*/u"web app title",
                         /*web_app_id=*/std::nullopt);

  SkBitmap small_bitmap = gfx::test::CreateBitmap(/*size=*/16, SK_ColorYELLOW);
  // Makes the center area transparent.
  small_bitmap.eraseArea(SkIRect::MakeXYWH(4, 4, 8, 8), SK_ColorTRANSPARENT);

  RichNotificationData data;
  data.settings_button_handler = SettingsButtonHandler::INLINE;

  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, std::string(kDefaultNotificationId), u"title",
      u"message",
      ui::ImageModel::FromImage(gfx::test::CreateImage(/*size=*/80)),
      u"display source", GURL(), notifier_id, data, delegate_);
  notification->SetSmallImage(gfx::Image::CreateFrom1xBitmap(small_bitmap));
  notification->SetImage(gfx::test::CreateImage(320, 240));

  notification->set_origin_url(web_app_url);

  UpdateNotificationViews(*notification);

  EXPECT_EQ(u"web app title",
            notification_view()->header_row_->app_name_for_testing());
}

TEST_F(NotificationViewBaseTest, ShowProgress) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_type(NOTIFICATION_TYPE_PROGRESS);
  notification->set_progress(50);
  UpdateNotificationViews(*notification);

  EXPECT_TRUE(notification_view()
                  ->header_row_->summary_text_for_testing()
                  ->GetVisible());
}

TEST_F(NotificationViewBaseTest, ShowTimestamp) {
  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_timestamp(base::Time::Now());
  UpdateNotificationViews(*notification);

  EXPECT_TRUE(notification_view()
                  ->header_row_->timestamp_view_for_testing()
                  ->GetVisible());

  // Expect timestamp view to hide for progress notifications.
  notification->set_type(NOTIFICATION_TYPE_PROGRESS);
  notification->set_progress(50);
  UpdateNotificationViews(*notification);
  EXPECT_FALSE(notification_view()
                   ->header_row_->timestamp_view_for_testing()
                   ->GetVisible());
}

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
#define MAYBE_SlideOutWithMessageCenterView \
  DISABLED_SlideOutWithMessageCenterView
#else
#define MAYBE_SlideOutWithMessageCenterView SlideOutWithMessageCenterView
#endif
// Tests slide out behavior when the `MessageCenterView` exists. The
// notification's popup should be dismissed but the notification should not be
// removed.
TEST_F(NotificationViewBaseTest, MAYBE_SlideOutWithMessageCenterView) {
  SetHasMessageCenterView(/*has_message_center_view=*/true);

  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  EXPECT_FALSE(IsPopupRemovedAfterIdle(kDefaultNotificationId));

  BeginScroll();
  ScrollBy(-10);
  EXPECT_FALSE(IsPopupRemovedAfterIdle(kDefaultNotificationId));
  EXPECT_EQ(-10.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_FALSE(IsPopupRemovedAfterIdle(kDefaultNotificationId));
  EXPECT_EQ(0.f, GetNotificationSlideAmount());

  BeginScroll();
  ScrollBy(-200);
  EXPECT_FALSE(IsPopupRemovedAfterIdle(kDefaultNotificationId));
  EXPECT_EQ(-200.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_TRUE(IsPopupRemovedAfterIdle(kDefaultNotificationId));
  EXPECT_FALSE(IsRemovedAfterIdle(kDefaultNotificationId));
}

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
#define MAYBE_SlideOutByTrackpad DISABLED_SlideOutByTrackpad
#else
#define MAYBE_SlideOutByTrackpad SlideOutByTrackpad
#endif
TEST_F(NotificationViewBaseTest, MAYBE_SlideOutByTrackpad) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  ui::test::EventGenerator generator(
      GetRootWindow(notification_view()->GetWidget()));
  generator.ScrollSequence(
      gfx::Point(), base::TimeDelta(),
      /*x_offset=*/notification_view()->bounds().width() + 1,
      /*y_offset=*/0, /*steps=*/1, /*num_fingers=*/2);
  EXPECT_TRUE(IsPopupRemovedAfterIdle(kDefaultNotificationId));
}

}  // namespace message_center
