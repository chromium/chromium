// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_popup_collection.h"

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/ax_node.h"
#include "ui/display/display.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/views/desktop_message_popup_collection.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_base.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace message_center {

namespace {

class MockMessagePopupView;

class MockMessagePopupCollection : public DesktopMessagePopupCollection {
 public:
  explicit MockMessagePopupCollection(gfx::NativeWindow context)
      : context_(context) {}

  MockMessagePopupCollection(const MockMessagePopupCollection&) = delete;
  MockMessagePopupCollection& operator=(const MockMessagePopupCollection&) =
      delete;

  ~MockMessagePopupCollection() override = default;

  void SetAnimationValue(double current) {
    animation()->SetCurrentValue(current);
    if (current == 1.0)
      animation()->End();
    else
      AnimationProgressed(animation());
  }

  void RemovePopup(MockMessagePopupView* popup) { std::erase(popups_, popup); }

  bool IsAnimating() { return animation()->is_animating(); }

  void set_is_primary_display(bool is_primary_display) {
    is_primary_display_ = is_primary_display;
  }

  void set_is_fullscreen(bool is_fullscreen) { is_fullscreen_ = is_fullscreen; }

  void set_new_popup_height(int new_popup_height) {
    new_popup_height_ = new_popup_height;
  }

  std::vector<raw_ptr<MockMessagePopupView, VectorExperimental>>& popups() {
    return popups_;
  }

  bool popup_timer_started() const { return popup_timer_started_; }

  int popup_collection_height_changed() const {
    return popup_collection_height_changed_;
  }

  int notify_silent_notification_count() const {
    return notify_silent_notification_count_;
  }

 protected:
  MessagePopupView* CreatePopup(const Notification& notification) override;

  void ConfigureWidgetInitParamsForContainer(
      views::Widget* widget,
      views::Widget::InitParams* init_params) override {
    // Provides an aura window context for widget creation.
    init_params->context = context_;
  }

  void RestartPopupTimers() override {
    MessagePopupCollection::RestartPopupTimers();
    popup_timer_started_ = true;
  }

  void PausePopupTimers() override {
    MessagePopupCollection::PausePopupTimers();
    popup_timer_started_ = false;
  }

  bool IsPrimaryDisplayForNotification() const override {
    return is_primary_display_;
  }

  bool BlockForMixedFullscreen(
      const Notification& notification) const override {
    return is_fullscreen_;
  }

  void NotifyPopupCollectionHeightChanged() override {
    ++popup_collection_height_changed_;
  }

  void NotifySilentNotification(const std::string& notification_id) override {
    ++notify_silent_notification_count_;
  }

 private:
  gfx::NativeWindow context_;

  std::vector<raw_ptr<MockMessagePopupView, VectorExperimental>> popups_;

  bool popup_timer_started_ = false;
  bool is_primary_display_ = true;
  bool is_fullscreen_ = false;
  int new_popup_height_ = 84;
  int popup_collection_height_changed_ = 0;
  int notify_silent_notification_count_ = 0;
};

class MockMessagePopupView : public MessagePopupView {
 public:
  MockMessagePopupView(const std::string& id,
                       int init_height,
                       MockMessagePopupCollection* popup_collection)
      : MessagePopupView(popup_collection),
        popup_collection_(popup_collection),
        id_(id),
        title_(base::UTF16ToUTF8(
            MessageCenter::Get()->FindVisibleNotificationById(id)->title())) {
    auto* view = new views::View;
    view->SetPreferredSize(gfx::Size(GetNotificationWidth(), init_height));
    AddChildView(view);
  }

  ~MockMessagePopupView() override = default;

  void Close() override {
    popup_collection_->RemovePopup(this);
    MessagePopupView::Close();
  }

  void UpdateContents(const Notification& notification) override {
    if (height_after_update_.has_value())
      SetPreferredHeight(height_after_update_.value());
    popup_collection_->NotifyPopupResized();
    updated_ = true;
    title_ = base::UTF16ToUTF8(notification.title());
  }

  void UpdateContentsForChildNotification(
      const std::string& notification_id,
      const Notification& notification) override {
    child_updated_ = true;
    child_updated_notification_id_ = notification_id;
    child_updated_title_ = base::UTF16ToUTF8(notification.title());
  }

  void AutoCollapse() override {
    if (expandable_)
      children().front()->SetPreferredSize(
          gfx::Size(GetNotificationWidth(), 42));
  }

  void SetPreferredHeight(int height) {
    children().front()->SetPreferredSize(
        gfx::Size(GetNotificationWidth(), height));
  }

  void SetHovered(bool is_hovered) {
    if (is_hovered) {
      ui::MouseEvent enter_event(ui::EventType::kMouseEntered, gfx::Point(),
                                 gfx::Point(), ui::EventTimeForNow(), 0, 0);
      OnMouseEntered(enter_event);
    } else {
      ui::MouseEvent exit_event(ui::EventType::kMouseExited, gfx::Point(),
                                gfx::Point(), ui::EventTimeForNow(), 0, 0);
      OnMouseExited(exit_event);
    }
  }

  void SimulateFocused() { OnDidChangeFocus(nullptr, children().front()); }

  void Activate() {
    SetCanActivate(true);
    GetWidget()->Activate();
  }

  const std::string& id() const { return id_; }
  bool updated() const { return updated_; }

  bool child_updated() const { return child_updated_; }
  const std::string& child_updated_notification_id() {
    return child_updated_notification_id_;
  }
  const std::string& child_updated_title() { return child_updated_title_; }

  const std::string& title() const { return title_; }

  void set_expandable(bool expandable) { expandable_ = expandable; }

  void set_height_after_update(std::optional<int> height_after_update) {
    height_after_update_ = height_after_update;
  }

 private:
  const raw_ptr<MockMessagePopupCollection> popup_collection_;

