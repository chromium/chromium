// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/org_gnome_mutter_idle_monitor.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace {

constexpr char kInterface[] = "org.gnome.Mutter.IdleMonitor";
constexpr char kObjectPath[] = "/org/gnome/Mutter/IdleMonitor/Core";
constexpr char kSignalWatchFired[] = "WatchFired";

constexpr base::TimeDelta kIdleThreshold = base::Seconds(5);
constexpr uint64_t kIdleThresholdMs = kIdleThreshold.InMilliseconds();

class OrgGnomeMutterIdleMonitorTest : public testing::Test {
 public:
  OrgGnomeMutterIdleMonitorTest() = default;
  ~OrgGnomeMutterIdleMonitorTest() override = default;

  void SetUp() override {
    mock_bus_ = base::MakeRefCounted<testing::NiceMock<dbus::MockBus>>(
        dbus::Bus::Options());

    ON_CALL(*mock_bus_, GetDBusTaskRunner())
        .WillByDefault(
            testing::Return(task_environment_.GetMainThreadTaskRunner().get()));

    mock_dbus_proxy_ =
        base::MakeRefCounted<testing::NiceMock<dbus::MockObjectProxy>>(
            mock_bus_.get(), DBUS_SERVICE_DBUS,
            dbus::ObjectPath(DBUS_PATH_DBUS));
    mock_mutter_proxy_ =
        base::MakeRefCounted<testing::NiceMock<dbus::MockObjectProxy>>(
            mock_bus_.get(), kInterface, dbus::ObjectPath(kObjectPath));

    ON_CALL(*mock_bus_, GetObjectProxy)
        .WillByDefault(
            [this](std::string_view service_name,
                   const dbus::ObjectPath& path) -> dbus::ObjectProxy* {
              if (service_name == DBUS_SERVICE_DBUS) {
                return mock_dbus_proxy_.get();
              }
              if (service_name == kInterface) {
                return mock_mutter_proxy_.get();
              }
              return nullptr;
            });

    ON_CALL(*mock_dbus_proxy_, CallMethod)
        .WillByDefault([this](dbus::MethodCall* method_call, int timeout_ms,
                              dbus::ObjectProxy::ResponseCallback callback) {
          std::unique_ptr<dbus::Response> response;
          if (method_call->GetMember() == "NameHasOwner" &&
              name_has_owner_response_) {
            response = dbus::Response::CreateEmpty();
            dbus::MessageWriter writer(response.get());
            writer.AppendBool(name_has_owner_result_);
          }
          PostResponse(std::move(callback), std::move(response));
        });

    ON_CALL(*mock_mutter_proxy_, ConnectToSignal(kInterface, kSignalWatchFired,
                                                 testing::_, testing::_))
        .WillByDefault(
            [this](
                const std::string& interface_name,
                const std::string& signal_name,
                dbus::ObjectProxy::SignalCallback signal_callback,
                dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
              watch_fired_callback_ = signal_callback;
              task_environment_.GetMainThreadTaskRunner()->PostTask(
                  FROM_HERE, base::BindOnce(std::move(on_connected_callback),
                                            interface_name, signal_name,
                                            connect_signal_succeed_));
            });

    ON_CALL(*mock_mutter_proxy_, CallMethod)
        .WillByDefault([this](dbus::MethodCall* method_call, int timeout_ms,
                              dbus::ObjectProxy::ResponseCallback callback) {
          std::unique_ptr<dbus::Response> response;
          if (method_call->GetMember() == "AddIdleWatch") {
            add_idle_watch_call_count_++;
            if (idle_watch_response_.has_value()) {
              response = dbus::Response::CreateEmpty();
              dbus::MessageWriter writer(response.get());
              writer.AppendUint32(idle_watch_response_.value());
            }
          } else if (method_call->GetMember() == "GetIdletime") {
            get_idletime_call_count_++;
            if (idletime_response_.has_value()) {
              response = dbus::Response::CreateEmpty();
              dbus::MessageWriter writer(response.get());
              writer.AppendUint64(idletime_response_.value());
            }
          } else if (method_call->GetMember() == "AddUserActiveWatch") {
            add_user_active_watch_call_count_++;
            if (user_active_watch_response_.has_value()) {
              response = dbus::Response::CreateEmpty();
              dbus::MessageWriter writer(response.get());
              writer.AppendUint32(user_active_watch_response_.value());
            }
          }
          PostResponse(std::move(callback), std::move(response));
        });
  }

