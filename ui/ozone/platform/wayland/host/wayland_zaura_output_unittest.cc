// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_output.h"

#include <aura-shell-client-protocol.h>
#include <aura-shell-server-protocol.h>
#include <wayland-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/test/mock_zaura_shell.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zaura_output.h"

namespace ui {
namespace {

using ::testing::Values;

class WaylandZAuraOutputTest : public ::testing::Test {
 public:
  WaylandZAuraOutputTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

  WaylandZAuraOutputTest(const WaylandZAuraOutputTest&) = delete;
  WaylandZAuraOutputTest& operator=(const WaylandZAuraOutputTest&) = delete;

  ~WaylandZAuraOutputTest() override = default;

  void SetUp() override {
    ::testing::Test::SetUp();

    ASSERT_TRUE(server_.Start({.shell_version = wl::ShellVersion::kStable}));
    mock_zaura_shell_.Initialize(server_.display());

    ASSERT_TRUE(connection_.Initialize());
    connection_.event_source()->StartProcessingEvents();
    base::RunLoop().RunUntilIdle();

    // Set default values for the output.
    wl::TestOutput* output = server_.output();
    output->SetRect({800, 600});
    output->SetScale(1);
    output->Flush();

    base::RunLoop().RunUntilIdle();
    server_.Pause();

    output_manager_ = connection_.wayland_output_manager();
    ASSERT_TRUE(output_manager_);
    EXPECT_TRUE(output_manager_->IsOutputReady());

    // Initializing the screen also connects it to the primary output, so it's
    // easier for us to get the associated WaylandOutput object later.
    platform_screen_ = output_manager_->CreateWaylandScreen();
    output_manager_->InitWaylandScreen(platform_screen_.get());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  wl::TestWaylandServerThread server_;
  wl::MockZAuraShell mock_zaura_shell_;
  WaylandConnection connection_;

  raw_ptr<WaylandOutputManager> output_manager_ = nullptr;
  std::unique_ptr<WaylandScreen> platform_screen_;
};

TEST_F(WaylandZAuraOutputTest, HandleInsets) {
  WaylandOutput* wayland_output = output_manager_->GetPrimaryOutput();
  ASSERT_TRUE(wayland_output);
  EXPECT_TRUE(wayland_output->is_ready());
  EXPECT_EQ(wayland_output->physical_size(), gfx::Size(800, 600));
  EXPECT_TRUE(wayland_output->insets().IsEmpty());
  EXPECT_TRUE(wayland_output->get_zaura_output());

  // Simulate server sending updated insets to the client.
  wl_resource* zaura_output_resource =
      server_.output()->GetAuraOutput()->resource();
  ASSERT_TRUE(zaura_output_resource);
  const gfx::Insets sent_insets =
      gfx::Rect(800, 600).InsetsFrom(gfx::Rect(10, 10, 500, 400));
  EXPECT_FALSE(sent_insets.IsEmpty());
  zaura_output_send_insets(zaura_output_resource, sent_insets.top(),
                           sent_insets.left(), sent_insets.bottom(),
                           sent_insets.right());

  server_.Resume();
  base::RunLoop().RunUntilIdle();
  server_.Pause();

  // Verify that insets is updated.
  EXPECT_TRUE(wayland_output->is_ready());
  EXPECT_EQ(wayland_output->physical_size(), gfx::Size(800, 600));
  EXPECT_EQ(wayland_output->insets(), sent_insets);
}

TEST_F(WaylandZAuraOutputTest, HandleLogicalTransform) {
  WaylandOutput* wayland_output = output_manager_->GetPrimaryOutput();
  ASSERT_TRUE(wayland_output);
  EXPECT_TRUE(wayland_output->is_ready());
  EXPECT_FALSE(wayland_output->logical_transform());
  EXPECT_TRUE(wayland_output->get_zaura_output());

  // Simulate server sending updated transform offset to the client.
  wl_resource* zaura_output_resource =
      server_.output()->GetAuraOutput()->resource();
  ASSERT_TRUE(zaura_output_resource);
  zaura_output_send_logical_transform(zaura_output_resource,
                                      WL_OUTPUT_TRANSFORM_270);

  server_.Resume();
  base::RunLoop().RunUntilIdle();
  server_.Pause();

  EXPECT_TRUE(wayland_output->is_ready());
  EXPECT_EQ(wayland_output->logical_transform(), WL_OUTPUT_TRANSFORM_270);
}

}  // namespace
}  // namespace ui