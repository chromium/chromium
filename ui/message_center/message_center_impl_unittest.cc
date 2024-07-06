// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/lock_screen/fake_lock_screen_controller.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using base::UTF8ToUTF16;

namespace message_center {

namespace {

class CheckObserver : public MessageCenterObserver {
 public:
  CheckObserver(MessageCenter* message_center, const std::string& target_id)
      : message_center_(message_center), target_id_(target_id) {
    DCHECK(message_center);
    DCHECK(!target_id.empty());
  }

  CheckObserver(const CheckObserver&) = delete;
  CheckObserver& operator=(const CheckObserver&) = delete;

  void OnNotificationUpdated(const std::string& notification_id) override {
    EXPECT_TRUE(message_center_->FindVisibleNotificationById(target_id_));
  }

 private:
  raw_ptr<MessageCenter> message_center_;
  std::string target_id_;
};

class RemoveObserver : public MessageCenterObserver {
 public:
  RemoveObserver(MessageCenter* message_center, const std::string& target_id)
      : message_center_(message_center), target_id_(target_id) {
    DCHECK(message_center);
    DCHECK(!target_id.empty());
  }

  RemoveObserver(const RemoveObserver&) = delete;
  RemoveObserver& operator=(const RemoveObserver&) = delete;

  void OnNotificationUpdated(const std::string& notification_id) override {
    message_center_->RemoveNotification(target_id_, false);
  }

 private:
  raw_ptr<MessageCenter> message_center_;
  std::string target_id_;
};

class TestAddObserver : public MessageCenterObserver {
 public:
  explicit TestAddObserver(MessageCenter* message_center)
      : message_center_(message_center) {
    message_center_->AddObserver(this);
  }

  ~TestAddObserver() override { message_center_->RemoveObserver(this); }

  void OnNotificationAdded(const std::string& id) override {
    std::string log = logs_[id];
    if (!log.empty())
      log += "_";
    logs_[id] = log + "add-" + id;
  }

  void OnNotificationUpdated(const std::string& id) override {
    std::string log = logs_[id];
    if (!log.empty())
      log += "_";
    logs_[id] = log + "update-" + id;
  }

  const std::string log(const std::string& id) { return logs_[id]; }
  void reset_logs() { logs_.clear(); }

 private:
  std::map<std::string, std::string> logs_;
  raw_ptr<MessageCenter> message_center_;
};

class TestDelegate : public NotificationDelegate {
 public:
  TestDelegate() = default;
  void Close(bool by_user) override {
    log_ += "Close_";
    log_ += (by_user ? "by_user_" : "programmatically_");
  }

  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;

  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    if (button_index) {
      if (!reply) {
        log_ += "ButtonClick_";
        log_ += base::NumberToString(*button_index) + "_";
      } else {
        log_ += "ReplyButtonClick_";
        log_ += base::NumberToString(*button_index) + "_";
        log_ += base::UTF16ToUTF8(*reply) + "_";
      }
    } else {
      log_ += "Click_";
    }
  }
  const std::string& log() { return log_; }

 private:
  ~TestDelegate() override = default;
  std::string log_;
};

class DeleteOnCloseDelegate : public NotificationDelegate {
 public:
  DeleteOnCloseDelegate(MessageCenter* message_center,
                        const std::string& notification_id)
      : message_center_(message_center), notification_id_(notification_id) {}
  DeleteOnCloseDelegate(const DeleteOnCloseDelegate&) = delete;
  DeleteOnCloseDelegate& operator=(const DeleteOnCloseDelegate&) = delete;

  void Close(bool by_user) override {
    // Removing the same notification inside Close should be a noop.
    message_center_->RemoveNotification(notification_id_, false /* by_user */);
  }
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {}

 private:
  ~DeleteOnCloseDelegate() override = default;

  raw_ptr<MessageCenter> message_center_;
  std::string notification_id_;
};

// The default app id used to create simple notifications.
const char kDefaultAppId[] = "app1";

}  // anonymous namespace

class MessageCenterImplTest : public testing::Test {
 public:
  MessageCenterImplTest() = default;

  MessageCenterImplTest(const MessageCenterImplTest&) = delete;
  MessageCenterImplTest& operator=(const MessageCenterImplTest&) = delete;

  void SetUp() override {
    MessageCenter::Initialize(std::make_unique<FakeLockScreenController>());
    message_center_ = MessageCenter::Get();
    task_environment_ =
        std::make_unique<base::test::SingleThreadTaskEnvironment>();
    run_loop_ = std::make_unique<base::RunLoop>();
    closure_ = run_loop_->QuitClosure();
  }

  void TearDown() override {
    run_loop_.reset();
    task_environment_.reset();
    message_center_ = nullptr;
    MessageCenter::Shutdown();
  }

  MessageCenter* message_center() const { return message_center_; }
  MessageCenterImpl* message_center_impl() const {
    return reinterpret_cast<MessageCenterImpl*>(message_center_.get());
  }

  base::RunLoop* run_loop() const { return run_loop_.get(); }
  base::RepeatingClosure closure() const { return closure_; }

 protected:
  std::unique_ptr<Notification> CreateSimpleNotification(
      const std::string& id) {
    return CreateNotificationWithNotifierId(id, kDefaultAppId,
                                            NOTIFICATION_TYPE_SIMPLE);
  }

  std::unique_ptr<Notification> CreateSimpleNotificationWithNotifierId(
      const std::string& id,
      const std::string& notifier_id) {
    return CreateNotificationWithNotifierId(
        id,
        notifier_id,
        NOTIFICATION_TYPE_SIMPLE);
  }

  std::unique_ptr<Notification> CreateSimpleNotificationWithDelegate(
      const std::string& id,
      scoped_refptr<NotificationDelegate> delegate) {
    return CreateNotificationWithNotifierIdAndDelegate(
        id, kDefaultAppId, NOTIFICATION_TYPE_SIMPLE, delegate,
        RichNotificationData());
  }

  std::unique_ptr<Notification> CreateSimpleNotificationWithOptionalFields(
      const std::string& id,
      const RichNotificationData& optional_fields) {
    return CreateNotificationWithNotifierIdAndDelegate(
        id, kDefaultAppId, NOTIFICATION_TYPE_SIMPLE,
        base::MakeRefCounted<TestDelegate>(), optional_fields);
  }

  std::unique_ptr<Notification> CreateNotification(const std::string& id,
                                                   NotificationType type) {
    return CreateNotificationWithNotifierId(id, kDefaultAppId, type);
  }

  std::unique_ptr<Notification> CreateNotificationWithNotifierId(
      const std::string& id,
      const std::string& notifier_id,
      NotificationType type) {
    return CreateNotificationWithNotifierIdAndDelegate(
        id, notifier_id, type, base::MakeRefCounted<TestDelegate>(),
        RichNotificationData());
  }

  std::unique_ptr<Notification> CreateNotificationWithNotifierIdAndDelegate(
      const std::string& id,
      const std::string& notifier_id,
      NotificationType type,
      scoped_refptr<NotificationDelegate> delegate,
      const RichNotificationData& optional_fields) {
    return std::make_unique<Notification>(
        type, id, u"title", UTF8ToUTF16(id), ui::ImageModel() /* icon */,
        std::u16string() /* display_source */, GURL(),
        NotifierId(NotifierType::APPLICATION, notifier_id), optional_fields,
        delegate);
  }

  TestDelegate* GetDelegate(const std::string& id) const {
    Notification* n = message_center()->FindVisibleNotificationById(id);
    return n ? static_cast<TestDelegate*>(n->delegate()) : nullptr;
  }

  FakeLockScreenController* lock_screen_controller() const {
    return static_cast<FakeLockScreenController*>(
        message_center_impl()->lock_screen_controller());
  }

 private:
  raw_ptr<MessageCenter> message_center_;
  std::unique_ptr<base::test::SingleThreadTaskEnvironment> task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::RepeatingClosure closure_;
};