  std::string id_;
  bool updated_ = false;

  bool child_updated_ = false;
  std::string child_updated_notification_id_;
  std::string child_updated_title_;

  bool expandable_ = false;
  std::string title_;

  std::optional<int> height_after_update_;
};

MessagePopupView* MockMessagePopupCollection::CreatePopup(
    const Notification& notification) {
  auto* popup =
      new MockMessagePopupView(notification.id(), new_popup_height_, this);
  popups_.push_back(popup);
  return popup;
}

}  // namespace

class MessagePopupCollectionTest : public views::ViewsTestBase,
                                   public MessageCenterObserver {
 public:
  MessagePopupCollectionTest() = default;

  MessagePopupCollectionTest(const MessagePopupCollectionTest&) = delete;
  MessagePopupCollectionTest& operator=(const MessagePopupCollectionTest&) =
      delete;

  ~MessagePopupCollectionTest() override = default;

  // views::ViewTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    MessageCenter::Initialize();
    MessageCenter::Get()->DisableTimersForTest();
    MessageCenter::Get()->AddObserver(this);

    popup_collection_ =
        std::make_unique<MockMessagePopupCollection>(GetContext());

    // This size fits test machines resolution and also can keep a few popups
    // w/o ill effects of hitting the screen overflow. This allows us to assume
    // and verify normal layout of the toast stack.
    SetDisplayInfo(gfx::Rect(0, 0, 1920, 1070),  // taskbar at the bottom.
                   gfx::Rect(0, 0, 1920, 1080));
  }

  void TearDown() override {
    MessageCenter::Get()->RemoveAllNotifications(
        false /* by_user */, MessageCenter::RemoveType::ALL);
    AnimateUntilIdle();
    popup_collection_.reset();

    MessageCenter::Get()->RemoveObserver(this);
    MessageCenter::Shutdown();
    views::ViewsTestBase::TearDown();
  }

  // MessageCenterObserver:
  void OnNotificationDisplayed(const std::string& notification_id,
                               const DisplaySource source) override {
    last_displayed_id_ = notification_id;
  }

 protected:
  std::unique_ptr<Notification> CreateNotification(const std::string& id) {
    return CreateNotification(id, "test title");
  }

  std::unique_ptr<Notification> CreateNotification(const std::string& id,
                                                   const std::string& title) {
    return std::make_unique<Notification>(
        NOTIFICATION_TYPE_SIMPLE, id, base::UTF8ToUTF16(title), u"test message",
        ui::ImageModel(), std::u16string() /* display_source */, GURL(),
        NotifierId(), RichNotificationData(), new NotificationDelegate());
  }

  std::unique_ptr<Notification> CreateLowPriorityNotification() {
    std::unique_ptr<Notification> notification =
        CreateNotification(base::NumberToString(id_++));
    notification->set_priority(LOW_PRIORITY);
    return notification;
  }

  std::string AddNotification() {
    std::string id = base::NumberToString(id_++);
    MessageCenter::Get()->AddNotification(CreateNotification(id));
    return id;
  }

  void AddNotification(std::unique_ptr<Notification> notification) {
    MessageCenter::Get()->AddNotification(std::move(notification));
  }

  void Update() { popup_collection_->Update(); }

  void SetAnimationValue(double current) {
    popup_collection_->SetAnimationValue(current);
  }

  bool IsAnimating() const { return popup_collection_->IsAnimating(); }

  void AnimateUntilIdle() {
    while (popup_collection_->IsAnimating()) {
      popup_collection_->SetAnimationValue(1.0);
    }
  }

  void AnimateToMiddle() {
    EXPECT_TRUE(popup_collection_->IsAnimating());
    popup_collection_->SetAnimationValue(0.5);
  }

  void AnimateToEnd() {
    EXPECT_TRUE(popup_collection_->IsAnimating());
    popup_collection_->SetAnimationValue(1.0);
  }

  MockMessagePopupView* GetPopup(const std::string& id) {
    for (MockMessagePopupView* popup : popup_collection_->popups()) {
      if (popup->id() == id)
        return popup;
    }
    return nullptr;
  }

  MockMessagePopupView* GetPopupAt(size_t index) {
    return popup_collection_->popups()[index];
  }

  void CloseAllPopupsNow() { popup_collection()->CloseAllPopupsNow(); }

  size_t GetPopupCounts() const { return popup_collection_->popups().size(); }

  void SetDisplayInfo(const gfx::Rect& work_area,
                      const gfx::Rect& display_bounds) {
    display::Display dummy_display;
    dummy_display.set_bounds(display_bounds);
    dummy_display.set_work_area(work_area);
    work_area_ = work_area;
    popup_collection_->RecomputeAlignment(dummy_display);
  }

  bool IsPopupTimerStarted() const {
    return popup_collection_->popup_timer_started();
  }

  MockMessagePopupCollection* popup_collection() const {
    return popup_collection_.get();
  }
  const gfx::Rect& work_area() const { return work_area_; }

  const std::string& last_displayed_id() const { return last_displayed_id_; }

 private:
  int id_ = 0;

  std::unique_ptr<MockMessagePopupCollection> popup_collection_;

  gfx::Rect work_area_;
  std::string last_displayed_id_;
};

TEST_F(MessagePopupCollectionTest, Nothing) {
  EXPECT_FALSE(IsAnimating());
  Update();
  // If no popups are available, nothing changes.
  EXPECT_FALSE(IsAnimating());
}

