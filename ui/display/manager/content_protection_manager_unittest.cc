// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/display/manager/content_protection_manager.h"

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/test/action_logger_util.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/manager/test/test_display_layout_manager.h"
#include "ui/display/manager/test/test_native_display_delegate.h"

namespace display::test {

namespace {

constexpr int64_t kDisplayIds[] = {123, 234, 345, 456};
const DisplayMode kDisplayMode({1366, 768}, false, 60.0f);

}  // namespace

using SecurityChanges = base::flat_map<int64_t, bool>;

class TestObserver : public ContentProtectionManager::Observer {
 public:
  explicit TestObserver(ContentProtectionManager* manager) : manager_(manager) {
    manager_->AddObserver(this);
  }

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override { manager_->RemoveObserver(this); }

  const SecurityChanges& security_changes() const { return security_changes_; }

  void Reset() { security_changes_.clear(); }

 private:
  void OnDisplaySecurityMaybeChanged(int64_t display_id, bool secure) override {
    security_changes_.emplace(display_id, secure);
  }

  const raw_ptr<ContentProtectionManager> manager_;
  SecurityChanges security_changes_;
};

class ContentProtectionManagerTest : public testing::Test {
 public:
  ContentProtectionManagerTest() = default;

  ContentProtectionManagerTest(const ContentProtectionManagerTest&) = delete;
  ContentProtectionManagerTest& operator=(const ContentProtectionManagerTest&) =
      delete;

  ~ContentProtectionManagerTest() override = default;

  void SetUp() override {
    manager_.set_native_display_delegate(&native_display_delegate_);

    DisplayConnectionType conn_types[] = {
        DISPLAY_CONNECTION_TYPE_INTERNAL, DISPLAY_CONNECTION_TYPE_HDMI,
        DISPLAY_CONNECTION_TYPE_VGA, DISPLAY_CONNECTION_TYPE_HDMI};
    for (size_t i = 0; i < std::size(kDisplayIds); ++i) {
      displays_[i] = FakeDisplaySnapshot::Builder()
                         .SetId(kDisplayIds[i])
                         .SetType(conn_types[i])
                         .SetCurrentMode(kDisplayMode.Clone())
                         .Build();
    }

    UpdateDisplays(2);
  }

  void ApplyContentProtectionCallback(bool success) {
    apply_content_protection_success_ = success;
    apply_content_protection_call_count_++;
  }

  void QueryContentProtectionCallback(bool success,
                                      uint32_t connection_mask,
                                      uint32_t protection_mask) {
    query_content_protection_success_ = success;
    query_content_protection_call_count_++;

    connection_mask_ = connection_mask;
    protection_mask_ = protection_mask;
  }

 protected:
  void UpdateDisplays(size_t count) {
    ASSERT_LE(count, std::size(displays_));

    std::vector<std::unique_ptr<DisplaySnapshot>> displays;
    for (size_t i = 0; i < count; ++i)
      displays.push_back(displays_[i]->Clone());

    native_display_delegate_.SetOutputs(std::move(displays));
    layout_manager_.set_displays(native_display_delegate_.GetOutputs());
  }

  void TriggerDisplayConfiguration() {
    manager_.OnDisplayConfigurationChanged(layout_manager_.GetDisplayStates());
  }

  bool TriggerDisplaySecurityTimeout() {
    return manager_.TriggerDisplaySecurityTimeoutForTesting();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestDisplayLayoutManager layout_manager_{{}, MULTIPLE_DISPLAY_STATE_INVALID};

  ActionLogger log_;
  TestNativeDisplayDelegate native_display_delegate_{&log_};

  ContentProtectionManager manager_{&layout_manager_,
                                    base::BindRepeating([] { return false; })};

  bool apply_content_protection_success_ = false;
  int apply_content_protection_call_count_ = 0;

  bool query_content_protection_success_ = false;
  int query_content_protection_call_count_ = 0;
  uint32_t connection_mask_ = DISPLAY_CONNECTION_TYPE_NONE;
  uint32_t protection_mask_ = CONTENT_PROTECTION_METHOD_NONE;

  std::unique_ptr<DisplaySnapshot> displays_[std::size(kDisplayIds)];
};

TEST_F(ContentProtectionManagerTest, Basic) {
  UpdateDisplays(1);

  auto id = manager_.RegisterClient();
  EXPECT_TRUE(id);

  manager_.QueryContentProtection(
      id, displays_[0]->display_id(),
      base::BindOnce(
          &ContentProtectionManagerTest::QueryContentProtectionCallback,
          base::Unretained(this)));

  EXPECT_EQ(1, query_content_protection_call_count_);
  EXPECT_TRUE(query_content_protection_success_);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_INTERNAL, connection_mask_);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_NONE, protection_mask_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());