namespace {

class ToggledNotificationBlocker : public NotificationBlocker {
 public:
  explicit ToggledNotificationBlocker(MessageCenter* message_center)
      : NotificationBlocker(message_center) {}

  ToggledNotificationBlocker(const ToggledNotificationBlocker&) = delete;
  ToggledNotificationBlocker& operator=(const ToggledNotificationBlocker&) =
      delete;

  ~ToggledNotificationBlocker() override = default;

  void SetPopupNotificationsEnabled(bool enabled) {
    if (popup_notifications_enabled_ == enabled)
      return;

    popup_notifications_enabled_ = enabled;
    NotifyBlockingStateChanged();
  }

  void SetNotificationsEnabled(bool enabled) {
    if (notifications_enabled_ == enabled)
      return;

    notifications_enabled_ = enabled;
    NotifyBlockingStateChanged();
  }

  // NotificationBlocker overrides:
  bool ShouldShowNotificationAsPopup(
      const Notification& notification) const override {
    return popup_notifications_enabled_;
  }

  bool ShouldShowNotification(const Notification& notification) const override {
    return notifications_enabled_;
  }

 private:
  bool notifications_enabled_ = true;
  bool popup_notifications_enabled_ = true;
};

class PopupNotificationBlocker : public ToggledNotificationBlocker {
 public:
  PopupNotificationBlocker(MessageCenter* message_center,
                           const NotifierId& allowed_notifier)
      : ToggledNotificationBlocker(message_center),
        allowed_notifier_(allowed_notifier) {}

  PopupNotificationBlocker(const PopupNotificationBlocker&) = delete;
  PopupNotificationBlocker& operator=(const PopupNotificationBlocker&) = delete;

  ~PopupNotificationBlocker() override = default;

  // NotificationBlocker overrides:
  bool ShouldShowNotificationAsPopup(
      const Notification& notification) const override {
    return (notification.notifier_id() == allowed_notifier_) ||
        ToggledNotificationBlocker::ShouldShowNotificationAsPopup(
            notification);
  }

 protected:
  const NotifierId allowed_notifier_;
};

class NearTotalNotificationBlocker : public PopupNotificationBlocker {
 public:
  NearTotalNotificationBlocker(MessageCenter* message_center,
                               const NotifierId& allowed_notifier)
      : PopupNotificationBlocker(message_center, allowed_notifier) {}

  NearTotalNotificationBlocker(const NearTotalNotificationBlocker&) = delete;
  NearTotalNotificationBlocker& operator=(const NearTotalNotificationBlocker&) =
      delete;

  ~NearTotalNotificationBlocker() override = default;

  // NotificationBlocker overrides:
  bool ShouldShowNotification(const Notification& notification) const override {
    return (notification.notifier_id() == allowed_notifier_) ||
           ToggledNotificationBlocker::ShouldShowNotification(notification);
  }
};

class TotalNotificationBlocker : public NotificationBlocker {
 public:
  explicit TotalNotificationBlocker(MessageCenter* message_center)
      : NotificationBlocker(message_center) {}

  TotalNotificationBlocker(const TotalNotificationBlocker&) = delete;
  TotalNotificationBlocker& operator=(const TotalNotificationBlocker&) = delete;

  ~TotalNotificationBlocker() override = default;

  // NotificationBlocker:
  bool ShouldShowNotification(const Notification& notification) const override {
    return false;
  }

  bool ShouldShowNotificationAsPopup(
      const Notification& notification) const override {
    return false;
  }
};

bool PopupNotificationsContain(
    const NotificationList::PopupNotifications& popups,
    const std::string& id) {
  for (const Notification* popup : popups) {
    if (popup->id() == id)
      return true;
  }
  return false;
}

// Right now, MessageCenter::HasNotification() returns regardless of blockers.
bool NotificationsContain(
    const NotificationList::Notifications& notifications,
    const std::string& id) {
  for (const Notification* notification : notifications) {
    if (notification->id() == id)
      return true;
  }
  return false;
}

}  // namespace

namespace internal {

class MockPopupTimersController : public PopupTimersController {
 public:
  MockPopupTimersController(MessageCenter* message_center,
                            base::RepeatingClosure quit_closure)
      : PopupTimersController(message_center),
        timer_finished_(0),
        quit_closure_(quit_closure) {}
  ~MockPopupTimersController() override = default;

  void TimerFinished(const std::string& id) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                                quit_closure_);
    timer_finished_++;
    last_id_ = id;
  }

  int timer_finished() const { return timer_finished_; }
  const std::string& last_id() const { return last_id_; }

 private:
  int timer_finished_;
  std::string last_id_;
  base::RepeatingClosure quit_closure_;
};

TEST_F(MessageCenterImplTest, PopupTimersEmptyController) {
  std::unique_ptr<PopupTimersController> popup_timers_controller =
      std::make_unique<PopupTimersController>(message_center());

  // Test that all functions succeed without any timers created.
  popup_timers_controller->PauseAll();
  popup_timers_controller->StartAll();
  popup_timers_controller->CancelAll();
  popup_timers_controller->TimerFinished("unknown");
  popup_timers_controller->CancelTimer("unknown");
}

TEST_F(MessageCenterImplTest, PopupTimersControllerStartTimer) {
  std::unique_ptr<MockPopupTimersController> popup_timers_controller =
      std::make_unique<MockPopupTimersController>(message_center(), closure());
  popup_timers_controller->StartTimer("test", base::Milliseconds(1));
  run_loop()->Run();
  EXPECT_EQ(popup_timers_controller->timer_finished(), 1);
}

TEST_F(MessageCenterImplTest, PopupTimersControllerCancelTimer) {
  std::unique_ptr<MockPopupTimersController> popup_timers_controller =
      std::make_unique<MockPopupTimersController>(message_center(), closure());
  popup_timers_controller->StartTimer("test", base::Milliseconds(1));
  popup_timers_controller->CancelTimer("test");
  run_loop()->RunUntilIdle();

  EXPECT_EQ(popup_timers_controller->timer_finished(), 0);
}

TEST_F(MessageCenterImplTest, PopupTimersControllerPauseAllTimers) {
  std::unique_ptr<MockPopupTimersController> popup_timers_controller =
      std::make_unique<MockPopupTimersController>(message_center(), closure());
  popup_timers_controller->StartTimer("test", base::Milliseconds(1));
  popup_timers_controller->PauseAll();
  run_loop()->RunUntilIdle();

  EXPECT_EQ(popup_timers_controller->timer_finished(), 0);
}

TEST_F(MessageCenterImplTest, PopupTimersControllerStartAllTimers) {
  std::unique_ptr<MockPopupTimersController> popup_timers_controller =
      std::make_unique<MockPopupTimersController>(message_center(), closure());
  popup_timers_controller->StartTimer("test", base::Milliseconds(1));
  popup_timers_controller->PauseAll();
  popup_timers_controller->StartAll();
  run_loop()->Run();

  EXPECT_EQ(popup_timers_controller->timer_finished(), 1);
}

TEST_F(MessageCenterImplTest, PopupTimersControllerStartMultipleTimers) {
  std::unique_ptr<MockPopupTimersController> popup_timers_controller =
      std::make_unique<MockPopupTimersController>(message_center(), closure());
  popup_timers_controller->StartTimer("test", base::TimeDelta::Max());
  popup_timers_controller->StartTimer("test2", base::Milliseconds(1));
  popup_timers_controller->StartTimer("test3", base::TimeDelta::Max());
  popup_timers_controller->PauseAll();
  popup_timers_controller->StartAll();
  run_loop()->Run();

  EXPECT_EQ(popup_timers_controller->last_id(), "test2");
  EXPECT_EQ(popup_timers_controller->timer_finished(), 1);
}

