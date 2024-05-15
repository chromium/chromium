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

// Test that if using xdg output (and supports_viewporter_surface_scaling is
// enabled) the scale factor is calculated as the ratio of physical size to
// logical size as long as it is more than 1, otherwise it is set to 1.
TEST_F(WaylandOutputTest, ScaleFactorCalculation) {
  auto* const output_manager = connection_->wayland_output_manager();
  ASSERT_TRUE(output_manager);

  auto* wl_output = output_manager->GetPrimaryOutput();
  ASSERT_TRUE(wl_output);
  EXPECT_FALSE(wl_output->xdg_output_);

  ASSERT_FALSE(connection_->surface_submission_in_pixel_coordinates());
  connection_->set_supports_viewporter_surface_scaling(true);
  ASSERT_TRUE(connection_->supports_viewporter_surface_scaling());

  constexpr float kWlOutputScale = 2.f;
  constexpr int kLogicalSideLength = 50;
  float compositor_scale = 3.f;
  wl_output->xdg_output_ = std::make_unique<XDGOutput>(nullptr);
  wl_output->xdg_output_->logical_size_ =
      gfx::Size(kLogicalSideLength, kLogicalSideLength);
  wl_output->physical_size_ = gfx::Size(compositor_scale * kLogicalSideLength,
                                        compositor_scale * kLogicalSideLength);
  wl_output->scale_factor_ = kWlOutputScale;

  wl_output->is_ready_ = true;
  wl_output->xdg_output_->is_ready_ = true;

  // Scale factor should be calculated from physical and logical sizes.
  wl_output->UpdateMetrics();
  EXPECT_EQ(compositor_scale, wl_output->scale_factor());

  // As the calculated value is less than one, the scale factor should be
  // clamped to 1.
  compositor_scale = 0.5f;
  wl_output->physical_size_ = gfx::Size(compositor_scale * kLogicalSideLength,
                                        compositor_scale * kLogicalSideLength);
  wl_output->UpdateMetrics();
  EXPECT_EQ(1, wl_output->scale_factor());
}

// Test scale factor falls back to wl_output::scale instead of being calculated
// as the ratio of physical size to logical size when xdg_output is not ready or
// if supports_viewporter_surface_scaling is disabled.
TEST_F(WaylandOutputTest, ScaleFactorFallback) {
  auto* const output_manager = connection_->wayland_output_manager();
  ASSERT_TRUE(output_manager);

  auto* wl_output = output_manager->GetPrimaryOutput();
  ASSERT_TRUE(wl_output);
  EXPECT_FALSE(wl_output->xdg_output_);
  EXPECT_FALSE(connection_->supports_viewporter_surface_scaling());

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

  // If xdg_output is ready but supports_viewporter_surface_scaling is
  // false, scale_factor should fall back to the value sent in wl_output::scale.
  wl_output->xdg_output_->is_ready_ = true;
  wl_output->UpdateMetrics();
  EXPECT_EQ(kDefaultScaleFactor, wl_output->scale_factor());
}

// Test that if using xdg output and viewporter surface scaling is enabled
// the scale factor calculation based on xdg-output's logical size is no-op when
// physical and logical sizes are equal.
//
// Regression test for https://crbug.com/339681887.
TEST_F(WaylandOutputTest, ScaleFactorCalculationNoop) {
  auto* const output_manager = connection_->wayland_output_manager();
  ASSERT_TRUE(output_manager);

  auto* wl_output = output_manager->GetPrimaryOutput();
  ASSERT_TRUE(wl_output);
  EXPECT_FALSE(wl_output->xdg_output_);

  ASSERT_FALSE(connection_->surface_submission_in_pixel_coordinates());
  connection_->set_supports_viewporter_surface_scaling(true);
  ASSERT_TRUE(connection_->supports_viewporter_surface_scaling());

  constexpr float kWlOutputScale = 3.f;
  wl_output->xdg_output_ = std::make_unique<XDGOutput>(nullptr);
  wl_output->xdg_output_->logical_size_ = gfx::Size(100, 100);
  wl_output->physical_size_ = gfx::Size(100, 100);
  wl_output->scale_factor_ = kWlOutputScale;

  // Since the logical size is the same as physical size, scale_factor should
  // fall back to the value sent in wl_output::scale.
  wl_output->is_ready_ = true;
  wl_output->xdg_output_->is_ready_ = true;

  wl_output->UpdateMetrics();
  EXPECT_EQ(kWlOutputScale, wl_output->scale_factor());
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

}  // namespace ui