  void PostResponse(dbus::ObjectProxy::ResponseCallback callback,
                    std::unique_ptr<dbus::Response> response) {
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](dbus::ObjectProxy::ResponseCallback cb,
                          std::unique_ptr<dbus::Response> resp) {
                         std::move(cb).Run(resp.get());
                       },
                       std::move(callback), std::move(response)));
  }

  void InitializeMonitor() {
    add_idle_watch_call_count_ = 0;
    get_idletime_call_count_ = 0;
    add_user_active_watch_call_count_ = 0;
    idle_monitor_ = std::make_unique<OrgGnomeMutterIdleMonitor>(mock_bus_);
    EXPECT_EQ(idle_monitor_->GetIdleTime(), base::Seconds(0));
    task_environment_.FastForwardBy(base::TimeDelta());
  }

  void InitializeActive() {
    idletime_response_ = 0;
    InitializeMonitor();
    EXPECT_EQ(add_idle_watch_call_count_, 1);
    EXPECT_EQ(get_idletime_call_count_, 1);
    EXPECT_EQ(add_user_active_watch_call_count_, 0);
  }

  void InitializeIdle(base::TimeDelta initial_idle = kIdleThreshold +
                                                     base::Seconds(1),
                      uint32_t active_watch_id = 42) {
    idletime_response_ = initial_idle.InMilliseconds();
    user_active_watch_response_ = active_watch_id;
    InitializeMonitor();
    EXPECT_EQ(add_idle_watch_call_count_, 1);
    EXPECT_EQ(get_idletime_call_count_, 1);
    EXPECT_EQ(add_user_active_watch_call_count_, 1);
    EXPECT_EQ(idle_monitor_->GetIdleTime(), initial_idle);
  }

  void EmitWatchFired(uint32_t watch_id) {
    ASSERT_TRUE(watch_fired_callback_);
    dbus::Signal signal(kInterface, kSignalWatchFired);
    dbus::MessageWriter writer(&signal);
    writer.AppendUint32(watch_id);
    watch_fired_callback_.Run(&signal);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<testing::NiceMock<dbus::MockBus>> mock_bus_;
  scoped_refptr<testing::NiceMock<dbus::MockObjectProxy>> mock_dbus_proxy_;
  scoped_refptr<testing::NiceMock<dbus::MockObjectProxy>> mock_mutter_proxy_;

  bool name_has_owner_response_ = true;
  bool name_has_owner_result_ = true;
  bool connect_signal_succeed_ = true;
  std::optional<uint32_t> idle_watch_response_ = 10;
  std::optional<uint64_t> idletime_response_ = 0;
  std::optional<uint32_t> user_active_watch_response_ = 100;

  int add_idle_watch_call_count_ = 0;
  int get_idletime_call_count_ = 0;
  int add_user_active_watch_call_count_ = 0;

  dbus::ObjectProxy::SignalCallback watch_fired_callback_;
  std::unique_ptr<OrgGnomeMutterIdleMonitor> idle_monitor_;
};

TEST_F(OrgGnomeMutterIdleMonitorTest, UninhibitedIdleWatchTransitionAndWakeup) {
  idle_watch_response_ = 10;
  user_active_watch_response_ = 42;
  InitializeActive();

  EmitWatchFired(10);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), kIdleThreshold);
  EXPECT_EQ(get_idletime_call_count_, 1);
  EXPECT_EQ(add_user_active_watch_call_count_, 1);
  task_environment_.FastForwardBy(base::TimeDelta());

  task_environment_.FastForwardBy(2 * kIdleThreshold);
  EXPECT_EQ(get_idletime_call_count_, 1);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), 3 * kIdleThreshold);

  EmitWatchFired(42);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), base::Seconds(0));
  EXPECT_EQ(get_idletime_call_count_, 1);

  // An in-flight GetIdletime reply (< kIdleThreshold) that raced with
  // WatchFired(idle_watch_id) must not clear the idle state set by the watch.
  idletime_response_ = kIdleThresholdMs - 10;
  task_environment_.FastForwardBy(kIdleThreshold);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), base::Seconds(0));
  EmitWatchFired(10);
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(idle_monitor_->GetIdleTime(), kIdleThreshold);
  EXPECT_EQ(add_user_active_watch_call_count_, 2);
}

TEST_F(OrgGnomeMutterIdleMonitorTest,
       InhibitedSessionPollsOnDemandAndTransitionsToIdle) {
  idle_watch_response_ = 10;
  user_active_watch_response_ = 42;
  InitializeActive();

  task_environment_.FastForwardBy(2 * kIdleThreshold);
  EXPECT_EQ(get_idletime_call_count_, 1);

  idletime_response_ = kIdleThresholdMs;
  EXPECT_EQ(idle_monitor_->GetIdleTime(), base::Seconds(0));
  EXPECT_EQ(get_idletime_call_count_, 2);

  // An overlapping query while the D-Bus request is in flight is ignored.
  EXPECT_EQ(idle_monitor_->GetIdleTime(), base::Seconds(0));
  EXPECT_EQ(get_idletime_call_count_, 2);

  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(add_user_active_watch_call_count_, 1);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), kIdleThreshold);

  task_environment_.FastForwardBy(2 * kIdleThreshold);
  EXPECT_EQ(get_idletime_call_count_, 2);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), 3 * kIdleThreshold);

  // If the inhibitor is released while already idle, a subsequent idle watch
  // signal must not overwrite the existing idle timestamp or re-arm the watch.
  EmitWatchFired(10);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), 3 * kIdleThreshold);
  EXPECT_EQ(add_user_active_watch_call_count_, 1);
}