TEST_F(MessageCenterImplTest, PopupTimersControllerRestartOnUpdate) {
  scoped_refptr<base::SingleThreadTaskRunner> old_task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>(
          base::Time::Now(), base::TimeTicks::Now());
  base::CurrentThread::Get()->SetTaskRunner(task_runner);

  NotifierId notifier_id(GURL("https://example.com"));

  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id1", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id, RichNotificationData(), nullptr));

  std::unique_ptr<MockPopupTimersController> popup_timers_controller =
      std::make_unique<MockPopupTimersController>(message_center(), closure());

  popup_timers_controller->OnNotificationDisplayed("id1", DISPLAY_SOURCE_POPUP);
  ASSERT_EQ(popup_timers_controller->timer_finished(), 0);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const int dismiss_time =
      popup_timers_controller->GetNotificationTimeoutDefault();
#else
  const int dismiss_time = kAutocloseHighPriorityDelaySeconds;
#endif

  // Fast forward the |task_runner| by one second less than the auto-close timer
  // frequency for Web Notifications. (As set by the |notifier_id|.)
  task_runner->FastForwardBy(base::Seconds(dismiss_time - 1));
  ASSERT_EQ(popup_timers_controller->timer_finished(), 0);

  // Trigger a replacement of the notification in the timer controller.
  popup_timers_controller->OnNotificationUpdated("id1");

  // Fast forward the |task_runner| by one second less than the auto-close timer
  // frequency for Web Notifications again. It should have been reset.
  task_runner->FastForwardBy(base::Seconds(dismiss_time - 1));
  ASSERT_EQ(popup_timers_controller->timer_finished(), 0);

  // Now fast forward the |task_runner| by two seconds (to avoid flakiness),
  // after which the timer should have fired.
  task_runner->FastForwardBy(base::Seconds(2));
  ASSERT_EQ(popup_timers_controller->timer_finished(), 1);

  base::CurrentThread::Get()->SetTaskRunner(old_task_runner);
}

TEST_F(MessageCenterImplTest, Renotify) {
  message_center()->SetHasMessageCenterView(true);
  const std::string id("id");

  // Add notification initially.
  std::unique_ptr<Notification> notification = CreateSimpleNotification(id);
  message_center()->AddNotification(std::move(notification));
  auto popups = message_center()->GetPopupNotifications();
  EXPECT_EQ(1u, popups.size());
  EXPECT_TRUE(PopupNotificationsContain(popups, id));

  // Mark notification as shown.
  message_center()->MarkSinglePopupAsShown(id, true);
  EXPECT_EQ(0u, message_center()->GetPopupNotifications().size());

  // Add notification again without |renotify| flag. It should not pop-up again.
  notification = CreateSimpleNotification(id);
  message_center()->AddNotification(std::move(notification));
  EXPECT_EQ(0u, message_center()->GetPopupNotifications().size());

  // Add notification again with |renotify| flag. It should pop-up again.
  notification = CreateSimpleNotification(id);
  notification->set_renotify(true);
  message_center()->AddNotification(std::move(notification));
  popups = message_center()->GetPopupNotifications();
  EXPECT_EQ(1u, popups.size());
  EXPECT_TRUE(PopupNotificationsContain(popups, id));
}

TEST_F(MessageCenterImplTest, NotificationBlocker) {
  NotifierId notifier_id(NotifierType::APPLICATION, "app1");
  // Multiple blockers to verify the case that one blocker blocks but another
  // doesn't.
  ToggledNotificationBlocker blocker1(message_center());
  blocker1.Init();
  ToggledNotificationBlocker blocker2(message_center());
  blocker2.Init();

  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id1", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id, RichNotificationData(), nullptr));
  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id2", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id, RichNotificationData(), nullptr));
  EXPECT_EQ(2u, message_center()->GetPopupNotifications().size());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  // "id1" is displayed as a pop-up so that it will be closed when blocked.
  message_center()->DisplayedNotification("id1", DISPLAY_SOURCE_POPUP);

  // Block all notifications. All popups are gone and message center should be
  // hidden.
  blocker1.SetPopupNotificationsEnabled(false);
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  // Updates |blocker2| state, which doesn't affect the global state.
  blocker2.SetPopupNotificationsEnabled(false);
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  blocker2.SetPopupNotificationsEnabled(true);
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  // If |blocker2| blocks, then unblocking blocker1 doesn't change the global
  // state.
  blocker2.SetPopupNotificationsEnabled(false);
  blocker1.SetPopupNotificationsEnabled(true);
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  // Unblock both blockers, which recovers the global state, the displayed
  // pop-ups before blocking aren't shown but the never-displayed ones will
  // be shown.
  blocker2.SetPopupNotificationsEnabled(true);
  NotificationList::PopupNotifications popups =
      message_center()->GetPopupNotifications();
  EXPECT_EQ(1u, popups.size());
  EXPECT_TRUE(PopupNotificationsContain(popups, "id2"));
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());
}

TEST_F(MessageCenterImplTest, MarkPopupAsShownWhileBlocked) {
  const std::string kMarkedId = "id1";
  const std::string kNotMarkedId = "id2";

  ToggledNotificationBlocker blocker(message_center());
  blocker.Init();

  message_center()->AddNotification(CreateSimpleNotification(kMarkedId));
  message_center()->AddNotification(CreateSimpleNotification(kNotMarkedId));

  EXPECT_EQ(message_center()->GetPopupNotifications().size(), 2u);
  EXPECT_EQ(message_center()->GetVisibleNotifications().size(), 2u);

  // Block all notifications. There should be no popups or visible
  // notifications.
  blocker.SetPopupNotificationsEnabled(false);
  blocker.SetNotificationsEnabled(false);
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_TRUE(message_center()->GetVisibleNotifications().empty());
  EXPECT_EQ(message_center()->GetNotifications().size(), 2u);

  // Mark one notification as being shown as a popup, so that when blocking ends
  // it will not be displayed.
  message_center()->MarkSinglePopupAsShown(kMarkedId, false);

  // Stop blocking notifications, which should cause notifications to show as
  // popups and in notifications center depending on their state.
  blocker.SetPopupNotificationsEnabled(true);
  blocker.SetNotificationsEnabled(true);

  // Only the notification we did not mark should show as a popup.
  NotificationList::PopupNotifications popups =
      message_center()->GetPopupNotifications();
  EXPECT_EQ(popups.size(), 1u);
  EXPECT_TRUE(PopupNotificationsContain(popups, kNotMarkedId));

  // Both notifications should be visible.
  EXPECT_EQ(message_center()->GetNotifications().size(), 2u);
  EXPECT_EQ(message_center()->GetVisibleNotifications().size(), 2u);
}

TEST_F(MessageCenterImplTest, VisibleNotificationsWithoutBlocker) {
  NotifierId notifier_id1(NotifierType::APPLICATION, /*id=*/"app1");
  NotifierId notifier_id2(NotifierType::APPLICATION, /*id=*/"app2");
  NearTotalNotificationBlocker blocker_1(message_center(), notifier_id1);
  blocker_1.Init();
  NearTotalNotificationBlocker blocker_2(message_center(), notifier_id2);
  blocker_2.Init();

  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id1", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id1, RichNotificationData(), nullptr));
  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id2", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id2, RichNotificationData(), nullptr));

  blocker_1.SetNotificationsEnabled(false);
  blocker_2.SetNotificationsEnabled(false);

  // Without blocker 1, notification 2 is visible.
  NotificationList::Notifications notifications_1 =
      message_center()->GetVisibleNotificationsWithoutBlocker(&blocker_1);
  EXPECT_EQ(1u, notifications_1.size());
  EXPECT_TRUE(NotificationsContain(notifications_1, "id2"));

  // Without blocker 2, notification 1 is visible.
  NotificationList::Notifications notifications_2 =
      message_center()->GetVisibleNotificationsWithoutBlocker(&blocker_2);
  EXPECT_EQ(1u, notifications_2.size());
  EXPECT_TRUE(NotificationsContain(notifications_2, "id1"));
}

