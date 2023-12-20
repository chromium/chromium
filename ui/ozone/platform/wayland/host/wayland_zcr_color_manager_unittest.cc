// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome-color-management-client-protocol.h>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_management_output.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_space.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_wayland_zcr_color_manager.h"
#include "ui/ozone/platform/wayland/test/test_wayland_zcr_color_management_output.h"
#include "ui/ozone/platform/wayland/test/test_wayland_zcr_color_management_surface.h"
#include "ui/ozone/platform/wayland/test/test_wayland_zcr_color_space.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::Values;

namespace ui {
namespace {

constexpr uint32_t kAugmentedSurfaceNotSupportedVersion = 0;

base::ScopedFD MakeFD() {
  base::FilePath temp_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&temp_path));
  auto file =
      base::File(temp_path, base::File::FLAG_READ | base::File::FLAG_WRITE |
                                base::File::FLAG_CREATE_ALWAYS);
  return base::ScopedFD(file.TakePlatformFile());
}

class WaylandZcrColorManagerTest : public WaylandTest {
 public:
  WaylandZcrColorManagerTest() = default;
  WaylandZcrColorManagerTest(const WaylandZcrColorManagerTest&) = delete;
  WaylandZcrColorManagerTest& operator=(const WaylandZcrColorManagerTest&) =
      delete;
  ~WaylandZcrColorManagerTest() override = default;

  void SetUp() override {
    WaylandTest::SetUp();
    ASSERT_TRUE(connection_->zcr_color_manager());
  }

  WaylandWindow* window() { return window_.get(); }
};

}  // namespace

TEST_P(WaylandZcrColorManagerTest, CreateColorManagementOutput) {
  // Set default values for the output.
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    wl::TestOutput* output = server->output();
    output->SetPhysicalAndLogicalBounds({800, 600});
    output->Flush();
  });

  auto* output_manager = connection_->wayland_output_manager();
  WaylandOutput* wayland_output = output_manager->GetPrimaryOutput();
  ASSERT_TRUE(wayland_output);
  EXPECT_TRUE(wayland_output->IsReady());

  auto* color_management_output = wayland_output->color_management_output();
  ASSERT_TRUE(color_management_output);
  // Set HDR10 on server output, notify color management output with event.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto params_vector =
        server->zcr_color_manager_v1()->color_management_outputs();
    for (wl::TestZcrColorManagementOutputV1* mock_params : params_vector) {
      mock_params->SetGfxColorSpace(gfx::ColorSpace::CreateHDR10());
      zcr_color_management_output_v1_send_color_space_changed(
          mock_params->resource());
    }
  });
  // Allow output to respond to server event, then send color space events.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto params_vector =
        server->zcr_color_manager_v1()->color_management_outputs();
    for (wl::TestZcrColorManagementOutputV1* mock_params : params_vector) {
      auto* zcr_color_space = mock_params->GetZcrColorSpace();
      // assert that the color space is the same as the one in output.
      EXPECT_EQ(zcr_color_space->GetGfxColorSpace(),
                gfx::ColorSpace::CreateHDR10());
      // send HDR10 over wayland.
      zcr_color_space_v1_send_names(zcr_color_space->resource(), 5, 5, 3);
      zcr_color_space_v1_send_done(zcr_color_space->resource());
    }
  });

  // Check that the received color space is equal to the one on server output.
  auto* gfx_color_space = color_management_output->gfx_color_space();
  gfx_color_space->ToString();
  EXPECT_EQ(*gfx_color_space, gfx::ColorSpace::CreateHDR10());
}