TEST_F(OrgGnomeMutterIdleMonitorTest,
       ActiveGetIdletimeQueriesAreThrottledByThreshold) {
  InitializeActive();

  task_environment_.FastForwardBy(kIdleThreshold / 2);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), base::Seconds(0));
  EXPECT_EQ(get_idletime_call_count_, 1);

  idletime_response_ = kIdleThresholdMs - 200;
  task_environment_.FastForwardBy(kIdleThreshold - kIdleThreshold / 2);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), base::Seconds(0));
  EXPECT_EQ(get_idletime_call_count_, 2);
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(add_user_active_watch_call_count_, 0);

  idletime_response_ = kIdleThresholdMs;
  user_active_watch_response_ = 77;
  task_environment_.FastForwardBy(kIdleThreshold);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), base::Seconds(0));
  EXPECT_EQ(get_idletime_call_count_, 3);
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(add_user_active_watch_call_count_, 1);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), kIdleThreshold);
}

TEST_F(OrgGnomeMutterIdleMonitorTest, StartupWhenAlreadyIdle) {
  const base::TimeDelta initial_idle = kIdleThreshold + base::Seconds(1);
  InitializeIdle(initial_idle);

  task_environment_.FastForwardBy(2 * kIdleThreshold);
  EXPECT_EQ(get_idletime_call_count_, 1);
  EXPECT_EQ(add_user_active_watch_call_count_, 1);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), initial_idle + 2 * kIdleThreshold);
}

TEST_F(OrgGnomeMutterIdleMonitorTest, IdleToActiveToIdleRearmsActiveWatch) {
  idle_watch_response_ = 10;
  InitializeIdle(kIdleThreshold + base::Seconds(1), /*active_watch_id=*/42);

  EmitWatchFired(42);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), base::Seconds(0));

  user_active_watch_response_ = 88;
  EmitWatchFired(10);
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(add_user_active_watch_call_count_, 2);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), kIdleThreshold);

  EmitWatchFired(88);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), base::Seconds(0));
}

TEST_F(OrgGnomeMutterIdleMonitorTest, SpuriousWatchFiredSignalsAreIgnored) {
  const base::TimeDelta initial_idle = kIdleThreshold + base::Seconds(1);
  InitializeIdle(initial_idle, /*active_watch_id=*/42);

  EmitWatchFired(999);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), initial_idle);

  EmitWatchFired(42);
  EmitWatchFired(0);
  EmitWatchFired(42);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), base::Seconds(0));
  EXPECT_EQ(get_idletime_call_count_, 1);
  EXPECT_EQ(add_user_active_watch_call_count_, 1);
}

TEST_F(OrgGnomeMutterIdleMonitorTest, DbusFailuresTransitionToUnavailable) {
  name_has_owner_result_ = false;
  InitializeMonitor();
  EXPECT_EQ(idle_monitor_->GetIdleTime(), std::nullopt);

  name_has_owner_result_ = true;
  connect_signal_succeed_ = false;
  InitializeMonitor();
  EXPECT_EQ(idle_monitor_->GetIdleTime(), std::nullopt);

  connect_signal_succeed_ = true;
  idle_watch_response_ = std::nullopt;
  InitializeMonitor();
  EXPECT_EQ(idle_monitor_->GetIdleTime(), std::nullopt);

  idle_watch_response_ = 10;
  idletime_response_ = std::nullopt;
  InitializeMonitor();
  EXPECT_EQ(idle_monitor_->GetIdleTime(), std::nullopt);

  idletime_response_ = kIdleThresholdMs;
  user_active_watch_response_ = std::nullopt;
  InitializeMonitor();
  EXPECT_EQ(idle_monitor_->GetIdleTime(), std::nullopt);

  user_active_watch_response_ = 42;
  InitializeActive();
  idletime_response_ = std::nullopt;
  task_environment_.FastForwardBy(kIdleThreshold);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), base::Seconds(0));
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(idle_monitor_->GetIdleTime(), std::nullopt);
}

TEST_F(OrgGnomeMutterIdleMonitorTest, DestroyWhileCallPending) {
  idle_monitor_ = std::make_unique<OrgGnomeMutterIdleMonitor>(mock_bus_);
  EXPECT_EQ(idle_monitor_->GetIdleTime(), base::Seconds(0));
  idle_monitor_.reset();
  task_environment_.FastForwardBy(kIdleThreshold);
  EXPECT_EQ(get_idletime_call_count_, 0);
}

}  // namespace
}  // namespace ui