TEST_F(MessageCenterImplTest, PopupsWithoutBlocker) {
  NotifierId notifier_id1(NotifierType::APPLICATION, "app1");
  NotifierId notifier_id2(NotifierType::APPLICATION, "app2");
  NotifierId notifier_id3(NotifierType::APPLICATION, "app3");
  PopupNotificationBlocker blocker_1(message_center(), notifier_id1);
  blocker_1.Init();
  PopupNotificationBlocker blocker_2(message_center(), notifier_id2);
  blocker_2.Init();
  PopupNotificationBlocker blocker_3(message_center(), notifier_id3);
  blocker_3.Init();

  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id1", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id1, RichNotificationData(), nullptr));
  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id2", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id2, RichNotificationData(), nullptr));

  // Verify that the method doesn't mark popups as seen.
  NotificationList::PopupNotifications initial_popups =
      message_center()->GetPopupNotificationsWithoutBlocker(blocker_3);
  NotificationList::PopupNotifications final_popups =
      message_center()->GetPopupNotificationsWithoutBlocker(blocker_3);
  EXPECT_EQ(2u, initial_popups.size());
  EXPECT_TRUE(PopupNotificationsContain(initial_popups, "id1"));
  EXPECT_TRUE(PopupNotificationsContain(initial_popups, "id2"));
  EXPECT_EQ(initial_popups, final_popups);

  blocker_1.SetPopupNotificationsEnabled(false);
  blocker_2.SetPopupNotificationsEnabled(false);

  // Without blocker 1, popup 2 can be seen.
  NotificationList::PopupNotifications popups_1 =
      message_center()->GetPopupNotificationsWithoutBlocker(blocker_1);
  EXPECT_EQ(1u, popups_1.size());
  EXPECT_TRUE(PopupNotificationsContain(popups_1, "id2"));

  // Without blocker 2, popup 1 can be seen.
  NotificationList::PopupNotifications popups_2 =
      message_center()->GetPopupNotificationsWithoutBlocker(blocker_2);
  EXPECT_EQ(1u, popups_2.size());
  EXPECT_TRUE(PopupNotificationsContain(popups_2, "id1"));
}

TEST_F(MessageCenterImplTest, NotificationsDuringBlocked) {
  NotifierId notifier_id(NotifierType::APPLICATION, "app1");
  ToggledNotificationBlocker blocker(message_center());
  blocker.Init();

  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id1", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id, RichNotificationData(), nullptr));
  EXPECT_EQ(1u, message_center()->GetPopupNotifications().size());
  EXPECT_EQ(1u, message_center()->GetVisibleNotifications().size());

  // "id1" is displayed as a pop-up so that it will be closed when blocked.
  message_center()->DisplayedNotification("id1", DISPLAY_SOURCE_POPUP);

  // Create a notification during blocked. Still no popups.
  blocker.SetPopupNotificationsEnabled(false);
  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id2", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id, RichNotificationData(), nullptr));
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  // Unblock notifications, the id1 should appear as a popup.
  blocker.SetPopupNotificationsEnabled(true);
  NotificationList::PopupNotifications popups =
      message_center()->GetPopupNotifications();
  EXPECT_EQ(1u, popups.size());
  EXPECT_TRUE(PopupNotificationsContain(popups, "id2"));
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());
}

TEST_F(MessageCenterImplTest, GetNotifications) {
  NotifierId notifier_id(NotifierType::APPLICATION, "app1");
  ToggledNotificationBlocker blocker(message_center());
  blocker.Init();

  // Create a notification without any blockers.
  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id1", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id, RichNotificationData(), nullptr));
  EXPECT_EQ(1u, message_center()->GetPopupNotifications().size());
  EXPECT_EQ(1u, message_center()->GetVisibleNotifications().size());
  EXPECT_EQ(1u, message_center()->GetNotifications().size());

  // Create a notification while blocking popup notifications.
  blocker.SetPopupNotificationsEnabled(false);
  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id2", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id, RichNotificationData(), nullptr));
  EXPECT_EQ(0u, message_center()->GetPopupNotifications().size());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());
  EXPECT_EQ(2u, message_center()->GetNotifications().size());

  // Create a notification while any notification is blocked.
  blocker.SetNotificationsEnabled(false);
  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id3", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id, RichNotificationData(), nullptr));
  EXPECT_EQ(0u, message_center()->GetPopupNotifications().size());
  EXPECT_EQ(0u, message_center()->GetVisibleNotifications().size());
  EXPECT_EQ(3u, message_center()->GetNotifications().size());

  // Allow non-popup notifications again.
  blocker.SetNotificationsEnabled(true);
  EXPECT_EQ(0u, message_center()->GetPopupNotifications().size());
  EXPECT_EQ(3u, message_center()->GetVisibleNotifications().size());
  EXPECT_EQ(3u, message_center()->GetNotifications().size());

  // Allow popup notifications again.
  blocker.SetPopupNotificationsEnabled(true);
  EXPECT_EQ(3u, message_center()->GetPopupNotifications().size());
  EXPECT_EQ(3u, message_center()->GetVisibleNotifications().size());
  EXPECT_EQ(3u, message_center()->GetNotifications().size());
}

// Similar to other blocker cases but this test case allows |notifier_id2| even
// in blocked.
TEST_F(MessageCenterImplTest, NotificationBlockerAllowsPopups) {
  NotifierId notifier_id1(NotifierType::APPLICATION, "app1");
  NotifierId notifier_id2(NotifierType::APPLICATION, "app2");
  PopupNotificationBlocker blocker(message_center(), notifier_id2);
  blocker.Init();

  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id1", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id1, RichNotificationData(), nullptr));
  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id2", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id2, RichNotificationData(), nullptr));

  // "id1" is displayed as a pop-up so that it will be closed when blocked.
  message_center()->DisplayedNotification("id1", DISPLAY_SOURCE_POPUP);

  // "id1" is closed but "id2" is still visible as a popup.
  blocker.SetPopupNotificationsEnabled(false);
  NotificationList::PopupNotifications popups =
      message_center()->GetPopupNotifications();
  EXPECT_EQ(1u, popups.size());
  EXPECT_TRUE(PopupNotificationsContain(popups, "id2"));
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id3", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id1, RichNotificationData(), nullptr));
  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id4", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id2, RichNotificationData(), nullptr));
  popups = message_center()->GetPopupNotifications();
  EXPECT_EQ(2u, popups.size());
  EXPECT_TRUE(PopupNotificationsContain(popups, "id2"));
  EXPECT_TRUE(PopupNotificationsContain(popups, "id4"));
  EXPECT_EQ(4u, message_center()->GetVisibleNotifications().size());

  blocker.SetPopupNotificationsEnabled(true);
  popups = message_center()->GetPopupNotifications();
  EXPECT_EQ(3u, popups.size());
  EXPECT_TRUE(PopupNotificationsContain(popups, "id2"));
  EXPECT_TRUE(PopupNotificationsContain(popups, "id3"));
  EXPECT_TRUE(PopupNotificationsContain(popups, "id4"));
  EXPECT_EQ(4u, message_center()->GetVisibleNotifications().size());
}

