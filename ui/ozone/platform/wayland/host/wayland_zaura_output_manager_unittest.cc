// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_output_manager.h"

#include "base/bit_cast.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/wayland/wayland_display_util.h"
#include "ui/display/test/test_screen.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/test/test_output.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

namespace ui {
namespace {

// A Test Screen that uses Display info in WaylandOutputManager. It has a bare
// minimum impl to support the test case for Set|Get|DisplayForNewWindows().
class WaylandTestScreen : public display::test::TestScreen {
 public:
  explicit WaylandTestScreen(WaylandScreen* wayland_screen)
      : display::test::TestScreen(/*create_display=*/false,
                                  /*register_screen=*/true),
        wayland_screen_(wayland_screen) {}

  const std::vector<display::Display>& GetAllDisplays() const override {
    return wayland_screen_->GetAllDisplays();
  }

 private:
  raw_ptr<WaylandScreen> wayland_screen_;
};

class MockWaylandOutputDelegate : public WaylandOutput::Delegate {
 public:
  MOCK_METHOD(void,
              OnOutputHandleMetrics,
              (const WaylandOutput::Metrics& metrics),
              (override));
};

}  // namespace

class WaylandZAuraOutputManagerTest : public WaylandTestSimple {
 public:
  WaylandZAuraOutputManagerTest()
      : WaylandTestSimple(wl::ServerConfig{
            .enable_aura_shell = wl::EnableAuraShellProtocol::kEnabled,
            .use_aura_output_manager = true}) {}

 protected:
  // Sends sample metrics to the primary output configured for this fixture.
  void SendSampleMetrics(const WaylandOutput::Metrics& metrics) {
    const auto wayland_display_id =
        ui::wayland::ToWaylandDisplayIdPair(metrics.display_id);

    PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
      auto* const manager_resource = server->zaura_output_manager()->resource();
      auto* const output_resource = server->output()->resource();
      zaura_output_manager_send_display_id(manager_resource, output_resource,
                                           wayland_display_id.high,
                                           wayland_display_id.low);
      zaura_output_manager_send_logical_position(
          manager_resource, output_resource, metrics.origin.x(),
          metrics.origin.y());
      zaura_output_manager_send_logical_size(manager_resource, output_resource,
                                             metrics.logical_size.width(),
                                             metrics.logical_size.height());
      zaura_output_manager_send_physical_size(manager_resource, output_resource,
                                              metrics.physical_size.width(),
                                              metrics.physical_size.height());
      zaura_output_manager_send_insets(
          manager_resource, output_resource, metrics.insets.top(),
          metrics.insets.left(), metrics.insets.bottom(),
          metrics.insets.right());
      zaura_output_manager_send_device_scale_factor(
          manager_resource, output_resource,
          base::bit_cast<uint32_t>(metrics.scale_factor));
      zaura_output_manager_send_logical_transform(
          manager_resource, output_resource, metrics.logical_transform);
      zaura_output_manager_send_panel_transform(
          manager_resource, output_resource, metrics.panel_transform);
      zaura_output_manager_send_name(manager_resource, output_resource,
                                     metrics.name.c_str());
      zaura_output_manager_send_description(manager_resource, output_resource,
                                            metrics.description.c_str());
      zaura_output_manager_send_done(manager_resource, output_resource);
    });
  }

  // Returns a Metrics object with fixed sample values for testing consistent
  // with the current primary output.
  WaylandOutput::Metrics GetSampleMetrics() {
    // Metrics should have already been propagated and the output as part of
    // test setup.
    const auto* output = primary_output();
    EXPECT_TRUE(output->IsReady());

    WaylandOutput::Metrics metrics;
    metrics.output_id = output->GetMetrics().output_id;
    metrics.display_id = output->GetMetrics().display_id;
    metrics.origin = gfx::Point(10, 20);
    metrics.scale_factor = 1;
    metrics.logical_size = gfx::Size(100, 200);
    metrics.physical_size = gfx::Size(100, 200);
    metrics.insets = gfx::Insets(10);
    metrics.panel_transform = WL_OUTPUT_TRANSFORM_90;
    metrics.logical_transform = WL_OUTPUT_TRANSFORM_180;
    metrics.name = "DisplayName";
    metrics.description = "DisplayDiscription";
    return metrics;
  }

  const WaylandOutputManager* wayland_output_manager() const {
    const auto* wayland_output_manager = connection_->wayland_output_manager();
    EXPECT_TRUE(wayland_output_manager);
    return wayland_output_manager;
  }

  WaylandZAuraOutputManager* aura_output_manager() {
    auto* const aura_output_manager = connection_->zaura_output_manager();
    EXPECT_TRUE(aura_output_manager);
    return aura_output_manager;
  }

  WaylandOutput* primary_output() {
    return const_cast<WaylandOutput*>(
        static_cast<const WaylandZAuraOutputManagerTest*>(this)
            ->primary_output());
  }
  const WaylandOutput* primary_output() const {
    return wayland_output_manager()->GetPrimaryOutput();
  }
};