TEST_F(MessagePopupCollectionTest, FadeInFadeOutNotification) {
  // Add a notification.
  std::string id = AddNotification();
  EXPECT_TRUE(IsAnimating());
  EXPECT_EQ(1u, GetPopupCounts());

  // The popup will fade in from right.
  const int before_x = GetPopup(id)->GetBoundsInScreen().x();
  EXPECT_EQ(0.0f, GetPopup(id)->GetOpacity());

  AnimateToMiddle();
  EXPECT_GT(before_x, GetPopup(id)->GetBoundsInScreen().x());
  EXPECT_LT(0.0f, GetPopup(id)->GetOpacity());

  AnimateToEnd();
  EXPECT_EQ(1.0f, GetPopup(id)->GetOpacity());
  EXPECT_FALSE(IsAnimating());
  EXPECT_TRUE(work_area().Contains(GetPopup(id)->GetBoundsInScreen()));
  EXPECT_EQ(id, last_displayed_id());

  // The popup will fade out because of timeout.
  MessageCenter::Get()->MarkSinglePopupAsShown(id, false);
  EXPECT_TRUE(IsAnimating());

  AnimateToMiddle();
  EXPECT_GT(1.0f, GetPopup(id)->GetOpacity());

  AnimateToEnd();
  EXPECT_FALSE(IsAnimating());
  EXPECT_FALSE(GetPopup(id));
}

TEST_F(MessagePopupCollectionTest, FadeInMultipleNotifications) {
  std::vector<std::string> ids;
  for (size_t i = 0; i < kMaxVisiblePopupNotifications; ++i)
    ids.push_back(AddNotification());

  for (size_t i = 0; i < ids.size(); ++i) {
    EXPECT_EQ(ids[i], last_displayed_id());
    EXPECT_EQ(i + 1, GetPopupCounts());
    AnimateToMiddle();
    EXPECT_LT(0.0f, GetPopupAt(i)->GetOpacity());
    AnimateToEnd();
    EXPECT_EQ(1.0f, GetPopupAt(i)->GetOpacity());
    EXPECT_TRUE(work_area().Contains(GetPopupAt(i)->GetBoundsInScreen()));
  }
  EXPECT_FALSE(IsAnimating());

  EXPECT_EQ(kMaxVisiblePopupNotifications, GetPopupCounts());

  for (size_t i = 0; i < ids.size(); ++i)
    EXPECT_EQ(ids[i], GetPopupAt(i)->id());

  for (size_t i = 0; i < ids.size() - 1; ++i) {
    EXPECT_GT(GetPopupAt(i)->GetBoundsInScreen().x(),
              GetPopupAt(i + 1)->GetBoundsInScreen().bottom());
  }
}

TEST_F(MessagePopupCollectionTest, UpdateContents) {
  std::string id = AddNotification();
  AnimateToEnd();
  EXPECT_FALSE(IsAnimating());
  EXPECT_EQ(1u, GetPopupCounts());
  EXPECT_FALSE(GetPopup(id)->updated());

  auto updated_notification = CreateNotification(id);
  updated_notification->set_message(u"updated");
  MessageCenter::Get()->UpdateNotification(id, std::move(updated_notification));
  EXPECT_EQ(1u, GetPopupCounts());
  EXPECT_TRUE(GetPopup(id)->updated());
}

// TODO(crbug.com/40885754): Flaky on all platforms.
TEST_F(MessagePopupCollectionTest, DISABLED_UpdateContentsCausesPopupClose) {
  std::string id = AddNotification();
  AnimateToEnd();
  RunPendingMessages();
  EXPECT_FALSE(IsAnimating());
  EXPECT_EQ(1u, GetPopupCounts());
  EXPECT_FALSE(GetPopup(id)->updated());

  GetPopup(id)->set_height_after_update(2048);

  auto updated_notification = CreateNotification(id);
  updated_notification->set_message(u"updated");
  MessageCenter::Get()->UpdateNotification(id, std::move(updated_notification));
  RunPendingMessages();
  EXPECT_EQ(0u, GetPopupCounts());
}

TEST_F(MessagePopupCollectionTest, OnChildNotificationViewUpdated) {
  std::string parent_id = AddNotification();
  std::string child_id = AddNotification();

  const std::string new_notification_title("new_title");
  auto new_notification = CreateNotification(child_id, new_notification_title);
  MessageCenter::Get()->UpdateNotification(child_id,
                                           std::move(new_notification));

  // Calling `OnChildNotificationViewUpdated()` should update the child
  // notification in parent's popup.
  popup_collection()->OnChildNotificationViewUpdated(parent_id, child_id);

  EXPECT_TRUE(GetPopup(parent_id)->child_updated());
  EXPECT_EQ(child_id, GetPopup(parent_id)->child_updated_notification_id());
  EXPECT_EQ(new_notification_title, GetPopup(parent_id)->child_updated_title());
}

TEST_F(MessagePopupCollectionTest, MessageCenterVisibility) {
  // It only applies to a platform with MessageCenterView i.e. Chrome OS.
  MessageCenter::Get()->SetHasMessageCenterView(true);

  for (size_t i = 0; i < kMaxVisiblePopupNotifications; ++i)
    AddNotification();
  AnimateUntilIdle();

  EXPECT_EQ(kMaxVisiblePopupNotifications, GetPopupCounts());

  EXPECT_EQ(3u, GetPopupCounts());
  EXPECT_EQ(3u, MessageCenter::Get()->GetPopupNotifications().size());

  // The notification should be hidden when MessageCenterView is visible.
  MessageCenter::Get()->SetVisibility(Visibility::VISIBILITY_MESSAGE_CENTER);
  // It should not animate in order to show ARC++ notifications properly.
  EXPECT_FALSE(IsAnimating());

  MessageCenter::Get()->SetVisibility(Visibility::VISIBILITY_TRANSIENT);
  EXPECT_FALSE(IsAnimating());
  EXPECT_EQ(0u, GetPopupCounts());
  EXPECT_EQ(0u, MessageCenter::Get()->GetPopupNotifications().size());
}