// NearTotalNotificationBlocker suppresses showing notifications even from the
// list. This would provide the feature to 'separated' message centers
// per-profile for ChromeOS multi-login.
TEST_F(MessageCenterImplTest, NearTotalNotificationBlocker) {
  NotifierId notifier_id1(NotifierType::APPLICATION, "app1");
  NotifierId notifier_id2(NotifierType::APPLICATION, "app2");
  NearTotalNotificationBlocker blocker(message_center(), notifier_id2);
  blocker.Init();

  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id1", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id1, RichNotificationData(), nullptr));
  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id2", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id2, RichNotificationData(), nullptr));

  // "id1" becomes invisible while "id2" is still visible.
  blocker.SetNotificationsEnabled(false);
  EXPECT_EQ(1u, message_center()->NotificationCount());
  NotificationList::Notifications notifications =
      message_center()->GetVisibleNotifications();
  EXPECT_FALSE(NotificationsContain(notifications, "id1"));
  EXPECT_TRUE(NotificationsContain(notifications, "id2"));

  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id3", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id1, RichNotificationData(), nullptr));
  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id4", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id2, RichNotificationData(), nullptr));
  EXPECT_EQ(2u, message_center()->NotificationCount());
  notifications = message_center()->GetVisibleNotifications();
  EXPECT_FALSE(NotificationsContain(notifications, "id1"));
  EXPECT_TRUE(NotificationsContain(notifications, "id2"));
  EXPECT_FALSE(NotificationsContain(notifications, "id3"));
  EXPECT_TRUE(NotificationsContain(notifications, "id4"));

  blocker.SetNotificationsEnabled(true);
  EXPECT_EQ(4u, message_center()->NotificationCount());
  notifications = message_center()->GetVisibleNotifications();
  EXPECT_TRUE(NotificationsContain(notifications, "id1"));
  EXPECT_TRUE(NotificationsContain(notifications, "id2"));
  EXPECT_TRUE(NotificationsContain(notifications, "id3"));
  EXPECT_TRUE(NotificationsContain(notifications, "id4"));

  // Remove just visible notifications.
  blocker.SetNotificationsEnabled(false);
  message_center()->RemoveAllNotifications(
      false /* by_user */, MessageCenter::RemoveType::NON_PINNED);
  EXPECT_EQ(0u, message_center()->NotificationCount());
  blocker.SetNotificationsEnabled(true);
  EXPECT_EQ(2u, message_center()->NotificationCount());
  notifications = message_center()->GetVisibleNotifications();
  EXPECT_TRUE(NotificationsContain(notifications, "id1"));
  EXPECT_FALSE(NotificationsContain(notifications, "id2"));
  EXPECT_TRUE(NotificationsContain(notifications, "id3"));
  EXPECT_FALSE(NotificationsContain(notifications, "id4"));

  // And remove all including invisible notifications.
  blocker.SetNotificationsEnabled(false);
  message_center()->RemoveAllNotifications(false /* by_user */,
                                           MessageCenter::RemoveType::ALL);
  EXPECT_EQ(0u, message_center()->NotificationCount());
}

// Tests that notification state is updated when a notification blocker is
// added.
TEST_F(MessageCenterImplTest, NotificationsUpdatedWhenBlockerAdded) {
  // Add a notification and display it as a popup.
  NotifierId notifier_id1(NotifierType::APPLICATION, "app1");
  message_center()->AddNotification(
      CreateSimpleNotificationWithNotifierId("id1", notifier_id1.id));
  message_center()->DisplayedNotification("id1", DISPLAY_SOURCE_POPUP);

  // Verify that it is not blocked.
  ASSERT_EQ(1u, message_center()->NotificationCount());
  NotificationList::Notifications notifications =
      message_center()->GetVisibleNotifications();
  ASSERT_TRUE(NotificationsContain(notifications, "id1"));
  NotificationList::PopupNotifications popups =
      message_center()->GetPopupNotifications();
  ASSERT_EQ(1u, popups.size());
  ASSERT_TRUE(PopupNotificationsContain(popups, "id1"));

  // Create a `NotificationBlocker` that blocks all notifications (even popups).
  TotalNotificationBlocker blocker(message_center());
  blocker.Init();

  // Verify that "id1" is now blocked.
  EXPECT_EQ(0u, message_center()->NotificationCount());
  EXPECT_FALSE(
      NotificationsContain(message_center()->GetVisibleNotifications(), "id1"));
  popups = message_center()->GetPopupNotifications();
  EXPECT_EQ(0u, popups.size());
  EXPECT_FALSE(PopupNotificationsContain(popups, "id1"));
}

// Tests that notification state is updated when a notification blocker is
// removed.
TEST_F(MessageCenterImplTest, NotificationsUpdatedWhenBlockerRemoved) {
  // Add two notifications and display them as popups.
  NotifierId notifier_id1(NotifierType::APPLICATION, "app1");
  NotifierId notifier_id2(NotifierType::APPLICATION, "app2");
  message_center()->AddNotification(
      CreateSimpleNotificationWithNotifierId("id1", notifier_id1.id));
  message_center()->AddNotification(
      CreateSimpleNotificationWithNotifierId("id2", notifier_id2.id));
  message_center()->DisplayedNotification("id1", DISPLAY_SOURCE_POPUP);
  message_center()->DisplayedNotification("id2", DISPLAY_SOURCE_POPUP);

  // Verify that they are not blocked.
  ASSERT_EQ(2u, message_center()->NotificationCount());
  NotificationList::Notifications notifications =
      message_center()->GetVisibleNotifications();
  ASSERT_TRUE(NotificationsContain(notifications, "id1"));
  ASSERT_TRUE(NotificationsContain(notifications, "id2"));
  NotificationList::PopupNotifications popups =
      message_center()->GetPopupNotifications();
  ASSERT_EQ(2u, popups.size());
  ASSERT_TRUE(PopupNotificationsContain(popups, "id1"));
  ASSERT_TRUE(PopupNotificationsContain(popups, "id2"));

  {
    // Block all notifications, including popups (except those from
    // `notifier_id2`).
    NearTotalNotificationBlocker blocker(message_center(), notifier_id2);
    blocker.Init();
    blocker.SetNotificationsEnabled(false);
    blocker.SetPopupNotificationsEnabled(false);

    // Verify that "id1" is blocked and "id2" is not.
    ASSERT_EQ(1u, message_center()->NotificationCount());
    ASSERT_FALSE(NotificationsContain(
        message_center()->GetVisibleNotifications(), "id1"));
    ASSERT_TRUE(NotificationsContain(
        message_center()->GetVisibleNotifications(), "id2"));
    popups = message_center()->GetPopupNotifications();
    ASSERT_EQ(1u, popups.size());
    ASSERT_FALSE(PopupNotificationsContain(popups, "id1"));
    ASSERT_TRUE(PopupNotificationsContain(popups, "id2"));

    // Add a third notification.
    message_center()->AddNotification(
        CreateSimpleNotificationWithNotifierId("id3", notifier_id1.id));
    message_center()->DisplayedNotification("id3", DISPLAY_SOURCE_POPUP);

    // Verify that "id3" is blocked.
    ASSERT_EQ(1u, message_center()->NotificationCount());
    ASSERT_FALSE(NotificationsContain(
        message_center()->GetVisibleNotifications(), "id3"));
    popups = message_center()->GetPopupNotifications();
    ASSERT_EQ(1u, popups.size());
    ASSERT_FALSE(PopupNotificationsContain(popups, "id3"));

    // Remove the blocker (it is removed in its dtor, which is called when
    // exiting this scope).
  }

  // Verify that the notifications are not blocked, and in particular that "id2"
  // is now shown as a popup since it didn't initially get to show as a popup.
  EXPECT_EQ(3u, message_center()->NotificationCount());
  notifications = message_center()->GetVisibleNotifications();
  EXPECT_TRUE(NotificationsContain(notifications, "id1"));
  EXPECT_TRUE(NotificationsContain(notifications, "id2"));
  EXPECT_TRUE(NotificationsContain(notifications, "id3"));
  popups = message_center()->GetPopupNotifications();
  EXPECT_EQ(2u, popups.size());
  EXPECT_TRUE(PopupNotificationsContain(popups, "id2"));
  EXPECT_TRUE(PopupNotificationsContain(popups, "id3"));
}

