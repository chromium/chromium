// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_idle_notify.h"

#include <ext-idle-notify-v1-server-protocol.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/wayland_connection_test_api.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

namespace ui {
namespace {

// Retain notification wrappers to distinguish destroyed resources from requests
// that were never made.
class TestIdleNotifier : public wl::GlobalObject {
 public:
  explicit TestIdleNotifier(uint32_t version)
      : GlobalObject(&ext_idle_notifier_v1_interface,
                     &kImplementation,
                     version) {}

  ~TestIdleNotifier() override {
    if (resource()) {
      wl_resource_destroy(resource());
    }
  }

  wl_resource* notification() const {
    return notifications_.empty() ? nullptr : notifications_.back()->resource();
  }
  size_t notification_count() const { return notifications_.size(); }
  uint32_t timeout_ms() const { return timeout_ms_; }
  uint32_t seat_id() const { return seat_id_; }

 private:
  static void GetIdleNotification(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  uint32_t timeout,
                                  wl_resource* seat) {
    auto* self = wl::GetUserDataAs<TestIdleNotifier>(resource);
    self->timeout_ms_ = timeout;
    self->seat_id_ = wl_resource_get_id(seat);
    auto* notification =
        wl_resource_create(client, &ext_idle_notification_v1_interface,
                           wl_resource_get_version(resource), id);
    ASSERT_TRUE(notification);
    auto object = std::make_unique<wl::ServerObject>(notification);
    static constexpr struct ext_idle_notification_v1_interface
        kNotificationImplementation = {.destroy = &wl::DestroyResource};
    wl::SetImplementationUnretained(notification, &kNotificationImplementation,
                                    object.get());
    self->notifications_.push_back(std::move(object));
  }

  static constexpr struct ext_idle_notifier_v1_interface kImplementation = {
      .destroy = &wl::DestroyResource,
      .get_idle_notification = &GetIdleNotification,
      .get_input_idle_notification = nullptr,
  };

  std::vector<std::unique_ptr<wl::ServerObject>> notifications_;
  uint32_t timeout_ms_ = 0;
  uint32_t seat_id_ = 0;
};

class WaylandIdleNotifyTest : public WaylandTestSimple {
 public:
  void TearDown() override {
    // Destroy client proxies and process their requests before deleting the
    // globals whose addresses are stored in server resources' user data.
    WaylandConnectionTestApi(connection_.get()).TakeExtIdleNotifier().reset();
    PostToServerAndWait([this] { globals_.clear(); });
    WaylandTestSimple::TearDown();
  }

 protected:
  void AdvertiseNotifier(uint32_t version = 1) {
    PostToServerAndWait([this, version](wl::TestWaylandServerThread* server) {
      auto global = std::make_unique<TestIdleNotifier>(version);
      ASSERT_TRUE(global->Initialize(server->display()));
      globals_.push_back(std::move(global));
    });
    // Ensure the global event is dispatched so the client can bind the
    // notifier.
    connection_->RoundTripQueue();
  }

  void SendIdled() {
    PostToServerAndWait([this] {
      ASSERT_TRUE(global()->notification());
      ext_idle_notification_v1_send_idled(global()->notification());
    });
    connection_->RoundTripQueue();
  }

  void SendResumed() {
    PostToServerAndWait([this] {
      ASSERT_TRUE(global()->notification());
      ext_idle_notification_v1_send_resumed(global()->notification());
    });
    connection_->RoundTripQueue();
  }

  ExtIdleNotifier* notifier() { return connection_->ext_idle_notifier(); }

  // Call only on the server thread.
  TestIdleNotifier* global() { return globals_.front().get(); }