// Tests the happy case where server events are correctly translated to
// WaylandOutput::Metrics.
TEST_F(WaylandZAuraOutputManagerTest, ServerEventsPopulateOutputMetrics) {
  const auto sample_metrics = GetSampleMetrics();
  SendSampleMetrics(sample_metrics);

  const WaylandOutput::Id output_id = primary_output()->output_id();
  const WaylandOutput::Metrics* metrics =
      aura_output_manager()->GetOutputMetrics(output_id);
  EXPECT_TRUE(metrics);
  EXPECT_EQ(output_id, metrics->output_id);
  EXPECT_EQ(sample_metrics.display_id, metrics->display_id);
  EXPECT_EQ(sample_metrics.origin, metrics->origin);
  EXPECT_EQ(sample_metrics.logical_size, metrics->logical_size);
  EXPECT_EQ(sample_metrics.physical_size, metrics->physical_size);
  EXPECT_EQ(sample_metrics.insets, metrics->insets);
  EXPECT_EQ(sample_metrics.scale_factor, metrics->scale_factor);
  EXPECT_EQ(sample_metrics.panel_transform, metrics->panel_transform);
  EXPECT_EQ(sample_metrics.logical_transform, metrics->logical_transform);
  EXPECT_EQ(sample_metrics.name, metrics->name);
  EXPECT_EQ(sample_metrics.description, metrics->description);
}