TEST_F(MessageCenterImplTest, RemoveAllNotifications) {
  NotifierId notifier_id1(NotifierType::APPLICATION, "app1");
  NotifierId notifier_id2(NotifierType::APPLICATION, "app2");

  NearTotalNotificationBlocker blocker(message_center(), notifier_id1);
  blocker.Init();
  blocker.SetNotificationsEnabled(false);

  // Notification 1: Visible, non-pinned
  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id1", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id1, RichNotificationData(), nullptr));

  // Notification 2: Invisible, non-pinned
  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id2", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id2, RichNotificationData(), nullptr));

  // Remove all the notifications which are visible and non-pinned.
  message_center()->RemoveAllNotifications(
      false /* by_user */, MessageCenter::RemoveType::NON_PINNED);

  EXPECT_EQ(0u, message_center()->NotificationCount());
  blocker.SetNotificationsEnabled(true);  // Show invisible notifications.
  EXPECT_EQ(1u, message_center()->NotificationCount());

  NotificationList::Notifications notifications =
      message_center()->GetVisibleNotifications();
  // Notification 1 should be removed.
  EXPECT_FALSE(NotificationsContain(notifications, "id1"));
  // Notification 2 shouldn't be removed since it was invisible.
  EXPECT_TRUE(NotificationsContain(notifications, "id2"));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(MessageCenterImplTest, RemoveAllNotificationsWithPinned) {
  NotifierId notifier_id1(NotifierType::APPLICATION, "app1");
  NotifierId notifier_id2(NotifierType::APPLICATION, "app2");

  NearTotalNotificationBlocker blocker(message_center(), notifier_id1);
  blocker.Init();
  blocker.SetNotificationsEnabled(false);

  // Notification 1: Visible, non-pinned
  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id1", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id1, RichNotificationData(), nullptr));

  // Notification 2: Invisible, non-pinned
  message_center()->AddNotification(std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id2", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id2, RichNotificationData(), nullptr));

  // Notification 3: Visible, pinned
  auto notification3 = std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id3", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id1, RichNotificationData(), nullptr);
  notification3->set_pinned(true);
  message_center()->AddNotification(std::move(notification3));

  // Notification 4: Invisible, pinned
  auto notification4 = std::make_unique<Notification>(
      NOTIFICATION_TYPE_SIMPLE, "id4", u"title", u"message",
      ui::ImageModel() /* icon */, std::u16string() /* display_source */,
      GURL(), notifier_id2, RichNotificationData(), nullptr);
  notification4->set_pinned(true);
  message_center()->AddNotification(std::move(notification4));

  // Remove all the notifications which are visible and non-pinned.
  message_center()->RemoveAllNotifications(
      false /* by_user */, MessageCenter::RemoveType::NON_PINNED);

  EXPECT_EQ(1u, message_center()->NotificationCount());
  blocker.SetNotificationsEnabled(true);  // Show invisible notifications.
  EXPECT_EQ(3u, message_center()->NotificationCount());

  NotificationList::Notifications notifications =
      message_center()->GetVisibleNotifications();
  // Notification 1 should be removed.
  EXPECT_FALSE(NotificationsContain(notifications, "id1"));
  // Notification 2 shouldn't be removed since it was invisible.
  EXPECT_TRUE(NotificationsContain(notifications, "id2"));
  // Notification 3 shouldn't be removed since it was pinned.
  EXPECT_TRUE(NotificationsContain(notifications, "id3"));
  // Notification 4 shouldn't be removed since it was invisible and pinned.
  EXPECT_TRUE(NotificationsContain(notifications, "id4"));
}
#endif

TEST_F(MessageCenterImplTest, NotifierEnabledChanged) {
  ASSERT_EQ(0u, message_center()->NotificationCount());
  message_center()->AddNotification(
      CreateSimpleNotificationWithNotifierId("id1-1", "app1"));
  message_center()->AddNotification(
      CreateSimpleNotificationWithNotifierId("id1-2", "app1"));
  message_center()->AddNotification(
      CreateSimpleNotificationWithNotifierId("id1-3", "app1"));
  message_center()->AddNotification(
      CreateSimpleNotificationWithNotifierId("id2-1", "app2"));
  message_center()->AddNotification(
      CreateSimpleNotificationWithNotifierId("id2-2", "app2"));
  message_center()->AddNotification(
      CreateSimpleNotificationWithNotifierId("id2-3", "app2"));
  message_center()->AddNotification(
      CreateSimpleNotificationWithNotifierId("id2-4", "app2"));
  message_center()->AddNotification(
      CreateSimpleNotificationWithNotifierId("id2-5", "app2"));
  ASSERT_EQ(8u, message_center()->NotificationCount());

  // Removing all of app2's notifications should only leave app1's.
  message_center()->RemoveNotificationsForNotifierId(
      NotifierId(NotifierType::APPLICATION, "app2"));
  ASSERT_EQ(3u, message_center()->NotificationCount());

  // Removal operations should be idempotent.
  message_center()->RemoveNotificationsForNotifierId(
      NotifierId(NotifierType::APPLICATION, "app2"));
  ASSERT_EQ(3u, message_center()->NotificationCount());

  // Now we remove the remaining notifications.
  message_center()->RemoveNotificationsForNotifierId(
      NotifierId(NotifierType::APPLICATION, "app1"));
  ASSERT_EQ(0u, message_center()->NotificationCount());
}

TEST_F(MessageCenterImplTest, UpdateWhileMessageCenterVisible) {
  std::string id1("id1");
  std::string id2("id2");
  NotifierId notifier_id1(NotifierType::APPLICATION, "app1");

  // First, add and update a notification to ensure updates happen
  // normally.
  std::unique_ptr<Notification> notification = CreateSimpleNotification(id1);
  message_center()->AddNotification(std::move(notification));
  notification = CreateSimpleNotification(id2);
  message_center()->UpdateNotification(id1, std::move(notification));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(id2));
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(id1));

  // Then open the message center.
  message_center()->SetVisibility(VISIBILITY_MESSAGE_CENTER);

  // Then update a notification; the update should have propagated.
  notification = CreateSimpleNotification(id1);
  message_center()->UpdateNotification(id2, std::move(notification));
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(id2));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(id1));
}

TEST_F(MessageCenterImplTest, AddWhileMessageCenterVisible) {
  std::string id("id1");

  // Then open the message center.
  message_center()->SetVisibility(VISIBILITY_MESSAGE_CENTER);

  // Add a notification and confirm the adding should have propagated.
  std::unique_ptr<Notification> notification = CreateSimpleNotification(id);
  message_center()->AddNotification(std::move(notification));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(id));
}

TEST_F(MessageCenterImplTest, RemoveWhileMessageCenterVisible) {
  std::string id("id1");

  // First, add a notification to ensure updates happen normally.
  std::unique_ptr<Notification> notification = CreateSimpleNotification(id);
  message_center()->AddNotification(std::move(notification));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(id));

  // Then open the message center.
  message_center()->SetVisibility(VISIBILITY_MESSAGE_CENTER);

  // Then update a notification; the update should have propagated.
  message_center()->RemoveNotification(id, false);
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(id));
}

TEST_F(MessageCenterImplTest, RemoveNonVisibleNotification) {
  // Add two notifications.
  message_center()->AddNotification(CreateSimpleNotification("id1"));
  message_center()->AddNotification(CreateSimpleNotification("id2"));
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  // Add a blocker to block all notifications.
  NotifierId allowed_notifier_id(NotifierType::APPLICATION, "notifier");
  NearTotalNotificationBlocker blocker(message_center(), allowed_notifier_id);
  blocker.Init();
  blocker.SetNotificationsEnabled(false);
  EXPECT_EQ(0u, message_center()->GetVisibleNotifications().size());

  // Removing a non-visible notification should work.
  message_center()->RemoveNotification("id1", false);
  blocker.SetNotificationsEnabled(true);
  EXPECT_EQ(1u, message_center()->GetVisibleNotifications().size());

  // Also try removing a visible notification.
  message_center()->RemoveNotification("id2", false);
  EXPECT_EQ(0u, message_center()->GetVisibleNotifications().size());
}

