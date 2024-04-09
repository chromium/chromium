// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_output_manager_v2.h"

#include <components/exo/wayland/protocol/aura-output-management-server-protocol.h>

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

// A TestScreen impl with minimal functionality to support activation testing.
class WaylandTestScreen : public display::test::TestScreen {
 public:
  explicit WaylandTestScreen(WaylandScreen* wayland_screen)
      : display::test::TestScreen(/*create_display=*/false,
                                  /*register_screen=*/true),
        wayland_screen_(wayland_screen) {}

  // display::test::TestScreen:
  const std::vector<display::Display>& GetAllDisplays() const override {
    return wayland_screen_->GetAllDisplays();
  }

 private:
  raw_ptr<WaylandScreen> wayland_screen_;
};

}  // namespace

class WaylandZAuraOutputManagerV2Test : public WaylandTestSimple {
 public:
  WaylandZAuraOutputManagerV2Test()
      : WaylandTestSimple(wl::ServerConfig{
            .enable_aura_shell = wl::EnableAuraShellProtocol::kEnabled}) {}

 protected:
  // Sends metrics for a given output from the server-thread.
  void SendMetricsOnServerThread(const WaylandOutput::Metrics& metrics) {
    PostToServerAndWait([&metrics](wl::TestWaylandServerThread* server) {
      wl_resource* manager_resource =
          server->zaura_output_manager_v2()->resource();
      const WaylandOutput::Id output_id = metrics.output_id;

      const auto wayland_display_id =
          ui::wayland::ToWaylandDisplayIdPair(metrics.display_id);
      zaura_output_manager_v2_send_display_id(manager_resource, output_id,
                                              wayland_display_id.high,
                                              wayland_display_id.low);
      zaura_output_manager_v2_send_logical_position(
          manager_resource, output_id, metrics.origin.x(), metrics.origin.y());
      zaura_output_manager_v2_send_logical_size(manager_resource, output_id,
                                                metrics.logical_size.width(),
                                                metrics.logical_size.height());
      zaura_output_manager_v2_send_physical_size(
          manager_resource, output_id, metrics.physical_size.width(),
          metrics.physical_size.height());
      zaura_output_manager_v2_send_work_area_insets(
          manager_resource, output_id, metrics.insets.top(),
          metrics.insets.left(), metrics.insets.bottom(),
          metrics.insets.right());
      zaura_output_manager_v2_send_overscan_insets(
          manager_resource, output_id, metrics.physical_overscan_insets.top(),
          metrics.physical_overscan_insets.left(),
          metrics.physical_overscan_insets.bottom(),
          metrics.physical_overscan_insets.right());
      zaura_output_manager_v2_send_device_scale_factor(
          manager_resource, output_id,
          base::bit_cast<uint32_t>(metrics.scale_factor));
      zaura_output_manager_v2_send_logical_transform(
          manager_resource, output_id, metrics.logical_transform);
      zaura_output_manager_v2_send_panel_transform(manager_resource, output_id,
                                                   metrics.panel_transform);
      zaura_output_manager_v2_send_name(manager_resource, output_id,
                                        metrics.name.c_str());
      zaura_output_manager_v2_send_description(manager_resource, output_id,
                                               metrics.description.c_str());
    });
  }

  void SendDoneOnServerThread() {
    PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
      wl_resource* manager_resource =
          server->zaura_output_manager_v2()->resource();
      zaura_output_manager_v2_send_done(manager_resource);
    });
  }

  void DisableSendImplicitDoneOnServerThread() {
    PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
      server->zaura_output_manager_v2()->set_send_done_on_config_change(false);
    });
  }

  const WaylandOutputManager* wayland_output_manager() const {
    const auto* wayland_output_manager = connection_->wayland_output_manager();
    EXPECT_TRUE(wayland_output_manager);
    return wayland_output_manager;
  }

  WaylandZAuraOutputManagerV2* aura_output_manager_v2() {
    auto* const aura_output_manager_v2 = connection_->zaura_output_manager_v2();
    EXPECT_TRUE(aura_output_manager_v2);
    return aura_output_manager_v2;
  }

  WaylandOutput* primary_output() {
    return wayland_output_manager()->GetPrimaryOutput();
  }
};