TEST_F(MessagePopupCollectionTest, ShowCustomOnPrimaryDisplay) {
  // TODO(yoshiki): Support custom popup notification on multiple display
  // (crbug.com/715370).
  popup_collection()->set_is_primary_display(true);
  auto custom = CreateNotification("id");
  custom->set_type(NOTIFICATION_TYPE_CUSTOM);
  MessageCenter::Get()->AddNotification(std::move(custom));
  EXPECT_TRUE(IsAnimating());
  EXPECT_EQ(1u, GetPopupCounts());
}

TEST_F(MessagePopupCollectionTest, NotShowCustomOnSubDisplay) {
  // Disables popup of custom notification on non-primary displays, since
  // currently custom notification supports only on one display at the same
  // time.
  // TODO(yoshiki): Support custom popup notification on multiple display
  // (crbug.com/715370).

  popup_collection()->set_is_primary_display(false);
  auto custom = CreateNotification("id");
  custom->set_type(NOTIFICATION_TYPE_CUSTOM);
  MessageCenter::Get()->AddNotification(std::move(custom));
  EXPECT_FALSE(IsAnimating());
  EXPECT_EQ(0u, GetPopupCounts());
}

TEST_F(MessagePopupCollectionTest, MixedFullscreenShow) {
  popup_collection()->set_is_fullscreen(false);
  AddNotification();
  EXPECT_TRUE(IsAnimating());
  EXPECT_EQ(1u, GetPopupCounts());
}

TEST_F(MessagePopupCollectionTest, MixedFullscreenBlock) {
  popup_collection()->set_is_fullscreen(true);
  AddNotification();
  EXPECT_FALSE(IsAnimating());
  EXPECT_EQ(0u, GetPopupCounts());
}

TEST_F(MessagePopupCollectionTest, NotificationsMoveDown) {
  std::vector<std::string> ids;
  for (size_t i = 0; i < kMaxVisiblePopupNotifications + 1; ++i)
    ids.push_back(AddNotification());

  AnimateUntilIdle();

  EXPECT_EQ(kMaxVisiblePopupNotifications, GetPopupCounts());
  EXPECT_FALSE(IsAnimating());

  gfx::Rect dismissed = GetPopup(ids.front())->GetBoundsInScreen();

  MessageCenter::Get()->MarkSinglePopupAsShown(ids.front(), false);
  EXPECT_TRUE(IsAnimating());

  AnimateToMiddle();
  EXPECT_GT(1.0f, GetPopup(ids[0])->GetOpacity());
  EXPECT_EQ(ids[0], GetPopup(ids[0])->id());

  AnimateToEnd();
  EXPECT_EQ(ids[1], GetPopup(ids[1])->id());
  EXPECT_TRUE(IsAnimating());

  gfx::Rect before = GetPopup(ids[1])->GetBoundsInScreen();

  AnimateToMiddle();
  gfx::Rect moving = GetPopup(ids[1])->GetBoundsInScreen();
  EXPECT_GT(moving.bottom(), before.bottom());
  EXPECT_GT(dismissed.bottom(), moving.bottom());

  AnimateToEnd();
  gfx::Rect after = GetPopup(ids[1])->GetBoundsInScreen();
  EXPECT_EQ(dismissed, after);
  EXPECT_EQ(kMaxVisiblePopupNotifications, GetPopupCounts());
  EXPECT_TRUE(IsAnimating());

  EXPECT_EQ(0.f, GetPopup(ids.back())->GetOpacity());

  AnimateToMiddle();
  EXPECT_LT(0.0f, GetPopup(ids.back())->GetOpacity());

  AnimateToEnd();
  EXPECT_EQ(1.0f, GetPopup(ids.back())->GetOpacity());
  EXPECT_FALSE(IsAnimating());
}

TEST_F(MessagePopupCollectionTest, PopupResized) {
  std::vector<std::string> ids;
  for (size_t i = 0; i < kMaxVisiblePopupNotifications; ++i)
    ids.push_back(AddNotification());
  AnimateUntilIdle();

  std::vector<gfx::Rect> previous_bounds;
  for (const auto& id : ids)
    previous_bounds.push_back(GetPopup(id)->GetBoundsInScreen());

  const int changed_height = 256;
  GetPopup(ids[1])->SetPreferredHeight(changed_height);

  EXPECT_TRUE(IsAnimating());

  AnimateToMiddle();
  EXPECT_EQ(previous_bounds[0], GetPopup(ids[0])->GetBoundsInScreen());
  EXPECT_EQ(previous_bounds[1].bottom(),
            GetPopup(ids[1])->GetBoundsInScreen().bottom());
  EXPECT_GT(previous_bounds[1].y(), GetPopup(ids[1])->GetBoundsInScreen().y());
  EXPECT_GT(previous_bounds[2].bottom(),
            GetPopup(ids[2])->GetBoundsInScreen().bottom());
  EXPECT_GT(previous_bounds[2].y(), GetPopup(ids[2])->GetBoundsInScreen().y());

  AnimateToEnd();
  EXPECT_FALSE(IsAnimating());
  EXPECT_EQ(previous_bounds[0], GetPopup(ids[0])->GetBoundsInScreen());
  EXPECT_EQ(changed_height, GetPopup(ids[1])->GetBoundsInScreen().height());
}

TEST_F(MessagePopupCollectionTest, ExpandLatest) {
  std::string id = AddNotification();
  AnimateToEnd();
  GetPopup(id)->set_expandable(true);
  const int top_y = GetPopup(id)->GetBoundsInScreen().y();

  AddNotification();
  EXPECT_TRUE(IsAnimating());
  EXPECT_EQ(1u, GetPopupCounts());
  AnimateToMiddle();
  EXPECT_LT(top_y, GetPopup(id)->GetBoundsInScreen().y());
  AnimateToEnd();
  EXPECT_LT(top_y, GetPopup(id)->GetBoundsInScreen().y());

  EXPECT_TRUE(IsAnimating());
  EXPECT_EQ(2u, GetPopupCounts());
  AnimateToEnd();
  EXPECT_FALSE(IsAnimating());
}