// Tests that multiple batches of update events are reflected in the output
// manager
TEST_F(WaylandZAuraOutputManagerTest, SuccessiveServerEventsUpdateMetrics) {
  // Generate and send an initial set of metrics.
  const auto sample_metrics = GetSampleMetrics();
  SendSampleMetrics(sample_metrics);

  // Validate that these original metrics are reflected in the manager.
  const WaylandOutput::Id output_id = primary_output()->output_id();
  const WaylandOutput::Metrics* metrics =
      aura_output_manager()->GetOutputMetrics(output_id);
  EXPECT_TRUE(metrics);
  EXPECT_EQ(output_id, metrics->output_id);
  EXPECT_EQ(sample_metrics.display_id, metrics->display_id);
  EXPECT_EQ(sample_metrics.origin, metrics->origin);
  EXPECT_EQ(sample_metrics.logical_size, metrics->logical_size);
  EXPECT_EQ(sample_metrics.physical_size, metrics->physical_size);
  EXPECT_EQ(sample_metrics.insets, metrics->insets);
  EXPECT_EQ(sample_metrics.scale_factor, metrics->scale_factor);
  EXPECT_EQ(sample_metrics.panel_transform, metrics->panel_transform);
  EXPECT_EQ(sample_metrics.logical_transform, metrics->logical_transform);
  EXPECT_EQ(sample_metrics.name, metrics->name);
  EXPECT_EQ(sample_metrics.description, metrics->description);

  // Create new metrics that will be sent as an update to the same output.
  WaylandOutput::Metrics new_sample_metrics;
  new_sample_metrics.origin = gfx::Point(20, 40);
  new_sample_metrics.logical_size = gfx::Size(200, 400);
  new_sample_metrics.physical_size = gfx::Size(400, 800);
  new_sample_metrics.insets = gfx::Insets(20);
  new_sample_metrics.scale_factor = 2;
  new_sample_metrics.panel_transform = WL_OUTPUT_TRANSFORM_180;
  new_sample_metrics.logical_transform = WL_OUTPUT_TRANSFORM_90;
  new_sample_metrics.name = "NewDisplayName";
  new_sample_metrics.description = "NewDisplayDiscription";

  // Send the new sample metrics and validate that these new metrics are
  // reflected in the manager.
  SendSampleMetrics(new_sample_metrics);
  const WaylandOutput::Metrics* new_metrics =
      aura_output_manager()->GetOutputMetrics(output_id);
  EXPECT_EQ(output_id, new_metrics->output_id);
  EXPECT_EQ(new_sample_metrics.display_id, new_metrics->display_id);
  EXPECT_EQ(new_sample_metrics.origin, new_metrics->origin);
  EXPECT_EQ(new_sample_metrics.logical_size, new_metrics->logical_size);
  EXPECT_EQ(new_sample_metrics.physical_size, new_metrics->physical_size);
  EXPECT_EQ(new_sample_metrics.insets, new_metrics->insets);
  EXPECT_EQ(new_sample_metrics.scale_factor, new_metrics->scale_factor);
  EXPECT_EQ(new_sample_metrics.panel_transform, new_metrics->panel_transform);
  EXPECT_EQ(new_sample_metrics.logical_transform,
            new_metrics->logical_transform);
  EXPECT_EQ(new_sample_metrics.name, new_metrics->name);
  EXPECT_EQ(new_sample_metrics.description, new_metrics->description);
}

// Asserts that an output's entry in the output manager's map is erased when
// the output is destroyed.
TEST_F(WaylandZAuraOutputManagerTest, MetricsStateErasedWhenOutputDestroyed) {
  const WaylandOutput::Id output_id = primary_output()->output_id();

  // The sample metrics should already have been populated when the output was
  // initially bound.
  EXPECT_NE(nullptr, aura_output_manager()->GetOutputMetrics(output_id));

  // Destroy the output, the entry should be removed from the manager.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->output()->DestroyGlobal();
  });
  EXPECT_EQ(nullptr, aura_output_manager()->GetOutputMetrics(output_id));

  // Calling remove again should simply no-op.
  aura_output_manager()->RemoveOutputMetrics(output_id);
  EXPECT_EQ(nullptr, aura_output_manager()->GetOutputMetrics(output_id));
}

// Returns nullptr when asked for metrics for an unknown output.
TEST_F(WaylandZAuraOutputManagerTest, HandlesMetricsRequestsForUnknownOutputs) {
  ASSERT_EQ(1u, wayland_output_manager()->GetAllOutputs().size());
  const WaylandOutput::Id output_id = primary_output()->output_id();

  EXPECT_EQ(nullptr, aura_output_manager()->GetOutputMetrics(output_id + 1));
}