TEST_P(WaylandZcrColorManagerTest, CreateColorManagementSurface) {
  auto* surface = window_.get()->root_surface();
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto params_vector =
        server->zcr_color_manager_v1()->color_management_surfaces();
    for (wl::TestZcrColorManagementSurfaceV1* mock_params : params_vector) {
      EXPECT_EQ(gfx::ColorSpace::CreateSRGB(), mock_params->GetGfxColorSpace());
    }
  });

  // Updated buffer handle needed for ApplyPendingState() to set color_space
  EXPECT_TRUE(connection_->buffer_manager_host());
  auto interface_ptr = connection_->buffer_manager_host()->BindInterface();
  buffer_manager_gpu_->Initialize(std::move(interface_ptr), {},
                                  /*supports_dma_buf=*/false,
                                  /*supports_viewporter=*/true,
                                  /*supports_acquire_fence=*/false,
                                  /*supports_overlays=*/true,
                                  kAugmentedSurfaceNotSupportedVersion,
                                  /*supports_single_pixel_buffer=*/true,
                                  /*server_version=*/{});

  // Setup wl_buffers.
  constexpr uint32_t buffer_id = 1;
  gfx::Size buffer_size(1024, 768);
  auto length = 1024 * 768 * 4;
  buffer_manager_gpu_->CreateShmBasedBuffer(MakeFD(), length, buffer_size,
                                            buffer_id);
  base::RunLoop().RunUntilIdle();

  // preload color_space in color manager cache, since first call with a
  // new color_space always returns null.
  connection_->zcr_color_manager()->GetColorSpace(
      gfx::ColorSpace::CreateHDR10());
  connection_->RoundTripQueue();
  surface->set_color_space(gfx::ColorSpace::CreateHDR10());
  surface->AttachBuffer(connection_->buffer_manager_host()->EnsureBufferHandle(
      surface, buffer_id));
  surface->ApplyPendingState();
  // Assert that surface on server has correct color_space.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto params_vector =
        server->zcr_color_manager_v1()->color_management_surfaces();
    EXPECT_EQ(gfx::ColorSpace::CreateHDR10(),
              params_vector.front()->GetGfxColorSpace());
  });
}

TEST_P(WaylandZcrColorManagerTest, DoNotSetInvaliColorSpace) {
  auto* surface = window_.get()->root_surface();
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto params_vector =
        server->zcr_color_manager_v1()->color_management_surfaces();
    for (wl::TestZcrColorManagementSurfaceV1* mock_params : params_vector) {
      EXPECT_EQ(gfx::ColorSpace::CreateSRGB(), mock_params->GetGfxColorSpace());
    }
  });

  // Updated buffer handle needed for ApplyPendingState() to set color_space
  EXPECT_TRUE(connection_->buffer_manager_host());
  auto interface_ptr = connection_->buffer_manager_host()->BindInterface();
  buffer_manager_gpu_->Initialize(std::move(interface_ptr), {},
                                  /*supports_dma_buf=*/false,
                                  /*supports_viewporter=*/true,
                                  /*supports_acquire_fence=*/false,
                                  /*supports_overlays=*/true,
                                  kAugmentedSurfaceNotSupportedVersion,
                                  /*supports_single_pixel_buffer=*/true,
                                  /*server_version=*/{});

  // Setup wl_buffers.
  constexpr uint32_t buffer_id = 1;
  gfx::Size buffer_size(1024, 768);
  auto length = 1024 * 768 * 4;
  buffer_manager_gpu_->CreateShmBasedBuffer(MakeFD(), length, buffer_size,
                                            buffer_id);
  base::RunLoop().RunUntilIdle();

  gfx::ColorSpace invalid_space =
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::INVALID,
                      gfx::ColorSpace::TransferID::INVALID);
  // Attempt to set an invalid color space on the surface and expect that the
  // original default color space remains unchanged on the surface.
  connection_->zcr_color_manager()->GetColorSpace(invalid_space);
  connection_->RoundTripQueue();
  surface->set_color_space(invalid_space);
  surface->AttachBuffer(connection_->buffer_manager_host()->EnsureBufferHandle(
      surface, buffer_id));
  surface->ApplyPendingState();
  // Assert that surface on server has correct color_space.
  PostToServerAndWait([&](wl::TestWaylandServerThread* server) {
    auto params_vector =
        server->zcr_color_manager_v1()->color_management_surfaces();
    EXPECT_EQ(gfx::ColorSpace::CreateSRGB(),
              params_vector.front()->GetGfxColorSpace());
  });
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandZcrColorManagerTest,
                         Values(wl::ServerConfig{}));

}  // namespace ui