TEST_F(MessagePopupCollectionTest, ExpandLatestWithMoveDown) {
  std::vector<std::string> ids;
  for (size_t i = 0; i < kMaxVisiblePopupNotifications + 1; ++i)
    ids.push_back(AddNotification());

  AnimateUntilIdle();
  EXPECT_EQ(kMaxVisiblePopupNotifications, GetPopupCounts());

  GetPopup(ids[1])->set_expandable(true);

  const int top_y = GetPopup(ids[1])->GetBoundsInScreen().y();

  MessageCenter::Get()->MarkSinglePopupAsShown(ids.front(), false);
  AnimateToEnd();
  EXPECT_TRUE(IsAnimating());
  EXPECT_EQ(kMaxVisiblePopupNotifications - 1, GetPopupCounts());

  AnimateToMiddle();
  EXPECT_LT(top_y, GetPopup(ids[2])->GetBoundsInScreen().y());
  AnimateToEnd();
  EXPECT_EQ(kMaxVisiblePopupNotifications, GetPopupCounts());
  EXPECT_TRUE(IsAnimating());
  AnimateToEnd();
  EXPECT_FALSE(IsAnimating());
}

TEST_F(MessagePopupCollectionTest, HoverClose) {
  std::string id0 = AddNotification();
  AnimateToEnd();
  popup_collection()->set_new_popup_height(256);
  std::string id1 = AddNotification();
  AnimateToEnd();
  popup_collection()->set_new_popup_height(84);
  std::string id2 = AddNotification();
  AnimateToEnd();
  EXPECT_FALSE(IsAnimating());

  EXPECT_TRUE(IsPopupTimerStarted());
  GetPopup(id0)->SetHovered(true);
  EXPECT_FALSE(IsPopupTimerStarted());

  const int first_popup_bottom = GetPopup(id0)->GetBoundsInScreen().bottom();

  MessageCenter::Get()->RemoveNotification(id0, true);
  EXPECT_TRUE(IsAnimating());
  AnimateToEnd();
  EXPECT_TRUE(IsAnimating());
  AnimateToMiddle();
  GetPopup(id1)->SetHovered(true);
  AnimateToEnd();
  EXPECT_FALSE(IsAnimating());
  EXPECT_EQ(first_popup_bottom, GetPopup(id1)->GetBoundsInScreen().bottom());

  EXPECT_FALSE(IsPopupTimerStarted());
  GetPopup(id1)->SetHovered(false);
  EXPECT_TRUE(IsPopupTimerStarted());
  EXPECT_EQ(first_popup_bottom, GetPopup(id1)->GetBoundsInScreen().bottom());
}

// Popup timers should be paused if a notification has focus.
// Once the focus is lost or the notification is resumed, popup timers
// should restart.
TEST_F(MessagePopupCollectionTest, FocusedClose) {
  std::string id0 = AddNotification();
  AnimateToEnd();
  popup_collection()->set_new_popup_height(256);
  std::string id1 = AddNotification();
  AnimateToEnd();
  popup_collection()->set_new_popup_height(84);
  std::string id2 = AddNotification();
  AnimateToEnd();
  EXPECT_FALSE(IsAnimating());

  EXPECT_TRUE(IsPopupTimerStarted());
  GetPopup(id0)->Activate();
  // Activating a popup should not pause timers.
  EXPECT_TRUE(IsPopupTimerStarted());
  // If the popup gets keyboard focus the timers should pause.
  GetPopup(id0)->SimulateFocused();
  EXPECT_FALSE(IsPopupTimerStarted());

  const int first_popup_top = GetPopup(id0)->GetBoundsInScreen().y();

  MessageCenter::Get()->RemoveNotification(id0, true);
  AnimateToEnd();
  AnimateToEnd();
  EXPECT_FALSE(IsAnimating());
  EXPECT_GT(first_popup_top, GetPopup(id1)->GetBoundsInScreen().y());
  EXPECT_TRUE(IsPopupTimerStarted());
}

TEST_F(MessagePopupCollectionTest, SlideOutClose) {
  std::vector<std::string> ids;
  for (size_t i = 0; i < kMaxVisiblePopupNotifications; ++i)
    ids.push_back(AddNotification());

  AnimateUntilIdle();

  GetPopup(ids[1])->SetOpacity(0);
  MessageCenter::Get()->RemoveNotification(ids[1], true);
  AnimateToEnd();

  EXPECT_FALSE(IsAnimating());
  EXPECT_TRUE(IsPopupTimerStarted());
}

TEST_F(MessagePopupCollectionTest, TooTallNotification) {
  SetDisplayInfo(gfx::Rect(0, 0, 800, 470),  // taskbar at the bottom.
                 gfx::Rect(0, 0, 800, 480));

  std::string id0 = AddNotification();
  std::string id1 = AddNotification();

  AnimateUntilIdle();

  EXPECT_EQ(2u, GetPopupCounts());

  popup_collection()->set_new_popup_height(400);
  std::string id2 = AddNotification();

  EXPECT_FALSE(IsAnimating());
  EXPECT_EQ(2u, GetPopupCounts());
  EXPECT_TRUE(GetPopup(id0));
  EXPECT_TRUE(GetPopup(id1));
  EXPECT_FALSE(GetPopup(id2));

  MessageCenter::Get()->MarkSinglePopupAsShown(id0, false);
  AnimateUntilIdle();
  EXPECT_EQ(1u, GetPopupCounts());
  EXPECT_FALSE(GetPopup(id2));

  MessageCenter::Get()->MarkSinglePopupAsShown(id1, false);
  AnimateUntilIdle();
  EXPECT_EQ(1u, GetPopupCounts());
  EXPECT_TRUE(GetPopup(id2));
}