TEST_F(MessageCenterImplTest, RemoveInCloseHandler) {
  const std::string id("id1");

  // Create a notification that calls RemoveNotification() on close.
  auto notification = CreateSimpleNotificationWithDelegate(
      id, base::MakeRefCounted<DeleteOnCloseDelegate>(message_center(), id));
  message_center()->AddNotification(std::move(notification));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(id));

  // Then remove the notification which calls RemoveNotification() reentrantly.
  message_center()->RemoveNotification(id, true /* by_user */);
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(id));
}

// Regression test for https://crbug.com/1135709
TEST_F(MessageCenterImplTest, RemoveInCloseHandlerCloseAll) {
  const std::string id1("id1");
  const std::string id2("id2");

  // Create two notifications that call RemoveNotification() on close.
  auto notification1 = CreateSimpleNotificationWithDelegate(
      id1, base::MakeRefCounted<DeleteOnCloseDelegate>(message_center(), id1));
  auto notification2 = CreateSimpleNotificationWithDelegate(
      id2, base::MakeRefCounted<DeleteOnCloseDelegate>(message_center(), id2));
  message_center()->AddNotification(std::move(notification1));
  message_center()->AddNotification(std::move(notification2));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(id1));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(id2));

  // Then remove all notifications which calls RemoveNotification() reentrantly.
  message_center()->RemoveAllNotifications(
      true /* by_user */,
      message_center::MessageCenter::RemoveType::NON_PINNED);
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(id1));
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(id2));
}

TEST_F(MessageCenterImplTest, FindNotificationsByAppId) {
  message_center()->SetHasMessageCenterView(true);

  const std::string app_id1("app_id1");
  const std::string app_id2("app_id2");

  {
    // Add a notification for |app_id1|.
    const std::string id1("id1");
    std::unique_ptr<Notification> notification =
        CreateNotificationWithNotifierId(id1, app_id1,
                                         NOTIFICATION_TYPE_SIMPLE);
    message_center()->AddNotification(std::move(notification));
    EXPECT_EQ(1u, message_center()->FindNotificationsByAppId(app_id1).size());

    // Mark the notification as shown but not read.
    message_center()->MarkSinglePopupAsShown(id1, false);
    EXPECT_EQ(1u, message_center()->FindNotificationsByAppId(app_id1).size());

    // Mark the notification as shown and read.
    message_center()->MarkSinglePopupAsShown(id1, true);
    EXPECT_EQ(1u, message_center()->FindNotificationsByAppId(app_id1).size());

    // Remove the notification.
    message_center()->RemoveNotification(id1, true);
    EXPECT_EQ(0u, message_center()->FindNotificationsByAppId(app_id1).size());

    // Add two notifications for |app_id1|.
    notification = CreateNotificationWithNotifierId(id1, app_id1,
                                                    NOTIFICATION_TYPE_SIMPLE);
    message_center()->AddNotification(std::move(notification));

    const std::string id2("id2");
    notification = CreateNotificationWithNotifierId(id2, app_id1,
                                                    NOTIFICATION_TYPE_SIMPLE);
    message_center()->AddNotification(std::move(notification));
    EXPECT_EQ(2u, message_center()->FindNotificationsByAppId(app_id1).size());

    // Remove |id2|,there should only be one notification for |app_id1|.
    message_center()->RemoveNotification(id2, true);
    EXPECT_EQ(1u, message_center()->FindNotificationsByAppId(app_id1).size());

    // Add a notification for |app_id2|.
    const std::string id3("id3");
    notification = CreateNotificationWithNotifierId(id3, app_id2,
                                                    NOTIFICATION_TYPE_SIMPLE);
    message_center()->AddNotification(std::move(notification));
    EXPECT_EQ(1u, message_center()->FindNotificationsByAppId(app_id1).size());
    EXPECT_EQ(1u, message_center()->FindNotificationsByAppId(app_id2).size());
  }

  for (std::string app_id : {app_id1, app_id2}) {
    for (Notification* notification :
         message_center()->FindNotificationsByAppId(app_id)) {
      EXPECT_EQ(app_id, notification->notifier_id().id);
    }
  }

  // Remove all notifications.
  message_center()->RemoveAllNotifications(true,
                                           MessageCenterImpl::RemoveType::ALL);

  EXPECT_EQ(0u, message_center()->FindNotificationsByAppId(app_id1).size());
  EXPECT_EQ(0u, message_center()->FindNotificationsByAppId(app_id2).size());
}

TEST_F(MessageCenterImplTest, QueueWhenCenterVisible) {
  TestAddObserver observer(message_center());

  message_center()->AddNotification(CreateSimpleNotification("n"));
  message_center()->SetVisibility(VISIBILITY_MESSAGE_CENTER);
  message_center()->AddNotification(CreateSimpleNotification("n2"));

  // 'update-n' should happen since SetVisibility updates is_read status of n.
  EXPECT_EQ("add-n_update-n", observer.log("n"));

  message_center()->SetVisibility(VISIBILITY_TRANSIENT);

  EXPECT_EQ("add-n2", observer.log("n2"));
}

TEST_F(MessageCenterImplTest, UpdateProgressNotificationWhenCenterVisible) {
  TestAddObserver observer(message_center());

  // Add a progress notification and update it while the message center
  // is visible.
  std::unique_ptr<Notification> notification = CreateSimpleNotification("n");
  notification->set_type(NOTIFICATION_TYPE_PROGRESS);
  Notification notification_copy = *notification;
  message_center()->AddNotification(std::move(notification));
  message_center()->ClickOnNotification("n");
  message_center()->SetVisibility(VISIBILITY_MESSAGE_CENTER);
  observer.reset_logs();
  notification_copy.set_progress(50);
  message_center()->UpdateNotification(
      "n", std::make_unique<Notification>(notification_copy));

  // Expect that the progress notification update is performed.
  EXPECT_EQ("update-n", observer.log("n"));
}

TEST_F(MessageCenterImplTest, UpdateNonProgressNotificationWhenCenterVisible) {
  TestAddObserver observer(message_center());

  // Add a non-progress notification and update it while the message center
  // is visible.
  std::unique_ptr<Notification> notification = CreateSimpleNotification("n");
  Notification notification_copy = *notification;
  message_center()->AddNotification(std::move(notification));
  message_center()->ClickOnNotification("n");
  message_center()->SetVisibility(VISIBILITY_MESSAGE_CENTER);
  observer.reset_logs();
  notification_copy.set_title(u"title2");
  message_center()->UpdateNotification(
      notification_copy.id(),
      std::make_unique<Notification>(notification_copy));

  // Expect that the notification update is done.
  EXPECT_NE("", observer.log("n"));

  message_center()->SetVisibility(VISIBILITY_TRANSIENT);
  EXPECT_EQ("update-n", observer.log("n"));
}

TEST_F(MessageCenterImplTest,
       UpdateNonProgressToProgressNotificationWhenCenterVisible) {
  TestAddObserver observer(message_center());

  // Add a non-progress notification and change the type to progress while the
  // message center is visible.
  std::unique_ptr<Notification> notification = CreateSimpleNotification("n");
  Notification notification_copy = *notification;
  message_center()->AddNotification(std::move(notification));
  message_center()->ClickOnNotification("n");
  message_center()->SetVisibility(VISIBILITY_MESSAGE_CENTER);
  observer.reset_logs();
  notification_copy.set_type(NOTIFICATION_TYPE_PROGRESS);
  message_center()->UpdateNotification(
      "n", std::make_unique<Notification>(notification_copy));

  // Expect that the notification update is done.
  EXPECT_NE("", observer.log("n"));

  message_center()->SetVisibility(VISIBILITY_TRANSIENT);
  EXPECT_EQ("update-n", observer.log("n"));
}