  UpdateDisplays(2);

  manager_.QueryContentProtection(
      id, displays_[1]->display_id(),
      base::BindOnce(
          &ContentProtectionManagerTest::QueryContentProtectionCallback,
          base::Unretained(this)));

  EXPECT_EQ(2, query_content_protection_call_count_);
  EXPECT_TRUE(query_content_protection_success_);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_HDMI, connection_mask_);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_NONE, protection_mask_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());

  manager_.ApplyContentProtection(
      id, displays_[1]->display_id(), CONTENT_PROTECTION_METHOD_HDCP,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  EXPECT_EQ(1, apply_content_protection_call_count_);
  EXPECT_TRUE(apply_content_protection_success_);
  EXPECT_EQ(GetSetHDCPStateAction(kDisplayIds[1], HDCP_STATE_DESIRED,
                                  CONTENT_PROTECTION_METHOD_HDCP),
            log_.GetActionsAndClear());

  EXPECT_EQ(HDCP_STATE_ENABLED, native_display_delegate_.hdcp_state());

  manager_.QueryContentProtection(
      id, displays_[1]->display_id(),
      base::BindOnce(
          &ContentProtectionManagerTest::QueryContentProtectionCallback,
          base::Unretained(this)));

  EXPECT_EQ(3, query_content_protection_call_count_);
  EXPECT_TRUE(query_content_protection_success_);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_HDMI, connection_mask_);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_HDCP, protection_mask_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());

  // Requests on invalid display should fail.
  constexpr int64_t kInvalidDisplayId = -999;
  manager_.QueryContentProtection(
      id, kInvalidDisplayId,
      base::BindOnce(
          &ContentProtectionManagerTest::QueryContentProtectionCallback,
          base::Unretained(this)));

  manager_.ApplyContentProtection(
      id, kInvalidDisplayId, CONTENT_PROTECTION_METHOD_HDCP,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  EXPECT_EQ(4, query_content_protection_call_count_);
  EXPECT_FALSE(query_content_protection_success_);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_NONE, connection_mask_);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_NONE, protection_mask_);

  EXPECT_EQ(2, apply_content_protection_call_count_);
  EXPECT_FALSE(apply_content_protection_success_);

  EXPECT_EQ(HDCP_STATE_ENABLED, native_display_delegate_.hdcp_state());

  // Protections should be disabled after unregister.
  manager_.UnregisterClient(id);

  EXPECT_EQ(GetSetHDCPStateAction(kDisplayIds[1], HDCP_STATE_UNDESIRED,
                                  CONTENT_PROTECTION_METHOD_NONE),
            log_.GetActionsAndClear());
  EXPECT_EQ(HDCP_STATE_UNDESIRED, native_display_delegate_.hdcp_state());
}