TEST_F(MessagePopupCollectionTest, DisplaySizeChanged) {
  std::string id0 = AddNotification();
  AnimateToEnd();
  std::string id1 = AddNotification();
  AnimateToEnd();
  popup_collection()->set_new_popup_height(400);
  std::string id2 = AddNotification();
  AnimateToEnd();
  EXPECT_FALSE(IsAnimating());

  EXPECT_TRUE(GetPopup(id0));
  EXPECT_TRUE(GetPopup(id1));
  EXPECT_TRUE(GetPopup(id2));

  SetDisplayInfo(gfx::Rect(0, 0, 800, 470),  // taskbar at the bottom.
                 gfx::Rect(0, 0, 800, 480));
  popup_collection()->ResetBounds();

  EXPECT_TRUE(GetPopup(id0));
  EXPECT_TRUE(work_area().Contains(GetPopup(id0)->GetBoundsInScreen()));
  EXPECT_TRUE(GetPopup(id1));
  EXPECT_TRUE(work_area().Contains(GetPopup(id1)->GetBoundsInScreen()));
  EXPECT_FALSE(GetPopup(id2));

  MessageCenter::Get()->MarkSinglePopupAsShown(id0, false);
  MessageCenter::Get()->MarkSinglePopupAsShown(id1, false);
  AnimateUntilIdle();
  EXPECT_EQ(1u, GetPopupCounts());
  EXPECT_TRUE(GetPopup(id2));
}

TEST_F(MessagePopupCollectionTest, PopupResizedAndOverflown) {
  SetDisplayInfo(gfx::Rect(0, 0, 800, 470),  // taskbar at the bottom.
                 gfx::Rect(0, 0, 800, 480));

  std::string id0 = AddNotification();
  std::string id1 = AddNotification();
  std::string id2 = AddNotification();
  AnimateUntilIdle();

  EXPECT_TRUE(GetPopup(id0));
  EXPECT_TRUE(GetPopup(id1));
  EXPECT_TRUE(GetPopup(id2));

  const int changed_height = 300;
  GetPopup(id1)->SetPreferredHeight(changed_height);

  AnimateUntilIdle();
  RunPendingMessages();

  EXPECT_TRUE(GetPopup(id0));
  EXPECT_TRUE(work_area().Contains(GetPopup(id0)->GetBoundsInScreen()));
  EXPECT_TRUE(GetPopup(id1));
  EXPECT_TRUE(work_area().Contains(GetPopup(id1)->GetBoundsInScreen()));
  EXPECT_FALSE(GetPopup(id2));

  MessageCenter::Get()->MarkSinglePopupAsShown(id0, false);
  AnimateUntilIdle();
  EXPECT_EQ(2u, GetPopupCounts());
  EXPECT_TRUE(GetPopup(id2));
}

TEST_F(MessagePopupCollectionTest, DismissOnClick) {
  MessageCenter::Get()->SetHasMessageCenterView(true);

  std::string id1 = AddNotification();
  std::string id2 = AddNotification();

  AnimateUntilIdle();

  EXPECT_EQ(2u, GetPopupCounts());
  EXPECT_TRUE(GetPopup(id1));
  EXPECT_TRUE(GetPopup(id2));

  MessageCenter::Get()->ClickOnNotification(id2);
  AnimateUntilIdle();

  EXPECT_EQ(1u, GetPopupCounts());
  EXPECT_TRUE(GetPopup(id1));
  EXPECT_FALSE(GetPopup(id2));

  MessageCenter::Get()->ClickOnNotificationButton(id1, 0);
  AnimateUntilIdle();

  EXPECT_EQ(0u, GetPopupCounts());
  EXPECT_FALSE(GetPopup(id1));
  EXPECT_FALSE(GetPopup(id2));
}

TEST_F(MessagePopupCollectionTest, NotDismissedOnClick) {
  MessageCenter::Get()->SetHasMessageCenterView(false);

  std::string id1 = AddNotification();
  std::string id2 = AddNotification();

  AnimateUntilIdle();

  EXPECT_EQ(2u, GetPopupCounts());
  EXPECT_TRUE(GetPopup(id1));
  EXPECT_TRUE(GetPopup(id2));

  MessageCenter::Get()->ClickOnNotification(id2);
  AnimateUntilIdle();

  EXPECT_EQ(2u, GetPopupCounts());
  EXPECT_TRUE(GetPopup(id1));
  EXPECT_TRUE(GetPopup(id2));

  MessageCenter::Get()->ClickOnNotificationButton(id1, 0);
  AnimateUntilIdle();

  EXPECT_EQ(2u, GetPopupCounts());
  EXPECT_TRUE(GetPopup(id1));
  EXPECT_TRUE(GetPopup(id2));
}

TEST_F(MessagePopupCollectionTest, PopupCollectionBounds) {
  EXPECT_EQ(gfx::Rect(), popup_collection()->popup_collection_bounds());

  std::string id0 = AddNotification();
  AnimateUntilIdle();

  gfx::Rect r0 = GetPopup(id0)->GetBoundsInScreen();

  // The popup collection bounds should be the bounds of the only popup.
  EXPECT_EQ(r0, popup_collection()->popup_collection_bounds());

  std::string id1 = AddNotification();
  std::string id2 = AddNotification();

  AnimateUntilIdle();

  r0 = GetPopup(id0)->GetBoundsInScreen();
  gfx::Rect r1 = GetPopup(id1)->GetBoundsInScreen();
  gfx::Rect r2 = GetPopup(id2)->GetBoundsInScreen();

  // The height of the entire popup collection bounds should be the total
  // heights of all popups, plus all the margins between them.
  int expected_height = r0.height() + kMarginBetweenPopups + r1.height() +
                        kMarginBetweenPopups + r2.height();

  EXPECT_EQ(gfx::Rect(r2.x(), r2.y(), GetNotificationWidth(), expected_height),
            popup_collection()->popup_collection_bounds());

  MessageCenter::Get()->RemoveNotification(id0, true);
  AnimateUntilIdle();

  r1 = GetPopup(id1)->GetBoundsInScreen();
  r2 = GetPopup(id2)->GetBoundsInScreen();

  EXPECT_EQ(gfx::Rect(r2.x(), r2.y(), GetNotificationWidth(),
                      r1.height() + kMarginBetweenPopups + r2.height()),
            popup_collection()->popup_collection_bounds());
}