TEST_F(WaylandZAuraOutputManagerTest, ActiveDisplay) {
  WaylandTestScreen test_screen(wayland_output_manager()->wayland_screen());

  wl::TestOutput* primary_output = nullptr;
  wl::TestOutput* secondary_output = nullptr;
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    primary_output = server->output();
    secondary_output = server->CreateAndInitializeOutput();
  });

  int64_t primary_id = display::kInvalidDisplayId;
  int64_t secondary_id = display::kInvalidDisplayId;
  // Wait so that the output metrics are flushed to clients immediately after
  // the bind.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    primary_id = primary_output->GetDisplayId();
    secondary_id = secondary_output->GetDisplayId();
  });

  WaitForAllDisplaysReady();

  auto* platform_screen = wayland_output_manager()->wayland_screen();
  DCHECK(platform_screen);
  ASSERT_EQ(2u, platform_screen->GetAllDisplays().size());

  EXPECT_EQ(primary_id, platform_screen->GetAllDisplays()[0].id());
  EXPECT_EQ(secondary_id, platform_screen->GetAllDisplays()[1].id());

  // Activate the secondary output.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    server->zaura_output_manager()->SendActivated(secondary_output->resource());
  });
  EXPECT_EQ(secondary_id,
            display::Screen::GetScreen()->GetDisplayForNewWindows().id());

  // Activate the primary output.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    server->zaura_output_manager()->SendActivated(primary_output->resource());
  });
  EXPECT_EQ(primary_id,
            display::Screen::GetScreen()->GetDisplayForNewWindows().id());
}

// Tests that metrics reported by WaylandOutput correctly reflect the state sent
// to the aura output manager.
TEST_F(WaylandZAuraOutputManagerTest,
       UsesAuraOutputManagerMetricsWhenManagerBound) {
  const auto verify_output_metrics = [](const WaylandOutput::Metrics& metrics,
                                        const WaylandOutput* output) {
    const auto& reported_metrics = output->GetMetrics();
    EXPECT_EQ(metrics.display_id, reported_metrics.display_id);
    EXPECT_EQ(metrics.origin, reported_metrics.origin);
    EXPECT_EQ(metrics.scale_factor, reported_metrics.scale_factor);
    EXPECT_EQ(metrics.logical_size, reported_metrics.logical_size);
    EXPECT_EQ(metrics.physical_size, reported_metrics.physical_size);
    EXPECT_EQ(metrics.insets, reported_metrics.insets);
    EXPECT_EQ(metrics.panel_transform, reported_metrics.panel_transform);
    EXPECT_EQ(metrics.logical_transform, reported_metrics.logical_transform);
    EXPECT_EQ(metrics.name, reported_metrics.name);
    EXPECT_EQ(metrics.description, reported_metrics.description);
  };

  // Send the initial set of sample metrics, this should be reflected in the
  // WaylandOutput's reported metrics.
  auto sample_metrics = GetSampleMetrics();

  SendSampleMetrics(sample_metrics);
  verify_output_metrics(sample_metrics, primary_output());

  // Update the sample metrics, this should again be reflected in the reported
  // metrics.
  sample_metrics.origin = gfx::Point(20, 40);
  sample_metrics.scale_factor = 2;
  sample_metrics.logical_size = gfx::Size(200, 400);
  sample_metrics.physical_size = gfx::Size(400, 800);
  sample_metrics.insets = gfx::Insets(20);
  sample_metrics.panel_transform = WL_OUTPUT_TRANSFORM_180;
  sample_metrics.logical_transform = WL_OUTPUT_TRANSFORM_90;
  sample_metrics.name = "NewDisplayName";
  sample_metrics.description = "NewDisplayDiscription";

  SendSampleMetrics(sample_metrics);
  verify_output_metrics(sample_metrics, primary_output());
}

// Tests that delegate notifications are triggered on the WaylandOutput when the
// done event has arrived at the aura output manager.
TEST_F(WaylandZAuraOutputManagerTest,
       TriggersWaylandOutputNotificationsOnDone) {
  testing::NiceMock<MockWaylandOutputDelegate> output_delegate;
  EXPECT_CALL(output_delegate, OnOutputHandleMetrics(testing::_)).Times(1);
  primary_output()->set_delegate_for_testing(&output_delegate);
  SendSampleMetrics(GetSampleMetrics());
}

}  // namespace ui