TEST_F(ContentProtectionManagerTest, BasicAsync) {
  auto id = manager_.RegisterClient();
  EXPECT_TRUE(id);

  native_display_delegate_.set_run_async(true);

  // Asynchronous tasks should be pending.
  constexpr int kTaskCount = 3;
  for (int i = 0; i < kTaskCount; ++i) {
    manager_.ApplyContentProtection(
        id, displays_[1]->display_id(), CONTENT_PROTECTION_METHOD_HDCP,
        base::BindOnce(
            &ContentProtectionManagerTest::ApplyContentProtectionCallback,
            base::Unretained(this)));

    manager_.QueryContentProtection(
        id, displays_[1]->display_id(),
        base::BindOnce(
            &ContentProtectionManagerTest::QueryContentProtectionCallback,
            base::Unretained(this)));
  }

  EXPECT_EQ(0, apply_content_protection_call_count_);
  EXPECT_EQ(0, query_content_protection_call_count_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
  EXPECT_EQ(HDCP_STATE_UNDESIRED, native_display_delegate_.hdcp_state());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kTaskCount, apply_content_protection_call_count_);
  EXPECT_TRUE(apply_content_protection_success_);

  EXPECT_EQ(kTaskCount, query_content_protection_call_count_);
  EXPECT_TRUE(query_content_protection_success_);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_HDMI, connection_mask_);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_HDCP, protection_mask_);

  EXPECT_EQ(GetSetHDCPStateAction(kDisplayIds[1], HDCP_STATE_DESIRED,
                                  CONTENT_PROTECTION_METHOD_HDCP),
            log_.GetActionsAndClear());
  EXPECT_EQ(HDCP_STATE_ENABLED, native_display_delegate_.hdcp_state());

  // Pending task should run even if previous task fails.
  native_display_delegate_.set_set_hdcp_state_expectation(false);

  manager_.ApplyContentProtection(
      id, displays_[1]->display_id(), CONTENT_PROTECTION_METHOD_NONE,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  manager_.QueryContentProtection(
      id, displays_[1]->display_id(),
      base::BindOnce(
          &ContentProtectionManagerTest::QueryContentProtectionCallback,
          base::Unretained(this)));

  EXPECT_EQ(kTaskCount, apply_content_protection_call_count_);
  EXPECT_EQ(kTaskCount, query_content_protection_call_count_);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kTaskCount + 1, apply_content_protection_call_count_);
  EXPECT_FALSE(apply_content_protection_success_);

  EXPECT_EQ(kTaskCount + 1, query_content_protection_call_count_);
  EXPECT_TRUE(query_content_protection_success_);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_HDMI, connection_mask_);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_NONE, protection_mask_);

  // Disabling protection should fail.
  EXPECT_EQ(GetSetHDCPStateAction(kDisplayIds[1], HDCP_STATE_UNDESIRED,
                                  CONTENT_PROTECTION_METHOD_NONE),
            log_.GetActionsAndClear());
  EXPECT_EQ(HDCP_STATE_ENABLED, native_display_delegate_.hdcp_state());
}

