// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_output.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/wayland/wayland_display_util.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/xdg_output.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::Values;

namespace ui {
namespace {

class MockWaylandOutputDelegate : public WaylandOutput::Delegate {
 public:
  MOCK_METHOD(void,
              OnOutputHandleMetrics,
              (const WaylandOutput::Metrics& metrics),
              (override));
};

}  // namespace

using WaylandOutputTest = WaylandTestSimple;

// Tests that name and description fall back to ones in the WaylandOutput if
// XDGOutput is not created.
TEST_F(WaylandOutputTest, NameAndDescriptionFallback) {
  constexpr char kWlOutputName[] = "kWlOutputName";
  constexpr char kWlOutputDescription[] = "kWlOutputDescription";
  constexpr char kXDGOutputName[] = "kXDGOutputName";
  constexpr char kXDGOutputDescription[] = "kXDGOutputDescription";

  auto* const output_manager = connection_->wayland_output_manager();
  ASSERT_TRUE(output_manager);

  auto* wl_output = output_manager->GetPrimaryOutput();
  ASSERT_TRUE(wl_output);
  EXPECT_FALSE(wl_output->xdg_output_);
  wl_output->name_ = kWlOutputName;
  wl_output->description_ = kWlOutputDescription;

  // We only test trivial stuff here so it is okay to create an output that is
  // not backed with a real object.
  wl_output->xdg_output_ = std::make_unique<XDGOutput>(nullptr);
  wl_output->xdg_output_->name_ = kXDGOutputName;
  wl_output->xdg_output_->description_ = kXDGOutputDescription;
  wl_output->xdg_output_->is_ready_ = true;
  wl_output->is_ready_ = true;
  wl_output->UpdateMetrics();

  EXPECT_EQ(wl_output->GetMetrics().name, kXDGOutputName);
  EXPECT_EQ(wl_output->GetMetrics().description, kXDGOutputDescription);

  wl_output->xdg_output_.reset();
  wl_output->UpdateMetrics();

  EXPECT_EQ(wl_output->GetMetrics().name, kWlOutputName);
  EXPECT_EQ(wl_output->GetMetrics().description, kWlOutputDescription);
}

// Test that if using xdg output (and surface_submission_in_pixel_coordinates is
// enabled) the scale factor is calculated as the ratio of physical size to
// logical size.
TEST_F(WaylandOutputTest, ScaleFactorFallback) {
  auto* const output_manager = connection_->wayland_output_manager();
  ASSERT_TRUE(output_manager);

  auto* wl_output = output_manager->GetPrimaryOutput();
  ASSERT_TRUE(wl_output);
  EXPECT_FALSE(wl_output->xdg_output_);
  EXPECT_FALSE(connection_->surface_submission_in_pixel_coordinates());

  // We only test trivial stuff here so it is okay to create an output that is
  // not backed with a real object.
  constexpr float kDefaultScaleFactor = 2.f;
  wl_output->xdg_output_ = std::make_unique<XDGOutput>(nullptr);
  wl_output->xdg_output_->logical_size_ = gfx::Size(100, 200);
  wl_output->physical_size_ = gfx::Size(250, 500);
  wl_output->scale_factor_ = kDefaultScaleFactor;

  // When wl_output is ready but xdg_output is not yet ready, scale_factor
  // should fall back to the value sent in wl_output::scale.
  wl_output->is_ready_ = true;
  wl_output->xdg_output_->is_ready_ = false;
  wl_output->UpdateMetrics();
  EXPECT_EQ(kDefaultScaleFactor, wl_output->scale_factor());

  // If xdg_output is ready but surface_submission_in_pixel_coordinates is
  // false, scale_factor should fall back to the value sent in wl_output::scale.
  wl_output->xdg_output_->is_ready_ = true;
  wl_output->UpdateMetrics();
  EXPECT_EQ(kDefaultScaleFactor, wl_output->scale_factor());

  // When xdg_output is ready and surface_submission_in_pixel_coordinates is
  // true, scale_factor should be calculated as the ratio of physical to logical
  // size.
  connection_->set_surface_submission_in_pixel_coordinates(true);
  wl_output->UpdateMetrics();
  EXPECT_EQ(2.5f, wl_output->scale_factor());
}

TEST_F(WaylandOutputTest, WaylandOutputIsReady) {
  auto* output_manager = connection_->wayland_output_manager();
  const auto* primary_output = output_manager->GetPrimaryOutput();

  // Create a new output but suppress metrics.
  wl::TestOutput* test_output = nullptr;
  PostToServerAndWait([&test_output](wl::TestWaylandServerThread* server) {
    test_output = server->CreateAndInitializeOutput(
        wl::TestOutputMetrics({0, 0, 800, 600}));
    ASSERT_TRUE(test_output);
    test_output->set_suppress_implicit_flush(true);
  });
  const auto& all_outputs = output_manager->GetAllOutputs();
  ASSERT_EQ(2u, all_outputs.size());

  // Get the newly created WaylandOutput.
  auto pair_it = base::ranges::find_if_not(all_outputs, [&](auto& pair) {
    return pair.first == primary_output->output_id();
  });
  ASSERT_NE(all_outputs.end(), pair_it);
  const auto* new_output = pair_it->second.get();
  EXPECT_NE(nullptr, new_output);

  // The output should not be marked ready since metrics and specifically the
  // wl_output.done event has not yet been received.
  EXPECT_FALSE(new_output->IsReady());

  // Flush metrics and the output should enter the ready state.
  PostToServerAndWait([&test_output](wl::TestWaylandServerThread* server) {
    test_output->Flush();
  });
  EXPECT_TRUE(new_output->IsReady());
}

class WaylandOutputWithAuraOutputManagerTest : public WaylandTestSimple {
 protected:
  WaylandOutputWithAuraOutputManagerTest()
      : WaylandTestSimple(wl::ServerConfig{
            .enable_aura_shell = wl::EnableAuraShellProtocol::kEnabled,
            .use_aura_output_manager = true}) {}