// Tests to ensure server output metrics events translate to a correct metrics
// representation client-side.
TEST_F(WaylandZAuraOutputManagerV2Test, ServerEventsPopulateOutputMetrics) {
  const WaylandOutput::Id output_id = primary_output()->output_id();
  ASSERT_TRUE(wayland_output_manager()->GetOutput(output_id));

  WaylandOutput::Metrics server_metrics;
  server_metrics.output_id = output_id;
  server_metrics.display_id = primary_output()->GetMetrics().display_id;
  server_metrics.origin = gfx::Point(10, 20);
  server_metrics.scale_factor = 1;
  server_metrics.logical_size = gfx::Size(100, 200);
  server_metrics.physical_size = gfx::Size(100, 200);
  server_metrics.insets = gfx::Insets(10);
  server_metrics.panel_transform = WL_OUTPUT_TRANSFORM_90;
  server_metrics.logical_transform = WL_OUTPUT_TRANSFORM_180;
  server_metrics.name = "DisplayName";
  server_metrics.description = "DisplayDiscription";

  SendMetricsOnServerThread(server_metrics);

  // Metrics sent mid-transaction should accumulate in the pending metrics map
  // and not the current metrics map.
  const auto get_pending_client_metrics = [&]() {
    return aura_output_manager_v2()
        ->pending_output_metrics_map_for_testing()
        .at(output_id);
  };
  const auto get_client_metrics = [&]() {
    return aura_output_manager_v2()->output_metrics_map_for_testing().at(
        output_id);
  };
  EXPECT_EQ(server_metrics, get_pending_client_metrics());
  EXPECT_NE(server_metrics, get_client_metrics());

  // After the done event the pending metrics should be moved to the current
  // metrics map.
  SendDoneOnServerThread();

  EXPECT_EQ(server_metrics, get_pending_client_metrics());
  EXPECT_EQ(server_metrics, get_client_metrics());
}

// Tests that the aura output manager correctly processes config changes for
// wl_output global add events.
TEST_F(WaylandZAuraOutputManagerV2Test, ProcessesOutputGlobalAdd) {
  // Create a new output on the server thread, withholding the done event.
  WaylandOutput::Id secondary_output_id = 0;
  DisableSendImplicitDoneOnServerThread();
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    secondary_output_id =
        server->CreateAndInitializeOutput()->GetOutputName(server->client());
  });

  // The secondary output should appear in the wayland output manager registry
  // but should not be flagged as "ready" since the output config transaction
  // has not yet completed.
  WaylandOutput* secondary_output =
      wayland_output_manager()->GetOutput(secondary_output_id);
  ASSERT_TRUE(secondary_output);
  EXPECT_FALSE(secondary_output->IsReady());

  // The secondary output should be flagged as ready after the done event has
  // been received, marking  the end of the transaction.
  SendDoneOnServerThread();

  EXPECT_TRUE(secondary_output->IsReady());
}

// Tests that the aura output manager correctly processes config changes for
// wl_output global remove events.
TEST_F(WaylandZAuraOutputManagerV2Test, ProcessesOutputGlobalRemove) {
  const WaylandOutput::Id output_id = primary_output()->output_id();
  ASSERT_TRUE(wayland_output_manager()->GetOutput(output_id));

  // Destroy the output server-side but do not yet send the aura output manager
  // done event.
  DisableSendImplicitDoneOnServerThread();
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->output()->DestroyGlobal();
  });

  // The client-side output should still be valid client-side.
  EXPECT_TRUE(wayland_output_manager()->GetOutput(output_id));
  EXPECT_TRUE(wayland_output_manager()->GetOutput(output_id)->IsReady());

  // After the done event the output should be removed from its client-side
  // registry.
  SendDoneOnServerThread();

  EXPECT_FALSE(wayland_output_manager()->GetOutput(output_id));
}