TEST_F(MessageCenterImplTest, Click) {
  TestAddObserver observer(message_center());
  std::string id("n");

  std::unique_ptr<Notification> notification = CreateSimpleNotification(id);
  message_center()->AddNotification(std::move(notification));
  message_center()->ClickOnNotification(id);

  EXPECT_EQ("Click_", GetDelegate(id)->log());
}

TEST_F(MessageCenterImplTest, ButtonClick) {
  TestAddObserver observer(message_center());
  std::string id("n");

  std::unique_ptr<Notification> notification = CreateSimpleNotification(id);
  message_center()->AddNotification(std::move(notification));
  message_center()->ClickOnNotificationButton(id, 1);

  EXPECT_EQ("ButtonClick_1_", GetDelegate(id)->log());
}

TEST_F(MessageCenterImplTest, RemoveAfterClick) {
  const std::string id = "n";
  RichNotificationData notification_data;

  ASSERT_FALSE(message_center()->FindNotificationById(id));
  message_center()->AddNotification(
      CreateSimpleNotificationWithOptionalFields(id, notification_data));
  ASSERT_TRUE(message_center()->FindNotificationById(id));

  message_center()->ClickOnNotification(id);
  EXPECT_TRUE(message_center()->FindNotificationById(id));

  message_center()->RemoveNotification(id, false);
  ASSERT_FALSE(message_center()->FindNotificationById(id));

  notification_data.remove_on_click = true;

  message_center()->AddNotification(
      CreateSimpleNotificationWithOptionalFields(id, notification_data));
  ASSERT_TRUE(message_center()->FindNotificationById(id));

  message_center()->ClickOnNotification(id);
  EXPECT_FALSE(message_center()->FindNotificationById(id));
}

TEST_F(MessageCenterImplTest, RemoveAfterButtonClick) {
  const std::string id = "n";
  RichNotificationData notification_data;
  notification_data.buttons.emplace_back(u"button");

  message_center()->AddNotification(
      CreateSimpleNotificationWithOptionalFields(id, notification_data));
  ASSERT_TRUE(message_center()->FindNotificationById(id));

  message_center()->ClickOnNotificationButton(id, 0);
  EXPECT_TRUE(message_center()->FindNotificationById(id));

  message_center()->RemoveNotification(id, false);
  ASSERT_FALSE(message_center()->FindNotificationById(id));

  notification_data.remove_on_click = true;

  message_center()->AddNotification(
      CreateSimpleNotificationWithOptionalFields(id, notification_data));
  ASSERT_TRUE(message_center()->FindNotificationById(id));

  message_center()->ClickOnNotificationButton(id, 0);
  EXPECT_FALSE(message_center()->FindNotificationById(id));
}

TEST_F(MessageCenterImplTest, ButtonClickWithReply) {
  TestAddObserver observer(message_center());
  std::string id("n");

  std::unique_ptr<Notification> notification = CreateSimpleNotification(id);
  message_center()->AddNotification(std::move(notification));
  message_center()->ClickOnNotificationButtonWithReply(id, 1, u"REPLYTEXT");

  EXPECT_EQ("ReplyButtonClick_1_REPLYTEXT_", GetDelegate(id)->log());
}

TEST_F(MessageCenterImplTest, Unlock) {
  lock_screen_controller()->set_is_screen_locked(true);

  EXPECT_FALSE(lock_screen_controller()->HasPendingCallback());
  EXPECT_TRUE(lock_screen_controller()->IsScreenLocked());

  lock_screen_controller()->SimulateUnlock();

  EXPECT_FALSE(lock_screen_controller()->HasPendingCallback());
  EXPECT_FALSE(lock_screen_controller()->IsScreenLocked());

  lock_screen_controller()->set_is_screen_locked(true);

  EXPECT_FALSE(lock_screen_controller()->HasPendingCallback());
  EXPECT_TRUE(lock_screen_controller()->IsScreenLocked());

  lock_screen_controller()->SimulateUnlock();

  EXPECT_FALSE(lock_screen_controller()->HasPendingCallback());
  EXPECT_FALSE(lock_screen_controller()->IsScreenLocked());
}

TEST_F(MessageCenterImplTest, ClickOnLockScreen) {
  lock_screen_controller()->set_is_screen_locked(true);

  TestAddObserver observer(message_center());
  std::string id("n");

  std::unique_ptr<Notification> notification = CreateSimpleNotification(id);
  message_center()->AddNotification(std::move(notification));
  message_center()->ClickOnNotification(id);

  EXPECT_EQ("", GetDelegate(id)->log());
  EXPECT_TRUE(lock_screen_controller()->HasPendingCallback());
  EXPECT_TRUE(lock_screen_controller()->IsScreenLocked());

  lock_screen_controller()->SimulateUnlock();

  EXPECT_EQ("Click_", GetDelegate(id)->log());
  EXPECT_FALSE(lock_screen_controller()->HasPendingCallback());
  EXPECT_FALSE(lock_screen_controller()->IsScreenLocked());
}

TEST_F(MessageCenterImplTest, ClickAndCancelOnLockScreen) {
  lock_screen_controller()->set_is_screen_locked(true);

  TestAddObserver observer(message_center());
  std::string id("n");

  std::unique_ptr<Notification> notification = CreateSimpleNotification(id);
  message_center()->AddNotification(std::move(notification));
  message_center()->ClickOnNotification(id);

  EXPECT_EQ("", GetDelegate(id)->log());
  EXPECT_TRUE(lock_screen_controller()->HasPendingCallback());
  EXPECT_TRUE(lock_screen_controller()->IsScreenLocked());

  lock_screen_controller()->CancelClick();

  EXPECT_EQ("", GetDelegate(id)->log());
  EXPECT_FALSE(lock_screen_controller()->HasPendingCallback());
  EXPECT_TRUE(lock_screen_controller()->IsScreenLocked());

  lock_screen_controller()->SimulateUnlock();

  EXPECT_EQ("", GetDelegate(id)->log());
  EXPECT_FALSE(lock_screen_controller()->HasPendingCallback());
  EXPECT_FALSE(lock_screen_controller()->IsScreenLocked());
}

TEST_F(MessageCenterImplTest, ButtonClickOnLockScreen) {
  lock_screen_controller()->set_is_screen_locked(true);

  TestAddObserver observer(message_center());
  std::string id("n");

  std::unique_ptr<Notification> notification = CreateSimpleNotification(id);
  message_center()->AddNotification(std::move(notification));
  message_center()->ClickOnNotificationButton(id, 1);

  EXPECT_EQ("", GetDelegate(id)->log());
  EXPECT_TRUE(lock_screen_controller()->HasPendingCallback());
  EXPECT_TRUE(lock_screen_controller()->IsScreenLocked());

  lock_screen_controller()->SimulateUnlock();

  EXPECT_EQ("ButtonClick_1_", GetDelegate(id)->log());
  EXPECT_FALSE(lock_screen_controller()->HasPendingCallback());
  EXPECT_FALSE(lock_screen_controller()->IsScreenLocked());
}

TEST_F(MessageCenterImplTest, ButtonClickWithReplyOnLockScreen) {
  lock_screen_controller()->set_is_screen_locked(true);

  TestAddObserver observer(message_center());
  std::string id("n");

  std::unique_ptr<Notification> notification = CreateSimpleNotification(id);
  message_center()->AddNotification(std::move(notification));
  message_center()->ClickOnNotificationButtonWithReply(id, 1, u"REPLYTEXT");

  EXPECT_EQ("", GetDelegate(id)->log());
  EXPECT_TRUE(lock_screen_controller()->HasPendingCallback());
  EXPECT_TRUE(lock_screen_controller()->IsScreenLocked());

  lock_screen_controller()->SimulateUnlock();

  EXPECT_EQ("ReplyButtonClick_1_REPLYTEXT_", GetDelegate(id)->log());
  EXPECT_FALSE(lock_screen_controller()->HasPendingCallback());
  EXPECT_FALSE(lock_screen_controller()->IsScreenLocked());
}

}  // namespace internal
}  // namespace message_center