  WaylandOutputManager* wayland_output_manager() {
    auto* wayland_output_manager = connection_->wayland_output_manager();
    EXPECT_TRUE(wayland_output_manager);
    return wayland_output_manager;
  }

  WaylandOutput* primary_output() {
    return wayland_output_manager()->GetPrimaryOutput();
  }
};

// Tests that WaylandOutput only reports as ready if metrics has been set when
// the aura output manager is bound.
TEST_F(WaylandOutputWithAuraOutputManagerTest, WaylandOutputIsReady) {
  auto* output_manager = connection_->wayland_output_manager();
  const auto* primary_output = output_manager->GetPrimaryOutput();

  // Create a new output but suppress metrics and wl_output.done.
  wl::TestOutput* test_output = nullptr;
  PostToServerAndWait([&test_output](wl::TestWaylandServerThread* server) {
    test_output = server->CreateAndInitializeOutput();
    ASSERT_TRUE(test_output);
    test_output->set_suppress_implicit_flush(true);
  });
  auto& all_outputs = output_manager->GetAllOutputs();
  ASSERT_EQ(2u, all_outputs.size());

  // Get the newly created WaylandOutput.
  auto pair_it = base::ranges::find_if_not(all_outputs, [&](auto& pair) {
    return pair.first == primary_output->output_id();
  });
  ASSERT_NE(all_outputs.end(), pair_it);
  auto* new_output = pair_it->second.get();
  EXPECT_NE(nullptr, new_output);

  // The output should not be marked ready since metrics have not yet been
  // explicitly set.
  EXPECT_FALSE(new_output->IsReady());

  // Set the output metrics without sending the wl_output.done event. The
  // WaylandOutput should report as ready.
  WaylandOutput::Metrics metrics;
  metrics.output_id = new_output->output_id();
  new_output->SetMetrics(metrics);
  EXPECT_TRUE(new_output->IsReady());
}

// Tests that delegate notifications are not triggered when the wl_output.done
// event is received. It is instead expected to be triggered by the aura output
// manager.
TEST_F(WaylandOutputWithAuraOutputManagerTest,
       SuppressesDelegateNotifications) {
  testing::NiceMock<MockWaylandOutputDelegate> output_delegate;
  EXPECT_CALL(output_delegate, OnOutputHandleMetrics(testing::_)).Times(0);
  primary_output()->set_delegate_for_testing(&output_delegate);

  // Send the wl_output.done event.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl_output_send_done(server->output()->resource());
  });
}

}  // namespace ui