// Tests that the aura output manager correctly processes config changes for
// output adds, removals and changes simultaneously.
TEST_F(WaylandZAuraOutputManagerV2Test, MultipleOutputChangeTransaction) {
  // Create an environment with two outputs.
  WaylandOutput::Id primary_output_id = 0;
  wl::TestOutput* secondary_test_output = nullptr;
  WaylandOutput::Id secondary_output_id = 0;
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    primary_output_id = server->output()->GetOutputName(server->client());
    secondary_test_output = server->CreateAndInitializeOutput();
    secondary_output_id =
        secondary_test_output->GetOutputName(server->client());
  });

  WaitForAllDisplaysReady();
  EXPECT_EQ(2u, wayland_output_manager()->GetAllOutputs().size());
  EXPECT_TRUE(
      wayland_output_manager()->GetOutput(primary_output_id)->IsReady());
  EXPECT_TRUE(
      wayland_output_manager()->GetOutput(secondary_output_id)->IsReady());

  // Setup an output configuration change transaction as follows, but suppress
  // the done event.
  // 1. Adding a tertiary output
  // 2. Removing the secondary output
  // 3. Updating the primary output's metrics.

  // 1. Add the tertiary output
  WaylandOutput::Id tertiary_output_id = 0;
  DisableSendImplicitDoneOnServerThread();
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    tertiary_output_id =
        server->CreateAndInitializeOutput()->GetOutputName(server->client());
  });

  // 2. Remove the secondary output.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    secondary_test_output->DestroyGlobal();
  });

  // 3. Create updated output metrics for the primary output.
  WaylandOutput* primary_output =
      wayland_output_manager()->GetOutput(primary_output_id);
  WaylandOutput::Metrics primary_server_metrics;
  primary_server_metrics.output_id = primary_output_id;
  primary_server_metrics.display_id = primary_output->GetMetrics().display_id;
  primary_server_metrics.origin = gfx::Point(10, 20);
  primary_server_metrics.scale_factor = 1;
  primary_server_metrics.logical_size = gfx::Size(100, 200);
  primary_server_metrics.physical_size = gfx::Size(100, 200);
  primary_server_metrics.insets = gfx::Insets(10);
  primary_server_metrics.panel_transform = WL_OUTPUT_TRANSFORM_90;
  primary_server_metrics.logical_transform = WL_OUTPUT_TRANSFORM_180;
  primary_server_metrics.name = "DisplayName";
  primary_server_metrics.description = "DisplayDiscription";
  SendMetricsOnServerThread(primary_server_metrics);

  // Validate the transaction has not yet been applied by the client.

  // Added tertiary output is not yet ready.
  WaylandOutput* tertiary_output =
      wayland_output_manager()->GetOutput(tertiary_output_id);
  ASSERT_TRUE(tertiary_output);
  EXPECT_FALSE(tertiary_output->IsReady());

  // Removed secondary output is still registered and ready.
  WaylandOutput* secondary_output =
      wayland_output_manager()->GetOutput(secondary_output_id);
  EXPECT_TRUE(secondary_output);
  EXPECT_TRUE(secondary_output->IsReady());

  // Updated primary output metrics have not been applied.
  const auto get_primary_client_metrics = [&]() {
    return aura_output_manager_v2()->output_metrics_map_for_testing().at(
        primary_output_id);
  };
  EXPECT_NE(primary_server_metrics, get_primary_client_metrics());

  // Send a done event to signal the end of the current transaction.
  SendDoneOnServerThread();

  // Validate the transaction has been applied.

  // Added tertiary output is registered and ready.
  tertiary_output = wayland_output_manager()->GetOutput(tertiary_output_id);
  ASSERT_TRUE(tertiary_output);
  EXPECT_TRUE(tertiary_output->IsReady());

  // Removed secondary output has been deregistered.
  EXPECT_FALSE(wayland_output_manager()->GetOutput(secondary_output_id));

  // Updated primary metrics are applied.
  EXPECT_EQ(primary_server_metrics, get_primary_client_metrics());
}

// Asserts output activations are correctly translated to the client's screen.
TEST_F(WaylandZAuraOutputManagerV2Test, ActiveDisplay) {
  WaylandTestScreen test_screen(wayland_output_manager()->wayland_screen());

  wl::TestOutput* primary_output = nullptr;
  wl::TestOutput* secondary_output = nullptr;
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    primary_output = server->output();
    secondary_output = server->CreateAndInitializeOutput();
  });

  WaitForAllDisplaysReady();

  WaylandScreen* platform_screen = wayland_output_manager()->wayland_screen();
  ASSERT_TRUE(platform_screen);
  ASSERT_EQ(2u, platform_screen->GetAllDisplays().size());
  const int64_t primary_display_id = platform_screen->GetAllDisplays()[0].id();
  const int64_t secondary_display_id =
      platform_screen->GetAllDisplays()[1].id();

  // Activate the secondary output.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    server->zaura_output_manager_v2()->SendActivated(secondary_output);
  });
  EXPECT_EQ(secondary_display_id,
            display::Screen::GetScreen()->GetDisplayForNewWindows().id());

  // Activate the primary output.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    server->zaura_output_manager_v2()->SendActivated(primary_output);
  });
  EXPECT_EQ(primary_display_id,
            display::Screen::GetScreen()->GetDisplayForNewWindows().id());
}

}  // namespace ui
