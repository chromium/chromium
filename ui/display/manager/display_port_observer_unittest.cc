// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_port_observer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/test/fake_display_snapshot.h"

namespace display {

namespace {

// Creates fake DP connector sysfs directory as the following.
//
//  # ls conn_path
//  > connector_id
//  > typec_connector
//
//  # ls conn_path/typec_connector
//  > uevent
bool CreateFakeDpConn(const base::FilePath& conn_path, int id, int port_num) {
  if (!base::CreateDirectory(conn_path)) {
    return false;
  }

  auto conn_id = base::StringPrintf("%d", id);
  if (!base::WriteFile(conn_path.Append("connector_id"), conn_id)) {
    return false;
  }

  auto typec_conn_path = conn_path.Append("typec_connector");
  if (!base::CreateDirectory(typec_conn_path)) {
    return false;
  }

  auto typec_conn_uevent = base::StringPrintf("TYPEC_PORT=port%d", port_num);
  if (!base::WriteFile(typec_conn_path.Append("uevent"), typec_conn_uevent)) {
    return false;
  }

  return true;
}

std::unique_ptr<DisplaySnapshot> CreateDisplaySnapshot(
    uint64_t base_connector_id,
    base::FilePath sys_path) {
  return FakeDisplaySnapshot::Builder()
      .SetId(1)
      .SetNativeMode(gfx::Size(1024, 768))
      .SetBaseConnectorId(base_connector_id)
      .SetSysPath(sys_path)
      .Build();
}

}  // namespace

class DisplayPortObserverTest : public testing::Test {
 public:
  DisplayPortObserverTest() = default;
  DisplayPortObserverTest(const DisplayPortObserverTest&) = delete;
  DisplayPortObserverTest& operator=(const DisplayPortObserverTest&) = delete;
  ~DisplayPortObserverTest() override = default;

  // testing::Test
  void SetUp() override {
    display_port_observer_ = std::make_unique<DisplayPortObserver>(
        nullptr,
        base::BindLambdaForTesting([&](const std::vector<uint32_t>& port_nums) {
          cached_port_nums_ = port_nums;
        }));

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    temp_dir_ = scoped_temp_dir_.GetPath();

    Test::SetUp();
  }

 protected:
  std::unique_ptr<DisplayPortObserver> display_port_observer_;
  base::FilePath temp_dir_;
  base::ScopedTempDir scoped_temp_dir_;
  base::test::TaskEnvironment task_environment_;
  std::vector<uint32_t> cached_port_nums_;
};

TEST_F(DisplayPortObserverTest, OnNoDisplayConnected) {
  // 4 Type C ports, each with a dedicated DP connector
  ASSERT_TRUE(CreateFakeDpConn(temp_dir_.Append("card0-DP-1"), 256, 0));
  ASSERT_TRUE(CreateFakeDpConn(temp_dir_.Append("card0-DP-2"), 266, 1));
  ASSERT_TRUE(CreateFakeDpConn(temp_dir_.Append("card0-DP-3"), 275, 2));
  ASSERT_TRUE(CreateFakeDpConn(temp_dir_.Append("card0-DP-4"), 284, 3));

  // Only internal display with an id not dedicated to any DP connector
  std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>> display_states;
  auto display_state = CreateDisplaySnapshot(236, temp_dir_);
  display_states.push_back(display_state.get());

  display_port_observer_->OnDisplayConfigurationChanged(
      std::move(display_states));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(cached_port_nums_.empty());
}

TEST_F(DisplayPortObserverTest, OnMultipleDisplaysConnected) {
  // 4 Type C ports, each with a dedicated DP connector
  ASSERT_TRUE(CreateFakeDpConn(temp_dir_.Append("card0-DP-1"), 256, 0));
  ASSERT_TRUE(CreateFakeDpConn(temp_dir_.Append("card0-DP-2"), 266, 1));
  ASSERT_TRUE(CreateFakeDpConn(temp_dir_.Append("card0-DP-3"), 275, 2));
  ASSERT_TRUE(CreateFakeDpConn(temp_dir_.Append("card0-DP-4"), 284, 3));

  // 2 displays on port 0 and 1 display on port 2
  std::vector<raw_ptr<DisplaySnapshot, VectorExperimental>> display_states;
  auto display_state1 = CreateDisplaySnapshot(256, temp_dir_);
  display_states.push_back(display_state1.get());

  auto display_state2 = CreateDisplaySnapshot(256, temp_dir_);
  display_states.push_back(display_state2.get());

  auto display_state3 = CreateDisplaySnapshot(275, temp_dir_);
  display_states.push_back(display_state3.get());

  display_port_observer_->OnDisplayConfigurationChanged(
      std::move(display_states));
  task_environment_.RunUntilIdle();

  const std::vector<uint32_t> kExpectedPortNums{0, 0, 2};
  EXPECT_EQ(kExpectedPortNums, cached_port_nums_);
}

}  // namespace display