TEST_F(MessagePopupCollectionTest, PopupCollectionHeightChanged) {
  EXPECT_EQ(0, popup_collection()->popup_collection_height_changed());

  std::string id0 = AddNotification();
  AnimateUntilIdle();

  EXPECT_EQ(1, popup_collection()->popup_collection_height_changed());

  std::string id1 = AddNotification();
  AnimateUntilIdle();

  EXPECT_EQ(2, popup_collection()->popup_collection_height_changed());

  std::string id2 = AddNotification();
  AnimateUntilIdle();

  EXPECT_EQ(3, popup_collection()->popup_collection_height_changed());

  MessageCenter::Get()->RemoveNotification(id0, true);
  AnimateUntilIdle();

  EXPECT_EQ(4, popup_collection()->popup_collection_height_changed());
}

// Tests that `MessagePopupCollection` notifies when there is an incoming silent
// notification.
TEST_F(MessagePopupCollectionTest, NotifySilentNotification) {
  ASSERT_EQ(0, popup_collection()->notify_silent_notification_count());

  // Add a silent notification.
  AddNotification(CreateLowPriorityNotification());

  // Assert that clients are notified of the incoming silent notification.
  EXPECT_EQ(1, popup_collection()->notify_silent_notification_count());

  // Add a non-silent notification.
  AddNotification();

  // Assert that clients are not notified for the incoming non-silent
  // notification.
  EXPECT_EQ(1, popup_collection()->notify_silent_notification_count());
}

TEST_F(MessagePopupCollectionTest, DefaultPositioning) {
  std::string id0 = AddNotification();
  std::string id1 = AddNotification();
  std::string id2 = AddNotification();
  std::string id3 = AddNotification();

  AnimateUntilIdle();

  gfx::Rect r0 = GetPopup(id0)->GetBoundsInScreen();
  gfx::Rect r1 = GetPopup(id1)->GetBoundsInScreen();
  gfx::Rect r2 = GetPopup(id2)->GetBoundsInScreen();

  // The 4th toast is not shown yet.
  EXPECT_FALSE(GetPopup(id3));

  // 3 toasts are shown, equal size, vertical stack.
  EXPECT_EQ(r0.width(), r1.width());
  EXPECT_EQ(r1.width(), r2.width());

  EXPECT_EQ(r0.height(), r1.height());
  EXPECT_EQ(r1.height(), r2.height());

  EXPECT_GT(r0.y(), r1.y());
  EXPECT_GT(r1.y(), r2.y());

  EXPECT_EQ(r0.x(), r1.x());
  EXPECT_EQ(r1.x(), r2.x());
}

TEST_F(MessagePopupCollectionTest, DefaultPositioningWithRightTaskbar) {
  // If taskbar is on the right we show the toasts bottom to top as usual.

  // Simulate a taskbar at the right.
  SetDisplayInfo(gfx::Rect(0, 0, 590, 400),   // Work-area.
                 gfx::Rect(0, 0, 600, 400));  // Display-bounds.
  std::string id0 = AddNotification();
  std::string id1 = AddNotification();

  AnimateUntilIdle();

  gfx::Rect r0 = GetPopup(id0)->GetBoundsInScreen();
  gfx::Rect r1 = GetPopup(id1)->GetBoundsInScreen();

  // 2 toasts are shown, equal size, vertical stack.
  EXPECT_EQ(r0.width(), r1.width());
  EXPECT_EQ(r0.height(), r1.height());
  EXPECT_GT(r0.y(), r1.y());
  EXPECT_EQ(r0.x(), r1.x());
}

TEST_F(MessagePopupCollectionTest, TopDownPositioningWithTopTaskbar) {
  // Simulate a taskbar at the top.
  SetDisplayInfo(gfx::Rect(0, 10, 600, 390),  // Work-area.
                 gfx::Rect(0, 0, 600, 400));  // Display-bounds.
  std::string id0 = AddNotification();
  std::string id1 = AddNotification();

  AnimateUntilIdle();

  gfx::Rect r0 = GetPopup(id0)->GetBoundsInScreen();
  gfx::Rect r1 = GetPopup(id1)->GetBoundsInScreen();

  // 2 toasts are shown, equal size, vertical stack.
  EXPECT_EQ(r0.width(), r1.width());
  EXPECT_EQ(r0.height(), r1.height());
  EXPECT_LT(r0.y(), r1.y());
  EXPECT_EQ(r0.x(), r1.x());
}

TEST_F(MessagePopupCollectionTest, TopDownPositioningWithLeftAndTopTaskbar) {
  // If there "seems" to be a taskbar on left and top (like in Unity), it is
  // assumed that the actual taskbar is the top one.

  // Simulate a taskbar at the top and left.
  SetDisplayInfo(gfx::Rect(10, 10, 590, 390),  // Work-area.
                 gfx::Rect(0, 0, 600, 400));   // Display-bounds.
  std::string id0 = AddNotification();
  std::string id1 = AddNotification();

  AnimateUntilIdle();

  gfx::Rect r0 = GetPopup(id0)->GetBoundsInScreen();
  gfx::Rect r1 = GetPopup(id1)->GetBoundsInScreen();

  // 2 toasts are shown, equal size, vertical stack.
  EXPECT_EQ(r0.width(), r1.width());
  EXPECT_EQ(r0.height(), r1.height());
  EXPECT_LT(r0.y(), r1.y());
  EXPECT_EQ(r0.x(), r1.x());
}