  std::vector<std::unique_ptr<TestIdleNotifier>> globals_;
};

TEST_F(WaylandIdleNotifyTest, UnavailableWithoutGlobal) {
  EXPECT_FALSE(notifier());
}

TEST_F(WaylandIdleNotifyTest, RejectsUnsupportedVersion) {
  // Version zero cannot be advertised by a conforming compositor. Exercise the
  // version guard directly; it must return before trying to use the registry.
  ExtIdleNotifier::Instantiate(connection_.get(), nullptr, 0,
                               ExtIdleNotifier::kInterfaceName, 0);
  EXPECT_FALSE(notifier());

  AdvertiseNotifier();
  EXPECT_TRUE(notifier());
}

TEST_F(WaylandIdleNotifyTest, BindsSupportedVersionFromNewerGlobal) {
  AdvertiseNotifier(2);
  ASSERT_TRUE(notifier());
  PostToServerAndWait([this] {
    ASSERT_TRUE(global()->resource());
    EXPECT_EQ(1, wl_resource_get_version(global()->resource()));
    EXPECT_EQ(0u, global()->notification_count());
  });
}

TEST_F(WaylandIdleNotifyTest, IgnoresDuplicateGlobal) {
  AdvertiseNotifier();
  auto* original = notifier();
  ASSERT_TRUE(original);
  EXPECT_EQ(base::TimeDelta(), original->GetIdleTime());

  AdvertiseNotifier();
  EXPECT_EQ(original, notifier());
  PostToServerAndWait([this] {
    EXPECT_EQ(1u, global()->notification_count());
    EXPECT_FALSE(globals_.back()->resource());
  });
  SendIdled();
  EXPECT_EQ(base::Seconds(5), notifier()->GetIdleTime());
}

TEST_F(WaylandIdleNotifyTest, UnavailableWithoutSeat) {
  WaylandConnection connection_without_seat;
  ASSERT_FALSE(connection_without_seat.seat());
  ExtIdleNotifier idle_notifier(nullptr, &connection_without_seat);
  EXPECT_EQ(std::nullopt, idle_notifier.GetIdleTime());
  task_environment_.AdvanceClock(base::Seconds(10));
  EXPECT_EQ(std::nullopt, idle_notifier.GetIdleTime());
}

TEST_F(WaylandIdleNotifyTest, CreatesNotificationLazilyAndReusesIt) {
  AdvertiseNotifier();
  ASSERT_TRUE(notifier());
  PostToServerAndWait([this] {
    ASSERT_TRUE(global()->resource());
    EXPECT_EQ(1, wl_resource_get_version(global()->resource()));
    EXPECT_EQ(0u, global()->notification_count());
  });

  EXPECT_EQ(base::TimeDelta(), notifier()->GetIdleTime());
  EXPECT_EQ(base::TimeDelta(), notifier()->GetIdleTime());
  PostToServerAndWait([this](wl::TestWaylandServerThread* server) {
    ASSERT_TRUE(global()->notification());
    EXPECT_EQ(1u, global()->notification_count());
    EXPECT_EQ(5000u, global()->timeout_ms());
    EXPECT_EQ(wl_resource_get_id(server->seat()->resource()),
              global()->seat_id());
  });

  task_environment_.AdvanceClock(base::Seconds(30));
  EXPECT_EQ(base::TimeDelta(), notifier()->GetIdleTime());
  PostToServerAndWait(
      [this] { EXPECT_EQ(1u, global()->notification_count()); });
}

TEST_F(WaylandIdleNotifyTest, IdleTimeIncludesThresholdAndElapsedTime) {
  AdvertiseNotifier();
  ASSERT_TRUE(notifier());
  EXPECT_EQ(base::TimeDelta(), notifier()->GetIdleTime());

  task_environment_.AdvanceClock(base::Seconds(30));
  SendIdled();
  EXPECT_EQ(base::Seconds(5), notifier()->GetIdleTime());
  task_environment_.AdvanceClock(base::Milliseconds(1234));
  EXPECT_EQ(base::Milliseconds(6234), notifier()->GetIdleTime());
  task_environment_.AdvanceClock(base::Hours(1));
  EXPECT_EQ(base::Hours(1) + base::Milliseconds(6234),
            notifier()->GetIdleTime());
  PostToServerAndWait(
      [this] { EXPECT_EQ(1u, global()->notification_count()); });
}

TEST_F(WaylandIdleNotifyTest, ResumedResetsIdleTimeAndNextIdleStartsFresh) {
  AdvertiseNotifier();
  ASSERT_TRUE(notifier());
  EXPECT_EQ(base::TimeDelta(), notifier()->GetIdleTime());

  SendIdled();
  task_environment_.AdvanceClock(base::Milliseconds(1234));
  EXPECT_EQ(base::Milliseconds(6234), notifier()->GetIdleTime());
  SendResumed();
  EXPECT_EQ(base::TimeDelta(), notifier()->GetIdleTime());
  task_environment_.AdvanceClock(base::Seconds(10));
  EXPECT_EQ(base::TimeDelta(), notifier()->GetIdleTime());

  SendIdled();
  EXPECT_EQ(base::Seconds(5), notifier()->GetIdleTime());
  task_environment_.AdvanceClock(base::Milliseconds(250));
  EXPECT_EQ(base::Milliseconds(5250), notifier()->GetIdleTime());
  SendResumed();
  EXPECT_EQ(base::TimeDelta(), notifier()->GetIdleTime());
  PostToServerAndWait(
      [this] { EXPECT_EQ(1u, global()->notification_count()); });
}

TEST_F(WaylandIdleNotifyTest, DestroysNotifierWithoutNotification) {
  AdvertiseNotifier();
  ASSERT_TRUE(notifier());
  WaylandConnectionTestApi(connection_.get()).TakeExtIdleNotifier().reset();
  PostToServerAndWait([this] {
    EXPECT_FALSE(global()->resource());
    EXPECT_EQ(0u, global()->notification_count());
  });
}

TEST_F(WaylandIdleNotifyTest, DestroysNotifierAndNotification) {
  AdvertiseNotifier();
  ASSERT_TRUE(notifier());
  EXPECT_EQ(base::TimeDelta(), notifier()->GetIdleTime());
  SendIdled();

  WaylandConnectionTestApi(connection_.get()).TakeExtIdleNotifier().reset();
  PostToServerAndWait([this] {
    EXPECT_FALSE(global()->resource());
    EXPECT_EQ(1u, global()->notification_count());
    EXPECT_FALSE(global()->notification());
  });
}

}  // namespace
}  // namespace ui