TEST_F(ContentProtectionManagerTest, TwoClients) {
  auto client1 = manager_.RegisterClient();
  auto client2 = manager_.RegisterClient();
  EXPECT_NE(client1, client2);

  // Clients should not be aware of requests from other clients.
  manager_.ApplyContentProtection(
      client1, displays_[1]->display_id(), CONTENT_PROTECTION_METHOD_HDCP,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  EXPECT_EQ(1, apply_content_protection_call_count_);
  EXPECT_TRUE(apply_content_protection_success_);

  EXPECT_EQ(GetSetHDCPStateAction(kDisplayIds[1], HDCP_STATE_DESIRED,
                                  CONTENT_PROTECTION_METHOD_HDCP),
            log_.GetActionsAndClear());
  EXPECT_EQ(HDCP_STATE_ENABLED, native_display_delegate_.hdcp_state());

  manager_.QueryContentProtection(
      client1, displays_[1]->display_id(),
      base::BindOnce(
          &ContentProtectionManagerTest::QueryContentProtectionCallback,
          base::Unretained(this)));

  EXPECT_EQ(1, query_content_protection_call_count_);
  EXPECT_TRUE(query_content_protection_success_);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_HDMI, connection_mask_);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_HDCP, protection_mask_);

  manager_.QueryContentProtection(
      client2, displays_[1]->display_id(),
      base::BindOnce(
          &ContentProtectionManagerTest::QueryContentProtectionCallback,
          base::Unretained(this)));

  EXPECT_EQ(2, query_content_protection_call_count_);
  EXPECT_TRUE(query_content_protection_success_);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_HDMI, connection_mask_);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_NONE, protection_mask_);

  // Protection should be disabled if there are no client requests.
  manager_.ApplyContentProtection(
      client2, displays_[1]->display_id(), CONTENT_PROTECTION_METHOD_NONE,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  EXPECT_EQ(2, apply_content_protection_call_count_);
  EXPECT_TRUE(apply_content_protection_success_);

  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
  EXPECT_EQ(HDCP_STATE_ENABLED, native_display_delegate_.hdcp_state());

  manager_.ApplyContentProtection(
      client1, displays_[1]->display_id(), CONTENT_PROTECTION_METHOD_NONE,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  EXPECT_EQ(3, apply_content_protection_call_count_);
  EXPECT_TRUE(apply_content_protection_success_);

  EXPECT_EQ(GetSetHDCPStateAction(kDisplayIds[1], HDCP_STATE_UNDESIRED,
                                  CONTENT_PROTECTION_METHOD_NONE),
            log_.GetActionsAndClear());
  EXPECT_EQ(HDCP_STATE_UNDESIRED, native_display_delegate_.hdcp_state());
}

TEST_F(ContentProtectionManagerTest, TwoClientsEnable) {
  auto client1 = manager_.RegisterClient();
  auto client2 = manager_.RegisterClient();
  EXPECT_NE(client1, client2);

  // Multiple requests should result in one update.
  manager_.ApplyContentProtection(
      client1, displays_[1]->display_id(), CONTENT_PROTECTION_METHOD_HDCP,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  EXPECT_EQ(1, apply_content_protection_call_count_);
  EXPECT_TRUE(apply_content_protection_success_);

  manager_.ApplyContentProtection(
      client2, displays_[1]->display_id(), CONTENT_PROTECTION_METHOD_HDCP,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  EXPECT_EQ(2, apply_content_protection_call_count_);
  EXPECT_TRUE(apply_content_protection_success_);

  EXPECT_EQ(GetSetHDCPStateAction(kDisplayIds[1], HDCP_STATE_DESIRED,
                                  CONTENT_PROTECTION_METHOD_HDCP),
            log_.GetActionsAndClear());
  EXPECT_EQ(HDCP_STATE_ENABLED, native_display_delegate_.hdcp_state());

  // Requests should not result in updates if protection is already active.
  manager_.ApplyContentProtection(
      client1, displays_[1]->display_id(), CONTENT_PROTECTION_METHOD_HDCP,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  EXPECT_EQ(3, apply_content_protection_call_count_);
  EXPECT_TRUE(apply_content_protection_success_);

  manager_.ApplyContentProtection(
      client2, displays_[1]->display_id(), CONTENT_PROTECTION_METHOD_HDCP,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  EXPECT_EQ(4, apply_content_protection_call_count_);
  EXPECT_TRUE(apply_content_protection_success_);

  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
  EXPECT_EQ(HDCP_STATE_ENABLED, native_display_delegate_.hdcp_state());
}

TEST_F(ContentProtectionManagerTest, RequestRetention) {
  auto id = manager_.RegisterClient();
  EXPECT_TRUE(id);

  native_display_delegate_.set_run_async(true);

  // Enable protection on external display.
  manager_.ApplyContentProtection(
      id, displays_[1]->display_id(), CONTENT_PROTECTION_METHOD_HDCP,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  // Disable protection on internal display.
  manager_.ApplyContentProtection(
      id, displays_[0]->display_id(), CONTENT_PROTECTION_METHOD_NONE,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  EXPECT_EQ(0, apply_content_protection_call_count_);
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
  EXPECT_EQ(HDCP_STATE_UNDESIRED, native_display_delegate_.hdcp_state());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, apply_content_protection_call_count_);
  EXPECT_TRUE(apply_content_protection_success_);

  // Protection on external display should be retained.
  EXPECT_EQ(GetSetHDCPStateAction(kDisplayIds[1], HDCP_STATE_DESIRED,
                                  CONTENT_PROTECTION_METHOD_HDCP)
                .c_str(),
            log_.GetActionsAndClear());
  EXPECT_EQ(HDCP_STATE_ENABLED, native_display_delegate_.hdcp_state());
}

TEST_F(ContentProtectionManagerTest, ClientRegistration) {
  auto id = manager_.RegisterClient();
  EXPECT_TRUE(id);

  native_display_delegate_.set_run_async(true);

  manager_.ApplyContentProtection(
      id, displays_[1]->display_id(), CONTENT_PROTECTION_METHOD_HDCP,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  manager_.QueryContentProtection(
      id, displays_[1]->display_id(),
      base::BindOnce(
          &ContentProtectionManagerTest::QueryContentProtectionCallback,
          base::Unretained(this)));

  manager_.UnregisterClient(id);

  base::RunLoop().RunUntilIdle();

  // Pending callbacks should not run if client was unregistered.
  EXPECT_EQ(0, apply_content_protection_call_count_);
  EXPECT_EQ(0, query_content_protection_call_count_);

  // Unregistration should disable protection.
  EXPECT_EQ(
      JoinActions(GetSetHDCPStateAction(kDisplayIds[1], HDCP_STATE_DESIRED,
                                        CONTENT_PROTECTION_METHOD_HDCP)
                      .c_str(),
                  GetSetHDCPStateAction(kDisplayIds[1], HDCP_STATE_UNDESIRED,
                                        CONTENT_PROTECTION_METHOD_NONE)
                      .c_str(),
                  nullptr),
      log_.GetActionsAndClear());
  EXPECT_EQ(HDCP_STATE_UNDESIRED, native_display_delegate_.hdcp_state());
}

TEST_F(ContentProtectionManagerTest, TasksKilledOnConfigure) {
  auto id = manager_.RegisterClient();
  EXPECT_TRUE(id);

  native_display_delegate_.set_run_async(true);

  manager_.ApplyContentProtection(
      id, displays_[1]->display_id(), CONTENT_PROTECTION_METHOD_HDCP,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  manager_.QueryContentProtection(
      id, displays_[1]->display_id(),
      base::BindOnce(
          &ContentProtectionManagerTest::QueryContentProtectionCallback,
          base::Unretained(this)));

  TriggerDisplayConfiguration();
  base::RunLoop().RunUntilIdle();

  // Configuration change should kill tasks and trigger failure callbacks.
  EXPECT_EQ(1, apply_content_protection_call_count_);
  EXPECT_FALSE(apply_content_protection_success_);

  EXPECT_EQ(1, query_content_protection_call_count_);
  EXPECT_FALSE(query_content_protection_success_);
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_NONE, connection_mask_);
  EXPECT_EQ(CONTENT_PROTECTION_METHOD_NONE, protection_mask_);

  // Pending task to enable protection should have been killed.
  EXPECT_EQ(kNoActions, log_.GetActionsAndClear());
  EXPECT_EQ(HDCP_STATE_UNDESIRED, native_display_delegate_.hdcp_state());
}

TEST_F(ContentProtectionManagerTest, DisplaySecurityObserver) {
  TestObserver observer(&manager_);

  // Internal display is secure if not mirroring.
  EXPECT_EQ(SecurityChanges({{kDisplayIds[0], true}, {kDisplayIds[1], false}}),
            observer.security_changes());
  observer.Reset();

  auto id = manager_.RegisterClient();
  EXPECT_TRUE(id);

  native_display_delegate_.set_run_async(true);

  manager_.ApplyContentProtection(
      id, kDisplayIds[1], CONTENT_PROTECTION_METHOD_HDCP,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.security_changes().empty());

  EXPECT_TRUE(TriggerDisplaySecurityTimeout());
  base::RunLoop().RunUntilIdle();

  // Observer should be notified when client applies protection.
  EXPECT_EQ(SecurityChanges({{kDisplayIds[0], true}, {kDisplayIds[1], true}}),
            observer.security_changes());
  observer.Reset();

  layout_manager_.set_display_state(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  TriggerDisplayConfiguration();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(TriggerDisplaySecurityTimeout());
  base::RunLoop().RunUntilIdle();

  // Observer should be notified on configuration change.
  EXPECT_EQ(SecurityChanges({{kDisplayIds[0], true}, {kDisplayIds[1], true}}),
            observer.security_changes());
  observer.Reset();

  manager_.ApplyContentProtection(
      id, kDisplayIds[1], CONTENT_PROTECTION_METHOD_NONE,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  // Timer should be stopped when no client requests protection.
  EXPECT_FALSE(TriggerDisplaySecurityTimeout());
  base::RunLoop().RunUntilIdle();

  // Internal display is not secure if mirrored to an unprotected display.
  EXPECT_EQ(SecurityChanges({{kDisplayIds[0], false}, {kDisplayIds[1], false}}),
            observer.security_changes());
  observer.Reset();

  native_display_delegate_.set_set_hdcp_state_expectation(false);

  manager_.ApplyContentProtection(
      id, kDisplayIds[1], CONTENT_PROTECTION_METHOD_HDCP,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.security_changes().empty());

  EXPECT_TRUE(TriggerDisplaySecurityTimeout());
  base::RunLoop().RunUntilIdle();

  // Internal display is not secure if mirrored to an unprotected display.
  EXPECT_EQ(SecurityChanges({{kDisplayIds[0], false}, {kDisplayIds[1], false}}),
            observer.security_changes());
  observer.Reset();

  native_display_delegate_.set_hdcp_state(HDCP_STATE_UNDESIRED);
  native_display_delegate_.set_set_hdcp_state_expectation(true);

  manager_.ApplyContentProtection(
      id, kDisplayIds[1], CONTENT_PROTECTION_METHOD_HDCP,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.security_changes().empty());

  EXPECT_TRUE(TriggerDisplaySecurityTimeout());
  base::RunLoop().RunUntilIdle();

  // Internal display is secure if mirrored to a protected display.
  EXPECT_EQ(SecurityChanges({{kDisplayIds[0], true}, {kDisplayIds[1], true}}),
            observer.security_changes());
  observer.Reset();

  layout_manager_.set_display_state(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  TriggerDisplayConfiguration();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(TriggerDisplaySecurityTimeout());
  base::RunLoop().RunUntilIdle();

  // Observer should be notified on configuration change.
  EXPECT_EQ(SecurityChanges({{kDisplayIds[0], true}, {kDisplayIds[1], true}}),
            observer.security_changes());
  observer.Reset();

  manager_.UnregisterClient(id);

  // Timer should be stopped when no client requests protection.
  EXPECT_FALSE(TriggerDisplaySecurityTimeout());
  base::RunLoop().RunUntilIdle();

  // Observer should be notified when client unregisters.
  EXPECT_EQ(SecurityChanges({{kDisplayIds[0], true}, {kDisplayIds[1], false}}),
            observer.security_changes());
}

TEST_F(ContentProtectionManagerTest, NoSecurityPollingIfInternalDisplayOnly) {
  UpdateDisplays(1);
  TestObserver observer(&manager_);

  EXPECT_EQ(SecurityChanges({{kDisplayIds[0], true}}),
            observer.security_changes());
  observer.Reset();

  auto id = manager_.RegisterClient();
  EXPECT_TRUE(id);

  native_display_delegate_.set_run_async(true);

  manager_.ApplyContentProtection(
      id, kDisplayIds[0], CONTENT_PROTECTION_METHOD_HDCP,
      base::BindOnce(
          &ContentProtectionManagerTest::ApplyContentProtectionCallback,
          base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SecurityChanges({{kDisplayIds[0], true}}),
            observer.security_changes());
  observer.Reset();

  // Timer should not be running unless there are external displays.
  EXPECT_FALSE(TriggerDisplaySecurityTimeout());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(SecurityChanges(), observer.security_changes());

  manager_.UnregisterClient(id);

  EXPECT_FALSE(TriggerDisplaySecurityTimeout());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(SecurityChanges({{kDisplayIds[0], true}}),
            observer.security_changes());
}

TEST_F(ContentProtectionManagerTest, AnalogDisplaySecurity) {
  UpdateDisplays(3);
  TestObserver observer(&manager_);

  EXPECT_EQ(SecurityChanges({{kDisplayIds[0], true},
                             {kDisplayIds[1], false},
                             {kDisplayIds[2], false}}),
            observer.security_changes());
  observer.Reset();

  auto id = manager_.RegisterClient();
  EXPECT_TRUE(id);

  native_display_delegate_.set_run_async(true);

  for (int64_t display_id : kDisplayIds) {
    manager_.ApplyContentProtection(
        id, display_id, CONTENT_PROTECTION_METHOD_HDCP,
        base::BindOnce(
            &ContentProtectionManagerTest::ApplyContentProtectionCallback,
            base::Unretained(this)));
  }

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer.security_changes().empty());

  EXPECT_TRUE(TriggerDisplaySecurityTimeout());
  base::RunLoop().RunUntilIdle();

  // Analog display is never secure.
  EXPECT_EQ(SecurityChanges({{kDisplayIds[0], true},
                             {kDisplayIds[1], true},
                             {kDisplayIds[2], false}}),
            observer.security_changes());
  observer.Reset();

  layout_manager_.set_display_state(MULTIPLE_DISPLAY_STATE_MULTI_MIRROR);
  TriggerDisplayConfiguration();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(TriggerDisplaySecurityTimeout());
  base::RunLoop().RunUntilIdle();

  // Internal display is not secure if mirrored to an analog display.
  EXPECT_EQ(SecurityChanges({{kDisplayIds[0], false},
                             {kDisplayIds[1], false},
                             {kDisplayIds[2], false}}),
            observer.security_changes());
  observer.Reset();

  layout_manager_.set_display_state(MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED);
  TriggerDisplayConfiguration();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(TriggerDisplaySecurityTimeout());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(SecurityChanges({{kDisplayIds[0], true},
                             {kDisplayIds[1], true},
                             {kDisplayIds[2], false}}),
            observer.security_changes());
  observer.Reset();

  manager_.UnregisterClient(id);

  // Timer should be stopped when no client requests protection.
  EXPECT_FALSE(TriggerDisplaySecurityTimeout());
  base::RunLoop().RunUntilIdle();

  // Observer should be notified when client unregisters.
  EXPECT_EQ(SecurityChanges({{kDisplayIds[0], true},
                             {kDisplayIds[1], false},
                             {kDisplayIds[2], false}}),
            observer.security_changes());
}

}  // namespace display::test