TEST_F(MessagePopupCollectionTest, TopDownPositioningWithBottomAndTopTaskbar) {
  // If there "seems" to be a taskbar on bottom and top (like in Gnome), it is
  // assumed that the actual taskbar is the top one.

  // Simulate a taskbar at the top and bottom.
  SetDisplayInfo(gfx::Rect(0, 10, 580, 400),  // Work-area.
                 gfx::Rect(0, 0, 600, 400));  // Display-bounds.
  std::string id0 = AddNotification();
  std::string id1 = AddNotification();

  AnimateUntilIdle();

  gfx::Rect r0 = GetPopup(id0)->GetBoundsInScreen();
  gfx::Rect r1 = GetPopup(id1)->GetBoundsInScreen();

  // 2 toasts are shown, equal size, vertical stack.
  EXPECT_EQ(r0.width(), r1.width());
  EXPECT_EQ(r0.height(), r1.height());
  EXPECT_LT(r0.y(), r1.y());
  EXPECT_EQ(r0.x(), r1.x());
}

TEST_F(MessagePopupCollectionTest, LeftPositioningWithLeftTaskbar) {
  // Simulate a taskbar at the left.
  SetDisplayInfo(gfx::Rect(10, 0, 590, 400),  // Work-area.
                 gfx::Rect(0, 0, 600, 400));  // Display-bounds.
  std::string id0 = AddNotification();
  std::string id1 = AddNotification();

  AnimateUntilIdle();

  gfx::Rect r0 = GetPopup(id0)->GetBoundsInScreen();
  gfx::Rect r1 = GetPopup(id1)->GetBoundsInScreen();

  EXPECT_EQ(r0.width(), r1.width());
  EXPECT_EQ(r0.height(), r1.height());
  EXPECT_GT(r0.y(), r1.y());
  EXPECT_EQ(r0.x(), r1.x());

  // Ensure that toasts are on the left.
  EXPECT_LT(r1.x(), work_area().CenterPoint().x());
  EXPECT_TRUE(work_area().Contains(r0));
  EXPECT_TRUE(work_area().Contains(r1));
}

TEST_F(MessagePopupCollectionTest, PopupWidgetClosedOutsideDuringFadeOut) {
  std::string id = AddNotification();
  AnimateUntilIdle();

  MessageCenter::Get()->MarkSinglePopupAsShown(id, false);
  AnimateToMiddle();

  // On Windows it might be possible that the widget is closed outside
  // MessagePopupCollection?  https://crbug.com/871199
  GetPopup(id)->GetWidget()->CloseNow();
  AnimateToEnd();

  EXPECT_FALSE(IsAnimating());
}

TEST_F(MessagePopupCollectionTest, NotifyPopupClosedThenCloseAllPopups) {
  std::string id1 = AddNotification();
  std::string id2 = AddNotification();
  AnimateUntilIdle();

  // This test make sure that when `NotifyPopupClosed()` is called and then
  // `CloseAllPopupsNow()` is triggered, no crash would happen. This scenerio
  // can happen when `MessagePopupView::~MessagePopupView()` is called, and then
  // at the same time another entity (i.e.
  // AshMessagePopupCollection::NotifierCollisionHandler) calls
  // `CloseAllPopupsNow()` (b/312515706).
  popup_collection()->NotifyPopupClosed(GetPopup(id1));
  CloseAllPopupsNow();
}

// Notification removing may occur while the animation triggered by the previous
// operation is running. As result, notification is removed from the message
// center but its popup is still kept. At this moment, a new notification with
// the same notification id may be added to the message center. This can happen
// on Chrome OS when an external display is connected with the Chromebook device
// (see https://crbug.com/921402). This test case emulates the procedure of
// the external display connection that is mentioned in the link above. Verifies
// that under this circumstance the notification popup is updated.
TEST_F(MessagePopupCollectionTest, RemoveNotificationWhileAnimating) {
  const std::string notification_id("test_id");
  const std::string old_notification_title("old_title");
  const std::string new_notification_title("new_title");

  // Create a notification and add it to message center.
  auto old_notification =
      CreateNotification(notification_id, old_notification_title);
  MessageCenter::Get()->AddNotification(std::move(old_notification));
  AnimateToMiddle();

  // On real device, MessageCenter::RemoveNotification is called before the
  // animation ends. As result, notification is removed while popup keeps still.
  EXPECT_TRUE(IsAnimating());
  MessageCenter::Get()->RemoveNotification(notification_id, false);
  EXPECT_FALSE(MessageCenter::Get()->HasPopupNotifications());
  EXPECT_EQ(1u, GetPopupCounts());
  EXPECT_EQ(old_notification_title, GetPopup(notification_id)->title());

  // On real device, the new notification with the same notification id is
  // created and added to message center before the animation ends.
  auto new_notification =
      CreateNotification(notification_id, new_notification_title);
  EXPECT_TRUE(IsAnimating());
  MessageCenter::Get()->AddNotification(std::move(new_notification));
  AnimateUntilIdle();

  // Verifies that the new notification popup is shown.
  EXPECT_EQ(1u, GetPopupCounts());
  EXPECT_EQ(new_notification_title, GetPopup(notification_id)->title());
}

TEST_F(MessagePopupCollectionTest, AccessibileAttributes) {
  // Add a notification.
  std::string id = AddNotification();

  ui::AXNodeData data;
  GetPopup(id)->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kAlertDialog);
}

}  // namespace message_center
