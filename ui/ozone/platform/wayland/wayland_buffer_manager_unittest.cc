// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <drm_fourcc.h>
#include <overlay-prioritizer-client-protocol.h>

#include <cstdint>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/platform/wayland/common/wayland_overlay_config.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_gpu.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_frame_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_linux_dmabuf.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_linux_dmabuf.h"
#include "ui/ozone/platform/wayland/test/test_overlay_prioritized_surface.h"
#include "ui/ozone/platform/wayland/test/test_zwp_linux_buffer_params.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/platform_window/platform_window_init_properties.h"

using testing::_;
using testing::Truly;
using testing::Values;

namespace ui {

namespace {

using MockTerminateGpuCallback =
    base::MockCallback<base::OnceCallback<void(std::string)>>;

constexpr gfx::Size kDefaultSize(1024, 768);

constexpr uint32_t kAugmentedSurfaceNotSupportedVersion = 0;

// TODO(msisov): add a test to exercise buffer management with non-default scale
// once all the patches land.
constexpr float kDefaultScale = 1.f;

struct InputData {
  bool has_file = false;
  gfx::Size size;
  uint32_t planes_count = 0;
  std::vector<uint32_t> strides;
  std::vector<uint32_t> offsets;
  std::vector<uint64_t> modifiers;
  uint32_t format = 0;
  uint32_t buffer_id = 0;
};

class MockSurfaceGpu : public WaylandSurfaceGpu {
 public:
  MockSurfaceGpu(WaylandBufferManagerGpu* buffer_manager,
                 gfx::AcceleratedWidget widget)
      : buffer_manager_(buffer_manager), widget_(widget) {
    buffer_manager_->RegisterSurface(widget_, this);
  }

  MockSurfaceGpu(const MockSurfaceGpu&) = delete;
  MockSurfaceGpu& operator=(const MockSurfaceGpu&) = delete;

  ~MockSurfaceGpu() override { buffer_manager_->UnregisterSurface(widget_); }

  MOCK_METHOD3(OnSubmission,
               void(uint32_t buffer_id,
                    const gfx::SwapResult& swap_result,
                    gfx::GpuFenceHandle release_fence));
  MOCK_METHOD2(OnPresentation,
               void(uint32_t buffer_id,
                    const gfx::PresentationFeedback& feedback));

 private:
  const raw_ptr<WaylandBufferManagerGpu> buffer_manager_;
  const gfx::AcceleratedWidget widget_;
};

}  // namespace

class WaylandBufferManagerTest : public WaylandTest {
 public:
  WaylandBufferManagerTest() = default;
  WaylandBufferManagerTest(const WaylandBufferManagerTest&) = delete;
  WaylandBufferManagerTest& operator=(const WaylandBufferManagerTest&) = delete;
  ~WaylandBufferManagerTest() override = default;

  void SetUp() override {
    // Surface submission in pixel coordinates is only checked once on surface
    // creation and persisted, so we must make sure the configuration is done
    // before we create the surface.
    connection_->set_surface_submission_in_pixel_coordinates(
        GetParam().surface_submission_in_pixel_coordinates);
    connection_->set_supports_viewporter_surface_scaling(
        GetParam().supports_viewporter_surface_scaling);

    WaylandTest::SetUp();

    manager_host_ = connection_->buffer_manager_host();
    EXPECT_TRUE(manager_host_);

    // Use the helper methods below, which automatically set the termination
    // callback and bind the interface again if the manager failed.
    manager_host_->SetTerminateGpuCallback(callback_.Get());
    auto interface_ptr = manager_host_->BindInterface();
    buffer_manager_gpu_->Initialize(std::move(interface_ptr), {},
                                    /*supports_dma_buf=*/false,
                                    /*supports_viewporter=*/true,
                                    /*supports_acquire_fence=*/false,
                                    /*supports_overlays=*/true,
                                    kAugmentedSurfaceNotSupportedVersion,
                                    /*supports_single_pixel_buffer=*/true,
                                    /*server_version=*/{});
    surface_id_ = window_->root_surface()->get_surface_id();
  }

  void TearDown() override {
    // MockGpuSurface will unregister itself from the WaylandBuffermanagerGpu,
    // which will remove objects associated with this surface on another thread.
    // This run loop gives it a chance to do that.
    base::RunLoop().RunUntilIdle();
    WaylandTest::TearDown();
  }

 protected:
  void SendFrameCallbackForSurface(uint32_t surface_id) {
    PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
      auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
      ASSERT_TRUE(mock_surface);
      mock_surface->SendFrameCallback();
    });
  }

  // Might be null. Make sure to not manipulate the returned resource on the
  // client thread. Though, if the client does that, the server is able to catch
  // calls for libwayland-server on a wrong thread.
  wl_resource* GetSurfaceAttachedBuffer(uint32_t surface_id) {
    wl_resource* wl_buffer = nullptr;
    PostToServerAndWait(
        [&wl_buffer, surface_id](wl::TestWaylandServerThread* server) {
          auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
          ASSERT_TRUE(mock_surface);
          wl_buffer = mock_surface->attached_buffer();
        });
    return wl_buffer;
  }

  base::ScopedFD MakeFD() {
    base::FilePath temp_path;
    EXPECT_TRUE(base::CreateTemporaryFile(&temp_path));
    auto file =
        base::File(temp_path, base::File::FLAG_READ | base::File::FLAG_WRITE |
                                  base::File::FLAG_CREATE_ALWAYS);
    return base::ScopedFD(file.TakePlatformFile());
  }

  // Sets the terminate gpu callback expectation, calls OnChannelDestroyed,
  // sets the same callback again and re-establishes mojo connection again
  // for convenience.
  void SetTerminateCallbackExpectationAndDestroyChannel(
      MockTerminateGpuCallback* callback,
      bool fail) {
    channel_destroyed_error_message_.clear();

    if (!fail) {
      // To avoid warning messages as "Expected to be never called, but has 0
      // WillOnce()s", split the expecations based on the expected call times.
      EXPECT_CALL(*callback, Run(_)).Times(0);
    } else {
      EXPECT_CALL(*callback, Run(_))
          .Times(1)
          .WillRepeatedly(
              ::testing::Invoke([this, callback](std::string error_string) {
                channel_destroyed_error_message_ = error_string;

                manager_host_->OnChannelDestroyed();

                manager_host_->SetTerminateGpuCallback(callback->Get());

                auto interface_ptr = manager_host_->BindInterface();
                // Recreate the gpu side manager (the production code does the
                // same).
                buffer_manager_gpu_ =
                    std::make_unique<WaylandBufferManagerGpu>();
                buffer_manager_gpu_->Initialize(
                    std::move(interface_ptr), {},
                    /*supports_dma_buf=*/false,
                    /*supports_viewporter=*/true,
                    /*supports_acquire_fence=*/false,
                    /*supports_overlays=*/true,
                    kAugmentedSurfaceNotSupportedVersion,
                    /*supports_single_pixel_buffer=*/true,
                    /*server_version=*/{});
              }));
    }
  }

  void CreateDmabufBasedBufferAndSetTerminateExpectation(
      bool fail,
      uint32_t buffer_id,
      base::ScopedFD fd = base::ScopedFD(),
      const gfx::Size& size = kDefaultSize,
      const std::vector<uint32_t>& strides = {1},
      const std::vector<uint32_t>& offsets = {2},
      const std::vector<uint64_t>& modifiers = {3},
      uint32_t format = DRM_FORMAT_R8,
      uint32_t planes_count = 1) {
    if (!fd.is_valid())
      fd = MakeFD();

    SetTerminateCallbackExpectationAndDestroyChannel(&callback_, fail);
    buffer_manager_gpu_->CreateDmabufBasedBuffer(
        std::move(fd), kDefaultSize, strides, offsets, modifiers, format,
        planes_count, buffer_id);

    base::RunLoop().RunUntilIdle();
  }

  void CreateShmBasedBufferAndSetTerminateExpecation(
      bool fail,
      uint32_t buffer_id,
      const gfx::Size& size = kDefaultSize,
      size_t length = 0) {
    SetTerminateCallbackExpectationAndDestroyChannel(&callback_, fail);

    if (!length)
      length = size.width() * size.height() * 4;
    buffer_manager_gpu_->CreateShmBasedBuffer(MakeFD(), length, size,
                                              buffer_id);

    base::RunLoop().RunUntilIdle();
  }

  void DestroyBufferAndSetTerminateExpectation(uint32_t buffer_id, bool fail) {
    SetTerminateCallbackExpectationAndDestroyChannel(&callback_, fail);

    buffer_manager_gpu_->DestroyBuffer(buffer_id);

    base::RunLoop().RunUntilIdle();
  }

  void ProcessCreatedBufferResourcesWithExpectation(size_t expected_size,
                                                    bool fail) {
    PostToServerAndWait(
        [expected_size, fail](wl::TestWaylandServerThread* server) {
          auto params_vector = server->zwp_linux_dmabuf_v1()->buffer_params();
          // To ensure, no other buffers are created, test the size of the
          // vector.
          EXPECT_EQ(params_vector.size(), expected_size);

          for (wl::TestZwpLinuxBufferParamsV1* mock_params : params_vector) {
            if (!fail) {
              zwp_linux_buffer_params_v1_send_created(
                  mock_params->resource(), mock_params->buffer_resource());
            } else {
              zwp_linux_buffer_params_v1_send_failed(mock_params->resource());
            }
          }
        });
  }

  std::unique_ptr<WaylandWindow> CreateWindow(
      PlatformWindowType type = PlatformWindowType::kWindow,
      gfx::AcceleratedWidget parent_widget = gfx::kNullAcceleratedWidget) {
    testing::Mock::VerifyAndClearExpectations(&delegate_);
    PlatformWindowInitProperties properties;
    properties.bounds = gfx::Rect(0, 0, 800, 600);
    properties.type = type;
    properties.parent_widget = parent_widget;
    auto new_window = WaylandWindow::Create(&delegate_, connection_.get(),
                                            std::move(properties));
    EXPECT_TRUE(new_window);
    WaylandTestBase::SyncDisplay();

    EXPECT_NE(new_window->GetWidget(), gfx::kNullAcceleratedWidget);
    return new_window;
  }

  wl::WaylandOverlayConfig CreateBasicWaylandOverlayConfig(
      int z_order,
      uint32_t buffer_id,
      const gfx::Rect& bounds_rect) {
    return CreateBasicWaylandOverlayConfig(z_order, buffer_id,
                                           gfx::RectF(bounds_rect));
  }

  wl::WaylandOverlayConfig CreateBasicWaylandOverlayConfig(
      int z_order,
      uint32_t buffer_id,
      const gfx::RectF& bounds_rect) {
    wl::WaylandOverlayConfig config;
    config.z_order = z_order;
    config.buffer_id = buffer_id;
    config.bounds_rect = bounds_rect;
    config.damage_region = gfx::ToEnclosedRect(bounds_rect);
    return config;
  }

  void CommitBuffer(gfx::AcceleratedWidget widget,
                    uint32_t frame_id,
                    uint32_t buffer_id,
                    gfx::FrameData data,
                    const gfx::Rect& bounds_rect,
                    const gfx::RoundedCornersF& corners,
                    float surface_scale_factor,
                    const gfx::Rect& damage_region) {
    buffer_manager_gpu_->CommitBuffer(
        widget, frame_id, buffer_id, data, bounds_rect, /*enable_blend=*/false,
        corners, surface_scale_factor, damage_region);
    // Let the mojo message to be processed.
    base::RunLoop().RunUntilIdle();
  }

  MockTerminateGpuCallback callback_;
  raw_ptr<WaylandBufferManagerHost> manager_host_;
  // Error message that is received when the manager_host destroys the channel.
  std::string channel_destroyed_error_message_;

  uint32_t surface_id_ = 0u;
};

TEST_P(WaylandBufferManagerTest, CreateDmabufBasedBuffers) {
  constexpr uint32_t kDmabufBufferId = 1;

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });

  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                    kDmabufBufferId);
  DestroyBufferAndSetTerminateExpectation(kDmabufBufferId, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, VerifyModifiers) {
  constexpr uint32_t kDmabufBufferId = 1;
  constexpr uint32_t kFourccFormatR8 = DRM_FORMAT_R8;
  constexpr uint64_t kFormatModiferLinear = DRM_FORMAT_MOD_LINEAR;

  const std::vector<uint64_t> kFormatModifiers{DRM_FORMAT_MOD_INVALID,
                                               kFormatModiferLinear};

  // Tests that fourcc format is added, but invalid modifier is ignored first.
  // Then, when valid modifier comes, it is stored.
  for (const auto& modifier : kFormatModifiers) {
    PostToServerAndWait([modifier](wl::TestWaylandServerThread* server) {
      uint32_t modifier_hi = modifier >> 32;
      uint32_t modifier_lo = modifier & UINT32_MAX;
      zwp_linux_dmabuf_v1_send_modifier(
          server->zwp_linux_dmabuf_v1()->resource(), kFourccFormatR8,
          modifier_hi, modifier_lo);
    });

    auto buffer_formats =
        connection_->buffer_factory()->GetSupportedBufferFormats();
    ASSERT_EQ(buffer_formats.size(), 1u);
    ASSERT_EQ(buffer_formats.begin()->first,
              GetBufferFormatFromFourCCFormat(kFourccFormatR8));

    auto modifiers = buffer_formats.begin()->second;
    if (modifier == DRM_FORMAT_MOD_INVALID) {
      ASSERT_EQ(modifiers.size(), 0u);
    } else {
      ASSERT_EQ(modifiers.size(), 1u);
      ASSERT_EQ(modifiers[0], modifier);
    }
  }

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });

  CreateDmabufBasedBufferAndSetTerminateExpectation(
      false /*fail*/, kDmabufBufferId, base::ScopedFD(), kDefaultSize, {1}, {2},
      {kFormatModiferLinear}, kFourccFormatR8, 1);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto params_vector = server->zwp_linux_dmabuf_v1()->buffer_params();
    EXPECT_EQ(params_vector.size(), 1u);
    EXPECT_EQ(params_vector[0]->modifier_hi_, kFormatModiferLinear >> 32);
    EXPECT_EQ(params_vector[0]->modifier_lo_,
              kFormatModiferLinear & UINT32_MAX);
  });

  // Clean up.
  DestroyBufferAndSetTerminateExpectation(kDmabufBufferId, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, CreateShmBasedBuffers) {
  constexpr uint32_t kShmBufferId = 1;

  CreateShmBasedBufferAndSetTerminateExpecation(false /*fail*/, kShmBufferId);

  DestroyBufferAndSetTerminateExpectation(kShmBufferId, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, ValidateDataFromGpu) {
  const InputData kBadInputs[] = {
      // All zeros.
      {},
      // Valid file but zeros everywhereelse.
      {true},
      // Valid file, invalid size, zeros elsewhere.
      {true, {kDefaultSize.width(), 0}},
      {true, {0, kDefaultSize.height()}},
      // Valid file and size but zeros in other fields.
      {true, kDefaultSize},
      // Vectors have different lengths.
      {true, kDefaultSize, 1, {1}, {2, 3}, {4, 5, 6}},
      // Vectors have same lengths but strides have a zero.
      {true, kDefaultSize, 1, {0}, {2}, {6}},
      // Vectors are valid but buffer format is not.
      {true, kDefaultSize, 1, {1}, {2}, {6}},
      // Everything is correct but the buffer ID is zero.
      {true, kDefaultSize, 1, {1}, {2}, {6}, DRM_FORMAT_R8},
  };

  for (const auto& bad : kBadInputs) {
    EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(0);
    base::ScopedFD dummy;
    CreateDmabufBasedBufferAndSetTerminateExpectation(
        true /*fail*/, bad.buffer_id,
        bad.has_file ? MakeFD() : std::move(dummy), bad.size, bad.strides,
        bad.offsets, bad.modifiers, bad.format, bad.planes_count);
  }
}

TEST_P(WaylandBufferManagerTest, CreateAndDestroyBuffer) {
  const uint32_t kBufferId1 = 1;
  const uint32_t kBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();

  // This section tests that it is impossible to create buffers with the same
  // id if they haven't been assigned to any surfaces yet.
  {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _))
          .Times(2);
    });
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId1);
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId2);

    // Can't create buffer with existing id.
    CreateDmabufBasedBufferAndSetTerminateExpectation(true /*fail*/,
                                                      kBufferId2);
  }

  // ... impossible to create buffers with the same id if one of them
  // has already been attached to a surface.
  {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _))
          .Times(1);
    });
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId1);

    CommitBuffer(widget, kBufferId1, kBufferId1,
                 gfx::FrameData(delegate_.viz_seq()),
                 window_->GetBoundsInPixels(), gfx::RoundedCornersF(),
                 kDefaultScale, gfx::Rect(window_->applied_state().size_px));

    CreateDmabufBasedBufferAndSetTerminateExpectation(true /*fail*/,
                                                      kBufferId1);
  }

  // ... impossible to destroy non-existing buffer.
  {
    // Either it is attached...
    DestroyBufferAndSetTerminateExpectation(kBufferId1, true /*fail*/);

    // Or not attached.
    DestroyBufferAndSetTerminateExpectation(kBufferId1, true /*fail*/);
  }

  // Can destroy the buffer without specifying the widget.
  {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _))
          .Times(1);
    });
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId1);

    CommitBuffer(widget, kBufferId1, kBufferId1,
                 gfx::FrameData(delegate_.viz_seq()),
                 window_->GetBoundsInPixels(), gfx::RoundedCornersF(),
                 kDefaultScale, gfx::Rect(window_->applied_state().size_px));

    DestroyBufferAndSetTerminateExpectation(kBufferId1, false /*fail*/);
  }

  // Still can destroy the buffer even if it has not been attached to any
  // widgets.
  {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _))
          .Times(1);
    });
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId1);
    DestroyBufferAndSetTerminateExpectation(kBufferId1, false /*fail*/);
  }

  // ... impossible to destroy buffers twice.
  {
    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _))
          .Times(3);
    });
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId1);
    // Attach to a surface.
    CommitBuffer(widget, kBufferId1, kBufferId1,
                 gfx::FrameData(delegate_.viz_seq()),
                 window_->GetBoundsInPixels(), gfx::RoundedCornersF(),
                 kDefaultScale, gfx::Rect(window_->applied_state().size_px));

    // Created non-attached buffer as well.
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId2);

    DestroyBufferAndSetTerminateExpectation(kBufferId1, false /*fail*/);
    // Can't destroy the buffer with non-existing id (the manager cleared the
    // state after the previous failure).
    DestroyBufferAndSetTerminateExpectation(kBufferId1, true /*fail*/);

    // Non-attached buffer must have been also destroyed (we can't destroy it
    // twice) if there was a failure.
    DestroyBufferAndSetTerminateExpectation(kBufferId2, true /*fail*/);

    // Create and destroy non-attached buffer twice.
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId2);
    DestroyBufferAndSetTerminateExpectation(kBufferId2, false /*fail*/);
    DestroyBufferAndSetTerminateExpectation(kBufferId2, true /*fail*/);
  }
}

TEST_P(WaylandBufferManagerTest, CommitBufferNonExistingBufferId) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, 1u);
  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);

    // Can't commit for non-existing buffer id.
    constexpr uint32_t kNumberOfCommits = 0;
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Commit()).Times(kNumberOfCommits);
  });

  CommitBuffer(window_->GetWidget(), 1u, 5u,
               gfx::FrameData(delegate_.viz_seq()),
               window_->GetBoundsInPixels(), gfx::RoundedCornersF(),
               kDefaultScale, gfx::Rect(window_->applied_state().size_px));

  // Let the mojo call to go through.
  base::RunLoop().RunUntilIdle();

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    testing::Mock::VerifyAndClearExpectations(
        server->GetObject<wl::MockSurface>(id));
  });
}

TEST_P(WaylandBufferManagerTest, CommitOverlaysNonExistingBufferId) {
  EXPECT_CALL(*server_.zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, 1u);
  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);

    // Can't commit for non-existing buffer id.
    constexpr uint32_t kNumberOfCommits = 0;
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Commit()).Times(kNumberOfCommits);
  });

  std::vector<wl::WaylandOverlayConfig> overlay_configs;
  if (connection_->ShouldUseOverlayDelegation()) {
    overlay_configs.emplace_back(CreateBasicWaylandOverlayConfig(
        INT32_MIN, 1u, window_->GetBoundsInPixels()));
  }
  // Non-existing buffer id
  overlay_configs.emplace_back(
      CreateBasicWaylandOverlayConfig(0, 2u, window_->GetBoundsInPixels()));
  buffer_manager_gpu_->CommitOverlays(window_->GetWidget(), 1u,
                                      gfx::FrameData(delegate_.viz_seq()),
                                      std::move(overlay_configs));

  // Let the mojo call to go through.
  base::RunLoop().RunUntilIdle();

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    testing::Mock::VerifyAndClearExpectations(
        server->GetObject<wl::MockSurface>(id));
  });
}

TEST_P(WaylandBufferManagerTest, CommitOverlaysWithSameBufferId) {
  if (!connection_->ShouldUseOverlayDelegation()) {
    GTEST_SKIP();
  }

  const size_t expected_number_of_buffers =
      connection_->linux_explicit_synchronization_v1() ? 1 : 2;

  PostToServerAndWait(
      [expected_number_of_buffers](wl::TestWaylandServerThread* server) {
        EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _))
            .Times(expected_number_of_buffers);
      });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, 1u);

  // Re-using the same buffer id across multiple surfaces is allowed.
  SetTerminateCallbackExpectationAndDestroyChannel(&callback_, false /*fail*/);

  std::vector<wl::WaylandOverlayConfig> overlay_configs;
  overlay_configs.emplace_back(
      CreateBasicWaylandOverlayConfig(0, 1u, window_->GetBoundsInPixels()));
  overlay_configs.emplace_back(
      CreateBasicWaylandOverlayConfig(1, 1u, window_->GetBoundsInPixels()));

  buffer_manager_gpu_->CommitOverlays(window_->GetWidget(), 1u,
                                      gfx::FrameData(delegate_.viz_seq()),
                                      std::move(overlay_configs));

  // Let the mojo call to go through.
  base::RunLoop().RunUntilIdle();

  ProcessCreatedBufferResourcesWithExpectation(
      expected_number_of_buffers /* expected size */, false /* fail */);
}

TEST_P(WaylandBufferManagerTest, CommitBufferNullWidget) {
  constexpr uint32_t kBufferId = 1;
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId);

  // Can't commit for non-existing widget.
  SetTerminateCallbackExpectationAndDestroyChannel(&callback_, true /*fail*/);
  CommitBuffer(gfx::kNullAcceleratedWidget, 1u, kBufferId,
               gfx::FrameData(delegate_.viz_seq()),
               window_->GetBoundsInPixels(), gfx::RoundedCornersF(),
               kDefaultScale, gfx::Rect(window_->applied_state().size_px));

  // Let the mojo call to go through.
  base::RunLoop().RunUntilIdle();
}

// Tests that committing overlays with bounds_rect containing NaN or infinity
// values is illegal - the host terminates the gpu process.
TEST_P(WaylandBufferManagerTest, CommitOverlaysNonsensicalBoundsRect) {
  if (!connection_->ShouldUseOverlayDelegation()) {
    GTEST_SKIP();
  }

  const std::vector<gfx::RectF> bounds_rect_test_data = {
      gfx::RectF(std::nanf(""), window_->GetBoundsInPixels().y(), std::nanf(""),
                 window_->GetBoundsInPixels().height()),
      gfx::RectF(window_->GetBoundsInPixels().x(),
                 std::numeric_limits<float>::infinity(),
                 window_->GetBoundsInPixels().width(),
                 std::numeric_limits<float>::infinity())};

  constexpr bool config[2] = {/*root_has_nan_bounds=*/true,
                              /*non_root_overlay_has_nan_bounds=*/false};
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;
  constexpr uint32_t kBufferId3 = 3;

  for (bool should_root_have_nan_bounds : config) {
    for (const auto& faulty_bounds_rect : bounds_rect_test_data) {
      CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                        kBufferId1);
      CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                        kBufferId2);
      CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                        kBufferId3);
      ProcessCreatedBufferResourcesWithExpectation(3u /* expected size */,
                                                   false /* fail */);

      // Can't commit for bounds rect containing NaN
      SetTerminateCallbackExpectationAndDestroyChannel(&callback_,
                                                       true /*fail*/);

      size_t z_order = 0;
      std::vector<wl::WaylandOverlayConfig> overlay_configs;
      if (should_root_have_nan_bounds) {
        // The root surface has nan bounds.
        overlay_configs.emplace_back(CreateBasicWaylandOverlayConfig(
            INT32_MIN, kBufferId1, faulty_bounds_rect));
        overlay_configs.emplace_back(CreateBasicWaylandOverlayConfig(
            z_order++, kBufferId2, window_->GetBoundsInPixels()));
        overlay_configs.emplace_back(CreateBasicWaylandOverlayConfig(
            z_order++, kBufferId3, window_->GetBoundsInPixels()));
      } else {
        // Overlays have nan bounds. Given playback starts with the biggest
        // z-order number, add two more overlays around the faulty overlay
        // config so that the test ensures no further playback happens and it
        // doesn't crash.
        overlay_configs.emplace_back(CreateBasicWaylandOverlayConfig(
            INT32_MIN, kBufferId1, window_->GetBoundsInPixels()));
        overlay_configs.emplace_back(CreateBasicWaylandOverlayConfig(
            z_order++, kBufferId2, window_->GetBoundsInPixels()));
        overlay_configs.emplace_back(CreateBasicWaylandOverlayConfig(
            z_order++, kBufferId3, faulty_bounds_rect));
        overlay_configs.emplace_back(CreateBasicWaylandOverlayConfig(
            z_order++, kBufferId2, window_->GetBoundsInPixels()));
      }
      buffer_manager_gpu_->CommitOverlays(window_->GetWidget(), 1u,
                                          gfx::FrameData(delegate_.viz_seq()),
                                          std::move(overlay_configs));

      base::RunLoop().RunUntilIdle();

      if (!should_root_have_nan_bounds &&
          !connection_->linux_explicit_synchronization_v1()) {
        // This case submits kBufferId2 twice. So, a second handle is requested
        // during a frame playback if explicit sync is unavailable.
        ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                                     false /* fail */);
      }

      EXPECT_EQ("Overlay bounds_rect is invalid (NaN or infinity).",
                channel_destroyed_error_message_);

      std::vector<uint32_t> ids_of_subsurfaces;
      ids_of_subsurfaces.reserve(window_->wayland_subsurfaces_.size());
      for (auto& subsurface : window_->wayland_subsurfaces_) {
        ids_of_subsurfaces.emplace_back(
            subsurface->wayland_surface()->get_surface_id());
      }

      PostToServerAndWait([id = surface_id_, ids_of_subsurfaces](
                              wl::TestWaylandServerThread* server) {
        // Clear all the possible frame and release callbacks.
        auto* mock_surface = server->GetObject<wl::MockSurface>(id);
        for (auto subsurface_id : ids_of_subsurfaces) {
          auto* mock_surface_of_subsurface =
              server->GetObject<wl::MockSurface>(subsurface_id);
          EXPECT_TRUE(mock_surface_of_subsurface);
          mock_surface_of_subsurface->SendFrameCallback();
          mock_surface_of_subsurface->ClearBufferReleases();
        }

        mock_surface->SendFrameCallback();
        mock_surface->ClearBufferReleases();
      });
    }
  }
}

// A similar test to the above with the only difference that a background
// dummy buffer is not used as delegation is disabled.
TEST_P(WaylandBufferManagerTest,
       CommitOverlaysNonsensicalBoundsRectSingleOverlay) {
  if (connection_->ShouldUseOverlayDelegation()) {
    GTEST_SKIP();
  }

  const std::vector<gfx::RectF> bounds_rect_test_data = {
      gfx::RectF(std::nanf(""), window_->GetBoundsInPixels().y(), std::nanf(""),
                 window_->GetBoundsInPixels().height()),
      gfx::RectF(window_->GetBoundsInPixels().x(),
                 std::numeric_limits<float>::infinity(),
                 window_->GetBoundsInPixels().width(),
                 std::numeric_limits<float>::infinity())};

  for (const auto& faulty_bounds_rect : bounds_rect_test_data) {
    constexpr uint32_t kBufferId1 = 1;

    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId1);
    ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                                 false /* fail */);

    // Can't commit for bounds rect containing NaN
    SetTerminateCallbackExpectationAndDestroyChannel(&callback_, true /*fail*/);

    size_t z_order = 0;
    std::vector<wl::WaylandOverlayConfig> overlay_configs;

    overlay_configs.emplace_back(CreateBasicWaylandOverlayConfig(
        z_order, kBufferId1, faulty_bounds_rect));

    buffer_manager_gpu_->CommitOverlays(window_->GetWidget(), 1u,
                                        gfx::FrameData(delegate_.viz_seq()),
                                        std::move(overlay_configs));

    base::RunLoop().RunUntilIdle();

    EXPECT_EQ("Overlay bounds_rect is invalid (NaN or infinity).",
              channel_destroyed_error_message_);
  }
}

TEST_P(WaylandBufferManagerTest, EnsureCorrectOrderOfCallbacks) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = gfx::Rect({0, 0}, kDefaultSize);
  window_->SetBoundsInDIP(bounds);

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget_);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(2);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);

  ProcessCreatedBufferResourcesWithExpectation(2u /* expected size */,
                                               false /* fail */);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);

    constexpr uint32_t kNumberOfCommits = 3;
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Commit()).Times(kNumberOfCommits);
  });

  // All the other expectations must come in order.
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  // wp_presentation must not exist now. This means that the buffer
  // manager must send synthetized presentation feedbacks.
  ASSERT_TRUE(!connection_->presentation());
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  base::RunLoop().RunUntilIdle();

  // As long as there hasn't any previous buffer attached (nothing to release
  // yet), it must be enough to just send a frame callback back.
  SendFrameCallbackForSurface(surface_id_);

  // Commit second buffer now.
  CommitBuffer(widget, kBufferId2, kBufferId2,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(1);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());
    mock_surface->SendFrameCallback();
  });

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    // wp_presentation is available now.
    auto* mock_wp_presentation = server->EnsureAndGetWpPresentation();
    ASSERT_TRUE(mock_wp_presentation);

    EXPECT_CALL(*mock_wp_presentation,
                Feedback(_, _, mock_surface->resource(), _))
        .Times(1);
  });

  // Now, the wp_presentation object exists and there must be a real feedback
  // sent. Ensure the order now.
  ASSERT_TRUE(connection_->presentation());

  // Commit second buffer now.
  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  base::RunLoop().RunUntilIdle();

  // Even though, the server send the presentation feedback, the host manager
  // must make sure the order of the submission and presentation callbacks is
  // correct. Thus, no callbacks must be received by the MockSurfaceGpu.
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->EnsureAndGetWpPresentation()->SendPresentationCallback();
  });

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    // Now, send the release callback. The host manager must send the submission
    // and presentation callbacks in correct order.
    mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());
  });

  DestroyBufferAndSetTerminateExpectation(kBufferId1, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(kBufferId2, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest,
       DestroyedBuffersGeneratePresentationFeedbackFailure) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;
  constexpr uint32_t kBufferId3 = 3;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = gfx::Rect({0, 0}, kDefaultSize);
  window_->SetBoundsInDIP(bounds);

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget_);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(3);
  });

  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId3);
  ProcessCreatedBufferResourcesWithExpectation(3u /* expected size */,
                                               false /* fail */);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    ASSERT_TRUE(mock_surface);
    auto* mock_wp_presentation = server->EnsureAndGetWpPresentation();
    ASSERT_TRUE(mock_wp_presentation);

    constexpr uint32_t kNumberOfCommits = 3;
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Commit()).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_wp_presentation,
                Feedback(_, _, mock_surface->resource(), _))
        .Times(3);
  });

  ::testing::InSequence s;

  // wp_presentation_feedback should work now.
  ASSERT_TRUE(connection_->presentation());

  // Commit the first buffer and expect OnSubmission immediately.
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  SendFrameCallbackForSurface(surface_id_);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Deliberately drop the presentation feedback for the first buffer,
    // since we will destroy it.
    auto* mock_wp_presentation = server->EnsureAndGetWpPresentation();
    EXPECT_EQ(1u, mock_wp_presentation->num_of_presentation_callbacks());
    mock_wp_presentation->DropPresentationCallback();
  });

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Commit second buffer now.
  CommitBuffer(widget, kBufferId2, kBufferId2,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  SendFrameCallbackForSurface(surface_id_);

  // Destroy the first buffer, which should trigger submission for the second
  // buffer.
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  DestroyBufferAndSetTerminateExpectation(kBufferId1, /*fail=*/false);
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    mock_surface->DestroyPrevAttachedBuffer();
    mock_surface->SendFrameCallback();
  });

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Deliberately drop the presentation feedback for the second buffer,
    // since we will destroy it.
    auto* mock_wp_presentation = server->EnsureAndGetWpPresentation();
    EXPECT_EQ(1u, mock_wp_presentation->num_of_presentation_callbacks());
    mock_wp_presentation->DropPresentationCallback();
  });

  // Commit buffer 3 then send the presentation callback for it. This should
  // not call OnPresentation as OnSubmission hasn't been called yet. Though,
  // the previously submitted buffers will have their presentation feedback
  // discarded and sent to the |mock_surface_gpu| as those buffers have already
  // been acked.
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _, _)).Times(0);
  EXPECT_CALL(
      mock_surface_gpu,
      OnPresentation(
          kBufferId1,
          ::testing::Field(
              &gfx::PresentationFeedback::flags,
              ::testing::Eq(gfx::PresentationFeedback::Flags::kFailure))))
      .Times(1);
  EXPECT_CALL(
      mock_surface_gpu,
      OnPresentation(
          kBufferId2,
          ::testing::Field(
              &gfx::PresentationFeedback::flags,
              ::testing::Eq(gfx::PresentationFeedback::Flags::kFailure))))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);
  CommitBuffer(widget, kBufferId3, kBufferId3,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    server->GetObject<wl::MockSurface>(id)->SendFrameCallback();
    server->EnsureAndGetWpPresentation()->SendPresentationCallback();
  });

  // Ensure that presentation feedback is flushed.
  task_environment_.FastForwardBy(
      WaylandFrameManager::GetPresentationFlushTimerDurationForTesting());

  // Verify our expecations.
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Destroy buffer 2, which should trigger OnSubmission for buffer 3, and
  // finally OnPresentation for buffer 3.
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId3, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId3, _)).Times(1);
  DestroyBufferAndSetTerminateExpectation(kBufferId2, /*fail=*/false);
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    mock_surface->DestroyPrevAttachedBuffer();
    mock_surface->SendFrameCallback();
    server->EnsureAndGetWpPresentation()->SendPresentationCallback();
  });

  // Let the mojo messages to be processed.
  // No need to fast forward to ensure the presentation flush timer is fired,
  // because in this case the OnSubmission should piggyback the feedback.
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  DestroyBufferAndSetTerminateExpectation(kBufferId3, false /*fail*/);
}

// This test ensures that a discarded presentation feedback sent prior receiving
// results for the previous presentation feedback does not make them
// automatically failed.
TEST_P(WaylandBufferManagerTest,
       EnsureDiscardedPresentationDoesNotMakePreviousFeedbacksFailed) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;
  constexpr uint32_t kBufferId3 = 3;

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Enable wp_presentation support.
    auto* mock_wp_presentation = server->EnsureAndGetWpPresentation();
    ASSERT_TRUE(mock_wp_presentation);
  });

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = gfx::Rect({0, 0}, kDefaultSize);
  window_->SetBoundsInDIP(bounds);

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget_);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(3);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId3);
  ProcessCreatedBufferResourcesWithExpectation(3u /* expected size */,
                                               false /* fail */);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);

    constexpr uint32_t kNumberOfCommits = 3;
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Commit()).Times(kNumberOfCommits);
  });

  // All the other expectations must come in order.
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  // Commit first buffer
  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  // Let the mojo message for OnSubmission go back.
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    // Verify we have a presentation callback now. This will be sent later.
    EXPECT_EQ(
        1u,
        server->EnsureAndGetWpPresentation()->num_of_presentation_callbacks());

    server->GetObject<wl::MockSurface>(id)->SendFrameCallback();
  });

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  // Commit second buffer
  CommitBuffer(widget, kBufferId2, kBufferId2,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    // Verify we have a presentation callback now. This will be sent later.
    EXPECT_EQ(
        2u,
        server->EnsureAndGetWpPresentation()->num_of_presentation_callbacks());

    // Release previous buffer and commit third buffer.
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());
    mock_surface->SendFrameCallback();
  });

  // Let the mojo message for OnSubmission go back.
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId3, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  // Commit third buffer
  CommitBuffer(widget, kBufferId3, kBufferId3,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());
  });

  // Let the mojo messages to be processed.
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Even though WaylandBufferManagerHost stores the previous stores
  // presentation feedbacks and waits for their value, the current last one
  // mustn't result in the previous marked as failed. Thus, no feedback must be
  // actually sent to the MockSurfaceGpu as it's required to send feedbacks in
  // order.
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* mock_wp_presentation = server->EnsureAndGetWpPresentation();
    EXPECT_EQ(3u, mock_wp_presentation->num_of_presentation_callbacks());
    mock_wp_presentation->SendPresentationCallbackDiscarded(/*last=*/true);
  });

  // Let the mojo messages to be processed if any to ensure there are no pending
  // messages.
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Now, start to send all the previous callbacks.
  EXPECT_CALL(mock_surface_gpu,
              OnPresentation(
                  kBufferId1,
                  ::testing::Field(
                      &gfx::PresentationFeedback::flags,
                      ::testing::Eq(gfx::PresentationFeedback::Flags::kVSync))))
      .Times(1);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* mock_wp_presentation = server->EnsureAndGetWpPresentation();
    EXPECT_EQ(2u, mock_wp_presentation->num_of_presentation_callbacks());
    mock_wp_presentation->SendPresentationCallback();
  });

  // Ensure that presentation feedback is flushed.
  task_environment_.FastForwardBy(
      WaylandFrameManager::GetPresentationFlushTimerDurationForTesting());

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Now, send the second presentation feedback. It will send both second and
  // third feedback that was discarded.
  EXPECT_CALL(mock_surface_gpu,
              OnPresentation(
                  kBufferId2,
                  ::testing::Field(
                      &gfx::PresentationFeedback::flags,
                      ::testing::Eq(gfx::PresentationFeedback::Flags::kVSync))))
      .Times(1);
  EXPECT_CALL(
      mock_surface_gpu,
      OnPresentation(
          kBufferId3,
          ::testing::Field(
              &gfx::PresentationFeedback::flags,
              ::testing::Eq(gfx::PresentationFeedback::Flags::kFailure))))
      .Times(1);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    auto* mock_wp_presentation = server->EnsureAndGetWpPresentation();
    EXPECT_EQ(1u, mock_wp_presentation->num_of_presentation_callbacks());
    mock_wp_presentation->SendPresentationCallback();
  });

  // Ensure that presentation feedback is flushed.
  task_environment_.FastForwardBy(
      WaylandFrameManager::GetPresentationFlushTimerDurationForTesting());

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  DestroyBufferAndSetTerminateExpectation(kBufferId1, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(kBufferId2, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(kBufferId3, false /*fail*/);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// This test ensures that a failed presentation feedback (with flags set as
// `gfx::PresentationFeedback::kFailure`) is received by the client and
// propagated as a failed presentation.
TEST_P(WaylandBufferManagerTest, EnsureFailedPresentationIsSent) {
  constexpr uint32_t kBufferId1 = 1;

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Enable wp_presentation support.
    auto* mock_wp_presentation = server->EnsureAndGetWpPresentation();
    ASSERT_TRUE(mock_wp_presentation);
  });

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = gfx::Rect({0, 0}, kDefaultSize);
  window_->SetBoundsInDIP(bounds);

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget_);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);

    constexpr uint32_t kNumberOfCommits = 1;
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Commit()).Times(kNumberOfCommits);
  });

  // All the other expectations must come in order.
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  // Commit the buffer
  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  // Let the mojo message for OnSubmission go back.
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    // Verify we have a presentation callback now. This will be sent later.
    EXPECT_EQ(
        1u,
        server->EnsureAndGetWpPresentation()->num_of_presentation_callbacks());

    server->GetObject<wl::MockSurface>(id)->SendFrameCallback();
  });

  // Its all setup until this point.
  // The main part of the test starts here.
  EXPECT_CALL(
      mock_surface_gpu,
      OnPresentation(
          kBufferId1,
          ::testing::Field(
              &gfx::PresentationFeedback::flags,
              ::testing::Eq(gfx::PresentationFeedback::Flags::kFailure))))
      .Times(1);

  // Set `.flags` as `gfx::PresentationFeedback::kFailure`.
  wl::MockWpPresentation::PresentationFeedbackParams params{
      .tv_sec_hi = 1,
      .tv_sec_lo = 1,
      .tv_nsec = 1,
      .refresh = 1,
      .seq_hi = 1,
      .seq_lo = 1,
      .flags = gfx::PresentationFeedback::kFailure};

  PostToServerAndWait([params](wl::TestWaylandServerThread* server) {
    auto* mock_wp_presentation = server->EnsureAndGetWpPresentation();
    EXPECT_EQ(1u, mock_wp_presentation->num_of_presentation_callbacks());
    mock_wp_presentation->SendPresentationFeedbackToClient(/*last=*/false,
                                                           params);
  });

  // Ensure that presentation feedback is flushed.
  task_environment_.FastForwardBy(
      WaylandFrameManager::GetPresentationFlushTimerDurationForTesting());

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  DestroyBufferAndSetTerminateExpectation(kBufferId1, false /*fail*/);
}
#endif

TEST_P(WaylandBufferManagerTest, TestCommitBufferConditions) {
  constexpr uint32_t kDmabufBufferId = 1;
  constexpr uint32_t kDmabufBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget_);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });

  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                    kDmabufBufferId);

  // Part 1: the surface mustn't have a buffer attached until
  // zwp_linux_buffer_params_v1_send_created is called. Instead, the buffer must
  // be set as pending buffer.

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(0);
    EXPECT_CALL(*mock_surface, Commit()).Times(0);
  });

  CommitBuffer(widget, kDmabufBufferId, kDmabufBufferId,
               gfx::FrameData(delegate_.viz_seq()),
               window_->GetBoundsInPixels(), gfx::RoundedCornersF(),
               kDefaultScale, gfx::Rect(window_->applied_state().size_px));

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
    EXPECT_CALL(*mock_surface, Commit()).Times(1);
  });

  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* linux_dmabuf = server->zwp_linux_dmabuf_v1();
    // Once the client receives a "...send_created" call, it must destroy the
    // params resource.
    EXPECT_TRUE(linux_dmabuf->buffer_params().empty());

    // Part 2: the surface mustn't have a buffer attached until frame callback
    // is sent by the server.

    EXPECT_CALL(*linux_dmabuf, CreateParams(_, _, _)).Times(1);

    testing::Mock::VerifyAndClearExpectations(
        server->GetObject<wl::MockSurface>(id));
  });

  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                    kDmabufBufferId2);

  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(0);
    EXPECT_CALL(*mock_surface, Commit()).Times(0);
  });

  CommitBuffer(widget, kDmabufBufferId2, kDmabufBufferId2,
               gfx::FrameData(delegate_.viz_seq()),
               window_->GetBoundsInPixels(), gfx::RoundedCornersF(),
               kDefaultScale, gfx::Rect(window_->applied_state().size_px));

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    testing::Mock::VerifyAndClearExpectations(mock_surface);
    // After the frame callback is sent, the pending buffer will be committed.
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
    EXPECT_CALL(*mock_surface, Commit()).Times(1);

    mock_surface->SendFrameCallback();
  });

  DestroyBufferAndSetTerminateExpectation(kDmabufBufferId, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(kDmabufBufferId2, false /*fail*/);
}

// Tests the surface does not have buffers attached until it's configured at
// least once.
TEST_P(WaylandBufferManagerTest, TestCommitBufferConditionsAckConfigured) {
  constexpr uint32_t kDmabufBufferId = 1;

  // Exercise two window types that create different windows - toplevel, menu.
  // subsurface windows do not create xdg_surfaces so no need to configure them.
  std::vector<PlatformWindowType> window_types{PlatformWindowType::kWindow,
                                               PlatformWindowType::kTooltip};

  for (const auto& type : window_types) {
    // If the type is not kWindow, provide default created window as parent of
    // the newly created window.
    auto temp_window = CreateWindow(type, type != PlatformWindowType::kWindow
                                              ? widget_
                                              : gfx::kNullAcceleratedWidget);
    auto widget = temp_window->GetWidget();

    temp_window->Show(false);

    const uint32_t temp_window_surface_id =
        temp_window->root_surface()->get_surface_id();
    MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _))
          .Times(1);
    });

    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kDmabufBufferId);

    PostToServerAndWait(
        [temp_window_surface_id](wl::TestWaylandServerThread* server) {
          auto* mock_surface =
              server->GetObject<wl::MockSurface>(temp_window_surface_id);
          auto* xdg_surface = mock_surface->xdg_surface();
          ASSERT_TRUE(xdg_surface);
        });
    ASSERT_FALSE(temp_window->IsSurfaceConfigured());

    ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                                 false /* fail */);

    PostToServerAndWait(
        [temp_window_surface_id](wl::TestWaylandServerThread* server) {
          auto* mock_surface =
              server->GetObject<wl::MockSurface>(temp_window_surface_id);
          auto* xdg_surface = mock_surface->xdg_surface();
          EXPECT_CALL(*xdg_surface, SetWindowGeometry(_)).Times(0);
          EXPECT_CALL(*xdg_surface, AckConfigure(_)).Times(0);
          EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
          EXPECT_CALL(*mock_surface, Frame(_)).Times(0);
          EXPECT_CALL(*mock_surface, Commit()).Times(0);
        });

    CommitBuffer(widget, kDmabufBufferId, kDmabufBufferId,
                 gfx::FrameData(delegate_.viz_seq()),
                 window_->GetBoundsInPixels(), gfx::RoundedCornersF(),
                 kDefaultScale, window_->GetBoundsInPixels());
    PostToServerAndWait(
        [temp_window_surface_id](wl::TestWaylandServerThread* server) {
          auto* mock_surface =
              server->GetObject<wl::MockSurface>(temp_window_surface_id);
          auto* xdg_surface = mock_surface->xdg_surface();
          testing::Mock::VerifyAndClearExpectations(mock_surface);

          EXPECT_CALL(*xdg_surface, SetWindowGeometry(_)).Times(0);
          EXPECT_CALL(*xdg_surface, AckConfigure(_)).Times(1);
          EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
          EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
          // Commit() can be called a second time as part of the configure->ack
          // sequence.
          EXPECT_CALL(*mock_surface, Commit()).Times(testing::Between(1, 2));
        });

    ActivateSurface(temp_window_surface_id);
    PostToServerAndWait(
        [temp_window_surface_id](wl::TestWaylandServerThread* server) {
          testing::Mock::VerifyAndClearExpectations(
              server->GetObject<wl::MockSurface>(temp_window_surface_id));
        });

    SetPointerFocusedWindow(nullptr);
    temp_window.reset();
    DestroyBufferAndSetTerminateExpectation(kDmabufBufferId, false /*fail*/);
  }
}

// Verifies toplevel surfaces do not have buffers attached until configured,
// even when the initial configure sequence is not acked in response to
// xdg_surface.configure event, i.e: done asynchronously when OnSequencePoint()
// is called by they FrameManager).
//
// Regression test for https://crbug.com/1313023.
TEST_P(WaylandBufferManagerTest,
       CommitBufferConditionsWithDeferredAckConfigure) {
  constexpr gfx::Rect kNormalBounds{800, 800};
  constexpr gfx::Rect kRestoredBounds{500, 500};
  constexpr uint32_t kDmabufBufferId = 1;

  testing::Mock::VerifyAndClearExpectations(&delegate_);
  PlatformWindowInitProperties properties;
  properties.type = PlatformWindowType::kWindow;
  properties.bounds = kNormalBounds;
  gfx::Insets insets;
  EXPECT_CALL(delegate_, CalculateInsetsInDIP(PlatformWindowState::kNormal))
      .WillRepeatedly(testing::Return(insets));
  auto window = WaylandWindow::Create(&delegate_, connection_.get(),
                                      std::move(properties));
  ASSERT_TRUE(window);
  ASSERT_NE(window->GetWidget(), gfx::kNullAcceleratedWidget);
  auto widget = window->GetWidget();

  // Set restored bounds to a value different from the initial window bounds in
  // order to force WaylandWindow::ProcessPendingConfigureState() to defer the
  // very first configure ack to be done in the subsequent OnSequencePoint()
  // call.
  window->SetRestoredBoundsInDIP(kRestoredBounds);
  WaylandTestBase::SyncDisplay();

  window->Show(false);

  const uint32_t surface_id = window->root_surface()->get_surface_id();
  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });

  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                    kDmabufBufferId);

  PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    auto* xdg_surface = mock_surface->xdg_surface();
    ASSERT_TRUE(xdg_surface);
  });
  ASSERT_FALSE(window->IsSurfaceConfigured());

  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  // Emulate the following steps:
  //
  // 1. A CommitBuffer request coming from the GPU service, with frame
  //    bounds that do not match the one stored in |pending_configures_| at Host
  //    side (filled when processing 0x0 initial configure sequence sent by the
  //    Wayland compositor.
  // 2. The initial configure sequence (i.e: with 0x0 size which means the
  //    client must suggest the initial geometry of the surface.
  // 3. And then a CommitBuffer with the expected bounds (ie: suggested to the
  //    Wayland compositor through a set_geometry/ack_configure sequence when
  //    processing (2).
  //
  // And ensures the xdg and wl_surface objects received the correct requests
  // amount. I.e: No buffer attaches before setting geometry + acking initial
  // configure sequence, etc.

  constexpr uint32_t kActivateSerial = 1u;
  PostToServerAndWait([surface_id, bounds = kRestoredBounds](
                          wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(surface_id);
    auto* xdg_surface = mock_surface->xdg_surface();
    EXPECT_CALL(*xdg_surface, SetWindowGeometry(bounds)).Times(1);
    EXPECT_CALL(*xdg_surface, AckConfigure(kActivateSerial)).Times(1);
    EXPECT_CALL(*mock_surface, Attach(_, 0, 0)).Times(1);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
    EXPECT_CALL(*mock_surface, Commit()).Times(1);
  });

  CommitBuffer(widget, kDmabufBufferId, kDmabufBufferId,
               gfx::FrameData(delegate_.viz_seq() - 1), gfx::Rect{55, 55},
               gfx::RoundedCornersF(), kDefaultScale, gfx::Rect{55, 55});
  ActivateSurface(surface_id, kActivateSerial);

  CommitBuffer(widget, kDmabufBufferId, kDmabufBufferId,
               gfx::FrameData(delegate_.viz_seq()), kRestoredBounds,
               gfx::RoundedCornersF(), kDefaultScale, kRestoredBounds);
  SetPointerFocusedWindow(nullptr);
  window.reset();
  DestroyBufferAndSetTerminateExpectation(kDmabufBufferId, false /*fail*/);
}

// The buffer that is not originally attached to any of the surfaces,
// must be attached when a commit request comes. Also, it must setup a buffer
// release listener and OnSubmission must be called for that buffer if it is
// released.
TEST_P(WaylandBufferManagerTest, AnonymousBufferAttachedAndReleased) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;
  constexpr uint32_t kBufferId3 = 3;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = gfx::Rect({0, 0}, kDefaultSize);
  window_->SetBoundsInDIP(bounds);

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget_);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);

  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);

    constexpr uint32_t kNumberOfCommits = 3;
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Commit()).Times(kNumberOfCommits);
  });

  // All the other expectations must come in order.
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  // Commit second buffer now.
  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  SendFrameCallbackForSurface(surface_id_);

  // Let mojo messages to be processed.
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Now synchronously create a second buffer and commit it. The release
    // callback must be setup and OnSubmission must be called.
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });

  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);

  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(1);

  // Commit second buffer now.
  CommitBuffer(widget, kBufferId2, kBufferId2,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());
  });

  SendFrameCallbackForSurface(surface_id_);

  // Let mojo messages to be processed.
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Now asynchronously create another buffer so that a commit request
  // comes earlier than it is created by the Wayland compositor, but it can
  // released once the buffer is committed and processed (that is, it must be
  // able to setup a buffer release callback).
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId3);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId3, gfx::SwapResult::SWAP_ACK, _))
      .Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId3, _)).Times(0);

  CommitBuffer(widget, kBufferId3, kBufferId3,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  // Let mojo messages to be processed from host to gpu (if any).
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId3, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId3, _)).Times(1);

  // Now, create the buffer from the Wayland compositor side and let the buffer
  // manager complete the commit request.
  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());
  });

  // Let mojo messages to be processed.
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  DestroyBufferAndSetTerminateExpectation(kBufferId1, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(kBufferId2, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(kBufferId3, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, DestroyBufferForDestroyedWindow) {
  constexpr uint32_t kBufferId = 1;

  auto temp_window = CreateWindow();
  auto widget = temp_window->GetWidget();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId);

  CommitBuffer(widget, kBufferId, kBufferId,
               gfx::FrameData(delegate_.viz_seq()),
               temp_window->GetBoundsInPixels(), gfx::RoundedCornersF(),
               kDefaultScale, temp_window->GetBoundsInPixels());

  temp_window.reset();
  DestroyBufferAndSetTerminateExpectation(kBufferId, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, DestroyedWindowNoSubmissionSingleBuffer) {
  constexpr uint32_t kBufferId = 1;

  auto temp_window = CreateWindow();
  auto widget = temp_window->GetWidget();
  auto bounds = temp_window->GetBoundsInPixels();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId);

  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

  // All the other expectations must come in order.
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  temp_window.reset();

  CommitBuffer(widget, kBufferId, kBufferId,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  DestroyBufferAndSetTerminateExpectation(kBufferId, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, DestroyedWindowNoSubmissionMultipleBuffers) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;

  auto temp_window = CreateWindow();
  temp_window->Show(false);

  auto widget = temp_window->GetWidget();
  auto bounds = temp_window->GetBoundsInPixels();

  const uint32_t temp_window_surface_id =
      temp_window->root_surface()->get_surface_id();
  PostToServerAndWait(
      [temp_window_surface_id](wl::TestWaylandServerThread* server) {
        ASSERT_TRUE(server->GetObject<wl::MockSurface>(temp_window_surface_id));
      });

  ActivateSurface(temp_window_surface_id);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

  // All the other expectations must come in order.
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _, _)).Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(1);

  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  // Let mojo messages to be processed back to the gpu.
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  PostToServerAndWait([temp_window_surface_id](
                          wl::TestWaylandServerThread* server) {
    server->GetObject<wl::MockSurface>(temp_window_surface_id)
        ->SendFrameCallback();
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });

  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);
  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(1);

  CommitBuffer(widget, kBufferId2, kBufferId2,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  PostToServerAndWait(
      [temp_window_surface_id](wl::TestWaylandServerThread* server) {
        auto* mock_surface =
            server->GetObject<wl::MockSurface>(temp_window_surface_id);
        mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());
      });

  // Let mojo messages to be processed back to the gpu.
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);
  temp_window.reset();

  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  // Let mojo messages to be processed back to the gpu.
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  DestroyBufferAndSetTerminateExpectation(kBufferId1, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(kBufferId2, false /*fail*/);
}

// Tests that OnSubmission and OnPresentation are properly triggered if a buffer
// is committed twice in a row and those buffers are destroyed.
TEST_P(WaylandBufferManagerTest, DestroyBufferCommittedTwiceInARow) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = window_->GetBoundsInPixels();

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(2);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);

  ProcessCreatedBufferResourcesWithExpectation(2u /* expected size */,
                                               false /* fail */);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  SendFrameCallbackForSurface(surface_id_);

  // Let mojo messages to be processed back to the gpu.
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Can't call OnSubmission until there is a release.
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  CommitBuffer(widget, kBufferId2, kBufferId2,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);
  SendFrameCallbackForSurface(surface_id_);

  CommitBuffer(widget, kBufferId2, kBufferId2,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);
  SendFrameCallbackForSurface(surface_id_);

  // Destroying buffer2 should do nothing yet.
  DestroyBufferAndSetTerminateExpectation(kBufferId2, false /*fail*/);

  // Let mojo messages to be processed back to the gpu.
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Destroying buffer1 should give us two acks for buffer2.
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK, _))
      .Times(2);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(2);
  DestroyBufferAndSetTerminateExpectation(kBufferId1, false /*fail*/);

  // Let mojo messages to be processed back to the gpu.
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);
}

// Tests that OnSubmission and OnPresentation are properly triggered if a buffer
// is committed twice in a row and then released.
TEST_P(WaylandBufferManagerTest, ReleaseBufferCommittedTwiceInARow) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = window_->GetBoundsInPixels();

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(2);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);
  ProcessCreatedBufferResourcesWithExpectation(2u /* expected size */,
                                               false /* fail */);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);
  SendFrameCallbackForSurface(surface_id_);

  // Let mojo messages to be processed back to the gpu.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Even though, this is stored on the client thread, but must only be used on
  // the server thread.
  wl_resource* wl_buffer1 = GetSurfaceAttachedBuffer(surface_id_);
  ASSERT_TRUE(wl_buffer1);

  // Can't call OnSubmission until there is a release.
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  CommitBuffer(widget, kBufferId2, kBufferId2,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);
  SendFrameCallbackForSurface(surface_id_);

  CommitBuffer(widget, kBufferId2, kBufferId2,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  // Let mojo messages to be processed back to the gpu.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Releasing buffer1 should trigger two acks for buffer2.
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK, _))
      .Times(2);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(2);
  PostToServerAndWait(
      [wl_buffer1, id = surface_id_](wl::TestWaylandServerThread* server) {
        server->GetObject<wl::MockSurface>(id)->ReleaseBuffer(wl_buffer1);
      });

  // Let mojo messages to be processed back to the gpu.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  DestroyBufferAndSetTerminateExpectation(kBufferId1, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(kBufferId2, false /*fail*/);
}

// Tests that OnSubmission and OnPresentation callbacks are properly called
// even if buffers are not released in the same order they were committed.
TEST_P(WaylandBufferManagerTest, ReleaseOrderDifferentToCommitOrder) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;
  constexpr uint32_t kBufferId3 = 3;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = window_->GetBoundsInPixels();

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(3);
  });

  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId3);
  ProcessCreatedBufferResourcesWithExpectation(3u /* expected size */,
                                               false /* fail */);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->GetObject<wl::MockSurface>(id), Attach(_, _, _))
        .Times(1);
  });

  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  SendFrameCallbackForSurface(surface_id_);

  // Let mojo messages to be processed and passed from host to gpu.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Even though, this is stored on the client thread, but must only be used on
  // the server thread.
  wl_resource* wl_buffer1 = GetSurfaceAttachedBuffer(surface_id_);
  ASSERT_TRUE(wl_buffer1);

  // Can't call OnSubmission until there is a release.
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->GetObject<wl::MockSurface>(id), Attach(_, _, _))
        .Times(2);
  });

  CommitBuffer(widget, kBufferId2, kBufferId2,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  SendFrameCallbackForSurface(surface_id_);

  // Even though, this is stored on the client thread, but must only be used on
  // the server thread.
  wl_resource* wl_buffer2 = GetSurfaceAttachedBuffer(surface_id_);
  ASSERT_TRUE(wl_buffer2);

  CommitBuffer(widget, kBufferId3, kBufferId3,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);
  SendFrameCallbackForSurface(surface_id_);

  // Let mojo messages to be processed and passed from host to gpu.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Releasing buffer2 can't trigger OnSubmission for buffer3, because
  // OnSubmission for buffer2 has not been sent yet.
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);
  PostToServerAndWait(
      [wl_buffer2, id = surface_id_](wl::TestWaylandServerThread* server) {
        server->GetObject<wl::MockSurface>(id)->ReleaseBuffer(wl_buffer2);
      });

  // Let mojo messages to be processed and passed from host to gpu.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Releasing buffer1 should trigger acks for both.
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(1);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId3, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId3, _)).Times(1);
  PostToServerAndWait(
      [wl_buffer1, id = surface_id_](wl::TestWaylandServerThread* server) {
        server->GetObject<wl::MockSurface>(id)->ReleaseBuffer(wl_buffer1);
      });

  // Let mojo messages to be processed and passed from host to gpu.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  DestroyBufferAndSetTerminateExpectation(kBufferId1, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(kBufferId2, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(kBufferId3, false /*fail*/);
}

// This test verifies that submitting the buffer more than once results in
// OnSubmission callback as Wayland compositor is not supposed to release the
// buffer committed twice.
TEST_P(WaylandBufferManagerTest,
       OnSubmissionCalledForBufferCommitedTwiceInARow) {
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = window_->GetBoundsInPixels();

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(2);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);
  ProcessCreatedBufferResourcesWithExpectation(2u /* expected size */,
                                               false /* fail */);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  ASSERT_TRUE(!connection_->presentation());
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  PostToServerAndWait(
      [id = surface_id_, bounds](wl::TestWaylandServerThread* server) {
        auto* mock_surface = server->GetObject<wl::MockSurface>(id);
        EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
        EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
        EXPECT_CALL(*mock_surface,
                    Damage(0, 0, bounds.width(), bounds.height()))
            .Times(1);
        EXPECT_CALL(*mock_surface, Commit()).Times(1);
      });

  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  // Let mojo messages to be passed from host to gpu.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  PostToServerAndWait(
      [id = surface_id_, bounds](wl::TestWaylandServerThread* server) {
        auto* mock_surface = server->GetObject<wl::MockSurface>(id);
        testing::Mock::VerifyAndClearExpectations(mock_surface);
        mock_surface->SendFrameCallback();

        EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
        EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
        EXPECT_CALL(*mock_surface,
                    Damage(0, 0, bounds.width(), bounds.height()))
            .Times(1);
        EXPECT_CALL(*mock_surface, Commit()).Times(1);
      });

  // Commit second buffer now.
  CommitBuffer(widget, kBufferId2, kBufferId2,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  // Let mojo messages to be passed from host to gpu.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(1);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    testing::Mock::VerifyAndClearExpectations(mock_surface);

    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(0);
    EXPECT_CALL(*mock_surface, Damage(_, _, _, _)).Times(0);
    EXPECT_CALL(*mock_surface, Commit()).Times(0);

    mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());
    mock_surface->SendFrameCallback();
  });

  // Let mojo messages to be passed from host to gpu.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Now, commit the buffer with the |kBufferId2| again. The manager does not
  // sends the submission callback, the compositor is not going to release a
  // buffer as it was the same buffer submitted more than once.
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId2, _)).Times(1);

  PostToServerAndWait(
      [id = surface_id_, bounds](wl::TestWaylandServerThread* server) {
        auto* mock_surface = server->GetObject<wl::MockSurface>(id);
        testing::Mock::VerifyAndClearExpectations(mock_surface);

        EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
        EXPECT_CALL(*mock_surface, Frame(_)).Times(0);
        EXPECT_CALL(*mock_surface,
                    Damage(0, 0, bounds.width(), bounds.height()))
            .Times(1);
        EXPECT_CALL(*mock_surface, Commit()).Times(1);
      });

  // Commit second buffer now.
  CommitBuffer(widget, kBufferId2, kBufferId2,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  // Let mojo messages to be passed from host to gpu.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    testing::Mock::VerifyAndClearExpectations(mock_surface);

    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(0);
    EXPECT_CALL(*mock_surface, Damage(_, _, _, _)).Times(0);
    EXPECT_CALL(*mock_surface, Commit()).Times(0);

    mock_surface->SendFrameCallback();
  });

  // Let mojo messages to be passed from host to gpu.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // If we commit another buffer now, the manager host must not automatically
  // trigger OnSubmission and OnPresentation callbacks.
  EXPECT_CALL(mock_surface_gpu, OnSubmission(_, _, _)).Times(0);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);

  PostToServerAndWait(
      [id = surface_id_, bounds](wl::TestWaylandServerThread* server) {
        auto* mock_surface = server->GetObject<wl::MockSurface>(id);
        testing::Mock::VerifyAndClearExpectations(mock_surface);

        EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
        EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
        EXPECT_CALL(*mock_surface,
                    Damage(0, 0, bounds.width(), bounds.height()))
            .Times(1);
        EXPECT_CALL(*mock_surface, Commit()).Times(1);
      });

  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  // Let mojo messages to be passed from host to gpu.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Now, they must be triggered once the buffer is released.
  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    testing::Mock::VerifyAndClearExpectations(mock_surface);

    mock_surface->ReleaseBuffer(mock_surface->prev_attached_buffer());
    mock_surface->SendFrameCallback();
  });

  // Let mojo messages to be passed from host to gpu.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  DestroyBufferAndSetTerminateExpectation(kBufferId1, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(kBufferId2, false /*fail*/);
}

// Tests that submitting a single buffer only receives an OnSubmission. This is
// required behaviour to make sure that submitting buffers in a quiescent state
// will be immediately acked.
TEST_P(WaylandBufferManagerTest, OnSubmissionCalledForSingleBuffer) {
  constexpr uint32_t kBufferId1 = 1;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = window_->GetBoundsInPixels();

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);

  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  EXPECT_CALL(mock_surface_gpu,
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(mock_surface_gpu, OnPresentation(kBufferId1, _)).Times(1);

  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  // Let mojo messages to be passed from host to gpu.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  DestroyBufferAndSetTerminateExpectation(kBufferId1, false /*fail*/);
}

// Tests that when CommitOverlays(), root_surface can only be committed once all
// overlays in the frame are committed.
TEST_P(WaylandBufferManagerTest, RootSurfaceIsCommittedLast) {
  if (!connection_->ShouldUseOverlayDelegation()) {
    GTEST_SKIP();
  }

  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;
  constexpr uint32_t kBufferId3 = 3;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = window_->GetBoundsInPixels();

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(3);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId3);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* linux_dmabuf = server->zwp_linux_dmabuf_v1();
    // Ack creation for only the first 2 wl_buffers.
    zwp_linux_buffer_params_v1_send_created(
        linux_dmabuf->buffer_params()[0]->resource(),
        linux_dmabuf->buffer_params()[0]->buffer_resource());
    zwp_linux_buffer_params_v1_send_created(
        linux_dmabuf->buffer_params()[1]->resource(),
        linux_dmabuf->buffer_params()[1]->buffer_resource());

    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    // root_surface shall not be committed as one of its subsurface is not
    // committed yet due to pending wl_buffer creation.
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(0);
    EXPECT_CALL(*mock_surface, Commit()).Times(0);
  });

  std::vector<wl::WaylandOverlayConfig> overlay_configs;
  overlay_configs.emplace_back(
      CreateBasicWaylandOverlayConfig(INT32_MIN, kBufferId1, bounds));
  overlay_configs.emplace_back(
      CreateBasicWaylandOverlayConfig(0, kBufferId2, bounds));
  overlay_configs.emplace_back(
      CreateBasicWaylandOverlayConfig(1, kBufferId3, bounds));
  buffer_manager_gpu_->CommitOverlays(window_->GetWidget(), 1u,
                                      gfx::FrameData(delegate_.viz_seq()),
                                      std::move(overlay_configs));
  // Let mojo messages from gpu to host to go through.
  base::RunLoop().RunUntilIdle();

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    testing::Mock::VerifyAndClearExpectations(mock_surface);

    auto* linux_dmabuf = server->zwp_linux_dmabuf_v1();
    // Once wl_buffer is created, all subsurfaces are committed, hence
    // root_surface can be committed.
    zwp_linux_buffer_params_v1_send_created(
        linux_dmabuf->buffer_params()[0]->resource(),
        linux_dmabuf->buffer_params()[0]->buffer_resource());
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
    EXPECT_CALL(*mock_surface, Commit()).Times(1);
  });
}

TEST_P(WaylandBufferManagerTest, FencedRelease) {
  if (!connection_->linux_explicit_synchronization_v1())
    GTEST_SKIP();

  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;
  constexpr uint32_t kBufferId3 = 3;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = gfx::Rect({0, 0}, kDefaultSize);
  window_->SetBoundsInDIP(bounds);

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget_);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(3);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId3);
  ProcessCreatedBufferResourcesWithExpectation(3u /* expected size */,
                                               false /* fail */);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);

    constexpr uint32_t kNumberOfCommits = 3;
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Commit()).Times(kNumberOfCommits);
  });

  ::testing::InSequence s;

  // Commit the first buffer and expect the OnSubmission immediately.
  EXPECT_CALL(
      mock_surface_gpu,
      OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK,
                   Truly([](const auto& fence) { return fence.is_null(); })))
      .Times(1);
  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  // Let mojo messages from gpu to host to go through.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  SendFrameCallbackForSurface(surface_id_);

  // Commit the second buffer now.
  CommitBuffer(widget, kBufferId2, kBufferId2,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  SendFrameCallbackForSurface(surface_id_);

  EXPECT_CALL(
      mock_surface_gpu,
      OnSubmission(kBufferId2, gfx::SwapResult::SWAP_ACK,
                   Truly([](const auto& fence) { return !fence.is_null(); })))
      .Times(1);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    // Release the first buffer via fenced release. This should trigger
    // OnSubmission for the second buffer with a non-null fence.
    gfx::GpuFenceHandle handle;
    const int32_t kFenceFD = dup(1);
    handle.Adopt(base::ScopedFD(kFenceFD));

    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    mock_surface->ReleaseBufferFenced(mock_surface->prev_attached_buffer(),
                                      std::move(handle));
    mock_surface->SendFrameCallback();
  });

  // Let mojo messages from gpu to host to go through.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  // Commit the third buffer now.
  CommitBuffer(widget, kBufferId3, kBufferId3,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  SendFrameCallbackForSurface(surface_id_);

  // Release the second buffer via immediate explicit release. This should
  // trigger OnSubmission for the second buffer with a null fence.
  EXPECT_CALL(
      mock_surface_gpu,
      OnSubmission(kBufferId3, gfx::SwapResult::SWAP_ACK,
                   Truly([](const auto& fence) { return fence.is_null(); })))
      .Times(1);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    mock_surface->ReleaseBufferFenced(mock_surface->prev_attached_buffer(),
                                      gfx::GpuFenceHandle());
    mock_surface->SendFrameCallback();
  });

  // Let mojo messages from gpu to host to go through.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  DestroyBufferAndSetTerminateExpectation(kBufferId1, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(kBufferId2, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(kBufferId3, false /*fail*/);
}

// Tests that destroying a channel doesn't result in resetting surface state
// and buffers can be attached after the channel has been reinitialized.
TEST_P(WaylandBufferManagerTest,
       CanSubmitBufferAfterChannelDestroyedAndInitialized) {
  constexpr uint32_t kBufferId1 = 1;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = window_->GetBoundsInPixels();

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);

    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
    EXPECT_CALL(*mock_surface, Commit()).Times(1);

    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });

  auto mock_surface_gpu =
      std::make_unique<MockSurfaceGpu>(buffer_manager_gpu_.get(), widget);

  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);

  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  EXPECT_CALL(*mock_surface_gpu.get(),
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(*mock_surface_gpu.get(), OnPresentation(kBufferId1, _)).Times(1);

  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  // Let mojo messages from host to gpu to go through.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    // The root surface shouldn't get null buffer attached.
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
    EXPECT_CALL(*mock_surface, Commit()).Times(1);

    mock_surface->SendFrameCallback();
  });

  // After the channel has been destroyed and surface state has been reset, the
  // interface should bind again and it still should be possible to attach
  // buffers as WaylandBufferManagerHost::Surface::ResetSurfaceContents mustn't
  // reset the state of |configured|.
  manager_host_->OnChannelDestroyed();
  manager_host_ = connection_->buffer_manager_host();

  // Let mojo messages from host to gpu go through.
  base::RunLoop().RunUntilIdle();

  // The surface must has the buffer detached and all the buffers are destroyed.
  // Release the fence as there is no further need to hold that as the client
  // no longer expects that. Moreover, its next attach may result in a DCHECK,
  // as the next buffer resource can be allocated on the same memory address
  // resulting in a DCHECK when set_linux_buffer_release is called. The reason
  // is that wl_resource_create calls internally calls malloc, which may reuse
  // that memory.
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    server->GetObject<wl::MockSurface>(id)->ClearBufferReleases();
  });

  auto interface_ptr = manager_host_->BindInterface();
  buffer_manager_gpu_->Initialize(std::move(interface_ptr), {},
                                  /*supports_dma_buf=*/false,
                                  /*supports_viewporter=*/true,
                                  /*supports_acquire_fence=*/false,
                                  /*supports_overlays=*/true,
                                  kAugmentedSurfaceNotSupportedVersion,
                                  /*supports_single_pixel_buffer=*/true,
                                  /*server_version=*/{});

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });

  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    // The buffer must be attached.
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(1);
    EXPECT_CALL(*mock_surface, Commit()).Times(1);
  });

  EXPECT_CALL(*mock_surface_gpu.get(),
              OnSubmission(kBufferId1, gfx::SwapResult::SWAP_ACK, _))
      .Times(1);
  EXPECT_CALL(*mock_surface_gpu.get(), OnPresentation(kBufferId1, _)).Times(1);

  CommitBuffer(widget, kBufferId1, kBufferId1,
               gfx::FrameData(delegate_.viz_seq()), bounds,
               gfx::RoundedCornersF(), kDefaultScale, bounds);

  // Let mojo messages from host to gpu to go through.
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);

  DestroyBufferAndSetTerminateExpectation(kBufferId1, false /*fail*/);
}

// Tests that destroying a channel results in attaching null buffers to the root
// surface, and hiding primary subsurface and overlay surfaces. This is required
// to make it possible for a GPU service to switch from hw acceleration to sw
// compositing. Otherwise, there will be frozen graphics represented by a
// primary subsurface as sw compositing uses the root surface to draw new
// frames. Verifies the fix for https://crbug.com/1201314
TEST_P(WaylandBufferManagerTest, HidesSubsurfacesOnChannelDestroyed) {
  if (!connection_->ShouldUseOverlayDelegation()) {
    GTEST_SKIP();
  }

  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;
  constexpr uint32_t kBufferId3 = 3;

  const gfx::Rect bounds = window_->GetBoundsInPixels();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(3);
  });

  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId3);
  ProcessCreatedBufferResourcesWithExpectation(3u /* expected size */,
                                               false /* fail */);

  // Prepare a frame with one background buffer, one primary plane and one
  // additional overlay plane. This will simulate hw accelerated compositing.
  std::vector<wl::WaylandOverlayConfig> overlay_configs;
  overlay_configs.emplace_back(
      CreateBasicWaylandOverlayConfig(INT32_MIN, kBufferId1, bounds));
  overlay_configs.emplace_back(
      CreateBasicWaylandOverlayConfig(0, kBufferId2, bounds));
  overlay_configs.emplace_back(
      CreateBasicWaylandOverlayConfig(1, kBufferId3, bounds));
  buffer_manager_gpu_->CommitOverlays(window_->GetWidget(), 1u,
                                      gfx::FrameData(delegate_.viz_seq()),
                                      std::move(overlay_configs));

  // Let mojo messages from gpu to host to go through.
  base::RunLoop().RunUntilIdle();

  // 3 surfaces must exist - root surface, the primary subsurface and one
  // additional overlay surface. All of them must have buffers attached.
  EXPECT_EQ(1u, window_->wayland_subsurfaces().size());
  PostToServerAndWait(
      [id = surface_id_,
       primary_subsurface_id =
           window_->primary_subsurface()->wayland_surface()->get_surface_id(),
       overlay_surface_id =
           window_->wayland_subsurfaces()
               .begin()
               ->get()
               ->wayland_surface()
               ->get_surface_id()](wl::TestWaylandServerThread* server) {
        EXPECT_TRUE(server->GetObject<wl::MockSurface>(id)->attached_buffer());
        EXPECT_TRUE(server->GetObject<wl::MockSurface>(primary_subsurface_id)
                        ->attached_buffer());
        EXPECT_TRUE(server->GetObject<wl::MockSurface>(overlay_surface_id)
                        ->attached_buffer());
      });

  // Pretend that the channel gets destroyed because of some internal reason.
  manager_host_->OnChannelDestroyed();
  manager_host_ = connection_->buffer_manager_host();

  // Let mojo messages from host to gpu go through.
  base::RunLoop().RunUntilIdle();

  // The root surface should still have the buffer attached....
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    EXPECT_TRUE(server->GetObject<wl::MockSurface>(id)->attached_buffer());
  });
  // ... and the primary and secondary subsurfaces must be hidden.
  EXPECT_FALSE(window_->primary_subsurface()->IsVisible());
  EXPECT_EQ(1u, window_->wayland_subsurfaces().size());
  EXPECT_FALSE(window_->wayland_subsurfaces().begin()->get()->IsVisible());

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* root_surface = server->GetObject<wl::MockSurface>(id);
    root_surface->ClearBufferReleases();
    root_surface->SendFrameCallback();
  });

  auto interface_ptr = manager_host_->BindInterface();
  buffer_manager_gpu_->Initialize(std::move(interface_ptr), {},
                                  /*supports_dma_buf=*/false,
                                  /*supports_viewporter=*/true,
                                  /*supports_acquire_fence=*/false,
                                  /*supports_overlays=*/true,
                                  kAugmentedSurfaceNotSupportedVersion,
                                  /*supports_single_pixel_buffer=*/true,
                                  /*server_version=*/{});

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    // Now, create only one buffer and attach that to the root surface. The
    // primary subsurface and secondary subsurface must remain invisible.
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(1);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  ProcessCreatedBufferResourcesWithExpectation(1u /* expected size */,
                                               false /* fail */);

  std::vector<wl::WaylandOverlayConfig> overlay_configs2;
  overlay_configs2.push_back(
      CreateBasicWaylandOverlayConfig(INT32_MIN, kBufferId1, bounds));
  buffer_manager_gpu_->CommitOverlays(window_->GetWidget(), 2u,
                                      gfx::FrameData(delegate_.viz_seq()),
                                      std::move(overlay_configs2));

  base::RunLoop().RunUntilIdle();

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    // The root surface should have the buffer detached.
    EXPECT_TRUE(server->GetObject<wl::MockSurface>(id)->attached_buffer());
  });

  // The primary and secondary subsurfaces must remain hidden.
  EXPECT_FALSE(window_->primary_subsurface()->IsVisible());
  EXPECT_EQ(1u, window_->wayland_subsurfaces().size());
  EXPECT_FALSE(window_->wayland_subsurfaces().begin()->get()->IsVisible());
}

TEST_P(WaylandBufferManagerTest,
       DoesNotAttachAndCommitOnHideIfNoBuffersAttached) {
  EXPECT_TRUE(window_->IsVisible());

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);

    constexpr uint32_t kNumberOfCommits = 0;
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Commit()).Times(kNumberOfCommits);
  });

  window_->Hide();
}

TEST_P(WaylandBufferManagerTest, HasOverlayPrioritizer) {
  EXPECT_TRUE(connection_->overlay_prioritizer());
}

TEST_P(WaylandBufferManagerTest, CanSubmitOverlayPriority) {
  if (!connection_->ShouldUseOverlayDelegation()) {
    GTEST_SKIP();
  }

  std::vector<uint32_t> kBufferIds = {1, 2, 3};

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(),
                                  window_->GetWidget());

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(3);
  });

  for (auto id : kBufferIds)
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, id);

  PostToServerAndWait(
      [size = kBufferIds.size()](wl::TestWaylandServerThread* server) {
        auto* linux_dmabuf = server->zwp_linux_dmabuf_v1();
        for (size_t i = 0; i < size; i++) {
          zwp_linux_buffer_params_v1_send_created(
              linux_dmabuf->buffer_params()[i]->resource(),
              linux_dmabuf->buffer_params()[i]->buffer_resource());
        }
      });

  std::vector<std::pair<gfx::OverlayPriorityHint, uint32_t>> priorities = {
      {gfx::OverlayPriorityHint::kNone,
       OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_NONE},
      {gfx::OverlayPriorityHint::kRegular,
       OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_REGULAR},
      {gfx::OverlayPriorityHint::kVideo,
       OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_REGULAR},
      {gfx::OverlayPriorityHint::kLowLatencyCanvas,
       OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_PREFERRED_LOW_LATENCY_CANVAS},
      {gfx::OverlayPriorityHint::kHardwareProtection,
       OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_REQUIRED_HARDWARE_PROTECTION}};

  uint32_t frame_id = 0u;
  for (const auto& priority : priorities) {
    std::vector<wl::WaylandOverlayConfig> overlay_configs;
    for (auto id : kBufferIds) {
      overlay_configs.emplace_back(CreateBasicWaylandOverlayConfig(
          id == 1 ? INT32_MIN : id, id, window_->GetBoundsInPixels()));
      overlay_configs.back().priority_hint = priority.first;
    }

    buffer_manager_gpu_->CommitOverlays(window_->GetWidget(), ++frame_id,
                                        gfx::FrameData(delegate_.viz_seq()),
                                        std::move(overlay_configs));

    base::RunLoop().RunUntilIdle();

    for (auto& subsurface : window_->wayland_subsurfaces_) {
      PostToServerAndWait(
          [subsurface_id = subsurface->wayland_surface()->get_surface_id(),
           &priority](wl::TestWaylandServerThread* server) {
            auto* mock_surface_of_subsurface =
                server->GetObject<wl::MockSurface>(subsurface_id);
            EXPECT_TRUE(mock_surface_of_subsurface);
            EXPECT_EQ(mock_surface_of_subsurface->prioritized_surface()
                          ->overlay_priority(),
                      priority.second);

            mock_surface_of_subsurface->SendFrameCallback();
          });
    }

    SendFrameCallbackForSurface(surface_id_);
  }
}

TEST_P(WaylandBufferManagerTest, HasSurfaceAugmenter) {
  InitializeSurfaceAugmenter();
  EXPECT_TRUE(connection_->surface_augmenter());
}

TEST_P(WaylandBufferManagerTest, CanSetRoundedCorners) {
  if (!connection_->ShouldUseOverlayDelegation()) {
    GTEST_SKIP();
  }

  InitializeSurfaceAugmenter();

  std::vector<uint32_t> kBufferIds = {1, 2, 3};

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(),
                                  window_->GetWidget());

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(3);
  });

  for (auto id : kBufferIds)
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, id);

  PostToServerAndWait(
      [size = kBufferIds.size()](wl::TestWaylandServerThread* server) {
        auto* linux_dmabuf = server->zwp_linux_dmabuf_v1();
        for (size_t i = 0; i < size; i++) {
          zwp_linux_buffer_params_v1_send_created(
              linux_dmabuf->buffer_params()[i]->resource(),
              linux_dmabuf->buffer_params()[i]->buffer_resource());
        }
      });

  std::vector<gfx::RRectF> rounded_corners_vec = {
      {{10, 10, 200, 200}, {1, 1, 1, 1}},  {{10, 10, 200, 200}, {0, 1, 0, 1}},
      {{10, 10, 200, 200}, {1, 0, 1, 0}},  {{10, 10, 200, 200}, {5, 10, 0, 1}},
      {{10, 10, 200, 200}, {0, 2, 20, 3}}, {{10, 10, 200, 200}, {2, 3, 4, 5}},
      {{10, 10, 200, 200}, {0, 0, 0, 0}},
  };

  // Use different scale factors to verify Ozone/Wayland translates the corners
  // from px to dip.
  std::vector<float> scale_factors = {1, 1.2, 1.5, 2};

  // Exo may allow to submit values in px.
  std::vector<bool> in_pixels = {true, false};

  uint32_t frame_id = 0u;
  for (auto scale_factor : scale_factors) {
    if (scale_factor != std::ceil(scale_factor) &&
        !GetParam().surface_submission_in_pixel_coordinates) {
      // Fractional scales not supported when surface submission in pixel
      // coordinates is disabled.
      continue;
    }
    for (const auto& rounded_corners : rounded_corners_vec) {
      std::vector<wl::WaylandOverlayConfig> overlay_configs;
      for (auto id : kBufferIds) {
        overlay_configs.emplace_back(CreateBasicWaylandOverlayConfig(
            id == 1 ? INT32_MIN : id, id, window_->GetBoundsInPixels()));
        overlay_configs.back().surface_scale_factor = scale_factor;
        overlay_configs.back().rounded_clip_bounds = rounded_corners;
      }

      buffer_manager_gpu_->CommitOverlays(window_->GetWidget(), ++frame_id,
                                          gfx::FrameData(),
                                          std::move(overlay_configs));

      base::RunLoop().RunUntilIdle();

      for (auto& subsurface : window_->wayland_subsurfaces_) {
        gfx::RRectF rounded_clip_bounds_dip = rounded_corners;
        // If submission in px is allowed, there is no need to convert px to
        // dip.
        if (!GetParam().surface_submission_in_pixel_coordinates) {
          // Ozone/Wayland applies ceiled scale factor if it's fractional.
          rounded_clip_bounds_dip.Scale(1.f / std::ceil(scale_factor));
        }
        PostToServerAndWait(
            [subsurface_id = subsurface->wayland_surface()->get_surface_id(),
             &rounded_clip_bounds_dip](wl::TestWaylandServerThread* server) {
              auto* mock_surface_of_subsurface =
                  server->GetObject<wl::MockSurface>(subsurface_id);
              EXPECT_TRUE(mock_surface_of_subsurface);

              EXPECT_EQ(mock_surface_of_subsurface->augmented_surface()
                            ->rounded_clip_bounds(),
                        rounded_clip_bounds_dip);
              mock_surface_of_subsurface->SendFrameCallback();
            });
      }

      SendFrameCallbackForSurface(surface_id_);
    }
  }
}

// Verifies that there are no more than certain number of submitted frames that
// wait presentation feedbacks. If the number of pending frames hit the
// threshold, the feedbacks are marked as failed and discarded. See the comments
// below in the test.
TEST_P(WaylandBufferManagerTest, FeedbacksAreDiscardedIfClientMisbehaves) {
  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    ASSERT_TRUE(server->EnsureAndGetWpPresentation());
  });

  // 2 buffers are enough.
  constexpr uint32_t kBufferId1 = 1;
  constexpr uint32_t kBufferId2 = 2;

  const gfx::AcceleratedWidget widget = window_->GetWidget();
  const gfx::Rect bounds = gfx::Rect({0, 0}, kDefaultSize);
  window_->SetBoundsInDIP(bounds);

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget_);

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    EXPECT_CALL(*server->zwp_linux_dmabuf_v1(), CreateParams(_, _, _)).Times(2);
  });
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId1);
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/, kBufferId2);
  ProcessCreatedBufferResourcesWithExpectation(2u /* expected size */,
                                               false /* fail */);

  // There will be 235 frames/commits.
  constexpr uint32_t kNumberOfCommits = 235u;
  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(kNumberOfCommits);
    EXPECT_CALL(*server->EnsureAndGetWpPresentation(), Feedback(_, _, _, _))
        .Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Commit()).Times(kNumberOfCommits);
  });

  // The presentation feedbacks should fail after first 20 commits (that's the
  // threshold that WaylandFrameManager maintains). Next, the presentation
  // feedbacks will fail every consequent 17 commits as 3 frames out of 20
  // previous frames that WaylandFrameManager stores are always preserved in
  // case if the client restores the behavior (which is very unlikely).
  uint32_t expect_presentation_failure_on_commit_seq = 20u;
  // Chooses the next buffer id that should be committed.
  uint32_t next_buffer_id_commit = 0;
  // Specifies the expected number of failing feedbacks if the client
  // misbehaves.
  constexpr uint32_t kExpectedFailedFeedbacks = 17u;
  for (auto commit_seq = 1u; commit_seq <= kNumberOfCommits; commit_seq++) {
    // All the other expectations must come in order.
    if (next_buffer_id_commit == kBufferId1)
      next_buffer_id_commit = kBufferId2;
    else
      next_buffer_id_commit = kBufferId1;

    EXPECT_CALL(mock_surface_gpu, OnSubmission(next_buffer_id_commit,
                                               gfx::SwapResult::SWAP_ACK, _))
        .Times(1);

    if (commit_seq % expect_presentation_failure_on_commit_seq == 0) {
      EXPECT_CALL(mock_surface_gpu,
                  OnPresentation(_, gfx::PresentationFeedback::Failure()))
          .Times(kExpectedFailedFeedbacks);
      // See comment near |expect_presentation_failure_on_commit_seq|.
      expect_presentation_failure_on_commit_seq += kExpectedFailedFeedbacks;
    } else {
      // The client misbehaves and doesn't send presentation feedbacks. The
      // frame manager doesn't mark the feedbacks as failed until the threshold
      // is hit.
      EXPECT_CALL(mock_surface_gpu, OnPresentation(_, _)).Times(0);
    }

    CommitBuffer(widget, next_buffer_id_commit, next_buffer_id_commit,
                 gfx::FrameData(delegate_.viz_seq()), bounds,
                 gfx::RoundedCornersF(), kDefaultScale, bounds);

    PostToServerAndWait(
        [id = surface_id_](wl::TestWaylandServerThread* server) {
          auto* mock_surface = server->GetObject<wl::MockSurface>(id);

          if (auto* buffer = mock_surface->prev_attached_buffer())
            mock_surface->ReleaseBuffer(buffer);

          server->EnsureAndGetWpPresentation()->DropPresentationCallback(
              /*last=*/true);

          mock_surface->SendFrameCallback();
        });

    base::RunLoop().RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&mock_surface_gpu);
  }

  DestroyBufferAndSetTerminateExpectation(kBufferId1, false /*fail*/);
  DestroyBufferAndSetTerminateExpectation(kBufferId2, false /*fail*/);
}

TEST_P(WaylandBufferManagerTest, ExecutesTasksAfterInitialization) {
  // Unbind the pipe.
  manager_host_->OnChannelDestroyed();

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(buffer_manager_gpu_->remote_host_);
  EXPECT_TRUE(buffer_manager_gpu_->pending_tasks_.empty());

  constexpr uint32_t kDmabufBufferId = 1;
  CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                    kDmabufBufferId);
  CommitBuffer(window_->GetWidget(), kDmabufBufferId, kDmabufBufferId,
               gfx::FrameData(delegate_.viz_seq()),
               window_->GetBoundsInPixels(), gfx::RoundedCornersF(),
               kDefaultScale, window_->GetBoundsInPixels());
  DestroyBufferAndSetTerminateExpectation(kDmabufBufferId, false /*fail*/);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3u, buffer_manager_gpu_->pending_tasks_.size());

  auto interface_ptr = manager_host_->BindInterface();
  buffer_manager_gpu_->Initialize(std::move(interface_ptr), {},
                                  /*supports_dma_buf=*/false,
                                  /*supports_viewporter=*/true,
                                  /*supports_acquire_fence=*/false,
                                  /*supports_overlays=*/true,
                                  kAugmentedSurfaceNotSupportedVersion,
                                  /*supports_single_pixel_buffer=*/true,
                                  /*server_version=*/{});

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(buffer_manager_gpu_->pending_tasks_.empty());
}

TEST_P(WaylandBufferManagerTest, DoesNotRequestReleaseForSolidColorBuffers) {
  if (!connection_->linux_explicit_synchronization_v1())
    GTEST_SKIP();

  PostToServerAndWait([](wl::TestWaylandServerThread* server) {
    server->EnsureSurfaceAugmenter();
  });

  MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(), widget_);

  const auto solid_color_buffer_id = buffer_manager_gpu_->AllocateBufferID();
  buffer_manager_gpu_->CreateSolidColorBuffer(
      SkColor4f::FromColor(SK_ColorBLUE), gfx::Size(1, 1),
      solid_color_buffer_id);

  base::RunLoop().RunUntilIdle();

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    constexpr uint32_t kNumberOfCommits = 1;
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(kNumberOfCommits);
    EXPECT_CALL(*mock_surface, Commit()).Times(kNumberOfCommits);
  });

  std::vector<wl::WaylandOverlayConfig> overlay_configs;
  auto bounds = window_->GetBoundsInPixels();
  overlay_configs.emplace_back(CreateBasicWaylandOverlayConfig(
      INT32_MIN, solid_color_buffer_id, bounds));
  buffer_manager_gpu_->CommitOverlays(widget_, 1u,
                                      gfx::FrameData(delegate_.viz_seq()),
                                      std::move(overlay_configs));

  base::RunLoop().RunUntilIdle();

  PostToServerAndWait([id = surface_id_](wl::TestWaylandServerThread* server) {
    EXPECT_FALSE(
        server->GetObject<wl::MockSurface>(id)->has_linux_buffer_release());
  });
}

class WaylandBufferManagerViewportTest : public WaylandBufferManagerTest {
 public:
  WaylandBufferManagerViewportTest() = default;
  ~WaylandBufferManagerViewportTest() override = default;

 protected:
  void ViewportDestinationTestHelper(const gfx::RectF& bounds_rect,
                                     const gfx::RectF& expected_bounds_rect) {
    auto temp_window = CreateWindow();
    temp_window->Show(false);
    const uint32_t surface_id = temp_window->root_surface()->get_surface_id();

    PostToServerAndWait([surface_id](wl::TestWaylandServerThread* server) {
      ASSERT_TRUE(server->GetObject<wl::MockSurface>(surface_id));
    });

    ActivateSurface(surface_id);

    constexpr uint32_t kBufferId1 = 1;
    constexpr uint32_t kBufferId2 = 2;

    MockSurfaceGpu mock_surface_gpu(buffer_manager_gpu_.get(),
                                    temp_window->GetWidget());

    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId1);
    CreateDmabufBasedBufferAndSetTerminateExpectation(false /*fail*/,
                                                      kBufferId2);
    ProcessCreatedBufferResourcesWithExpectation(2u /* expected size */,
                                                 false /* fail */);

    std::vector<wl::WaylandOverlayConfig> overlay_configs;
    auto bounds = temp_window->GetBoundsInPixels();
    overlay_configs.emplace_back(
        CreateBasicWaylandOverlayConfig(INT32_MIN, kBufferId1, bounds));
    overlay_configs.emplace_back(
        CreateBasicWaylandOverlayConfig(0, kBufferId1, bounds));
    overlay_configs.emplace_back(
        CreateBasicWaylandOverlayConfig(1, kBufferId1, bounds_rect));
    buffer_manager_gpu_->CommitOverlays(temp_window->GetWidget(), 1u,
                                        gfx::FrameData(delegate_.viz_seq()),
                                        std::move(overlay_configs));

    base::RunLoop().RunUntilIdle();

    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      // Creates a handle for a subsurface.
      const std::vector<raw_ptr<wl::TestZwpLinuxBufferParamsV1,
                                VectorExperimental>>& params_vector =
          server->zwp_linux_dmabuf_v1()->buffer_params();
      for (wl::TestZwpLinuxBufferParamsV1* mock_params : params_vector) {
        zwp_linux_buffer_params_v1_send_created(mock_params->resource(),
                                                mock_params->buffer_resource());
      }
    });

    EXPECT_EQ(temp_window->wayland_subsurfaces_.size(), 1u);
    WaylandSubsurface* subsurface =
        temp_window->wayland_subsurfaces_.begin()->get();
    ASSERT_TRUE(subsurface);
    const uint32_t subsurface_id =
        subsurface->wayland_surface()->get_surface_id();
    PostToServerAndWait([subsurface_id](wl::TestWaylandServerThread* server) {
      ASSERT_TRUE(server->GetObject<wl::MockSurface>(subsurface_id));
    });

    // The conversion from double to fixed and back is necessary because it
    // happens during the roundtrip, and it creates significant error.
    gfx::SizeF expected_size(wl_fixed_to_double(wl_fixed_from_double(
                                 expected_bounds_rect.size().width())),
                             wl_fixed_to_double(wl_fixed_from_double(
                                 expected_bounds_rect.size().height())));

    PostToServerAndWait([surface_id, subsurface_id,
                         &expected_size](wl::TestWaylandServerThread* server) {
      auto* mock_surface_of_subsurface =
          server->GetObject<wl::MockSurface>(subsurface_id);
      auto* test_vp = mock_surface_of_subsurface->viewport();
      EXPECT_EQ(expected_size, test_vp->destination_size());

      mock_surface_of_subsurface->SendFrameCallback();
      server->GetObject<wl::MockSurface>(surface_id)->SendFrameCallback();
    });

    DestroyBufferAndSetTerminateExpectation(kBufferId1, false);
    DestroyBufferAndSetTerminateExpectation(kBufferId2, false);
  }
};

// Tests viewport destination is set correctly when the augmenter subsurface
// protocol is not available and then becomes available.
TEST_P(WaylandBufferManagerViewportTest, ViewportDestinationNonInteger) {
  if (!connection_->ShouldUseOverlayDelegation()) {
    GTEST_SKIP();
  }
  constexpr std::array<std::array<gfx::RectF, 2>, 2> test_data = {
      {{gfx::RectF({21, 18}, {7, 11}), gfx::RectF({21, 18}, {7, 11})},
       {gfx::RectF({7, 8}, {43, 63}), gfx::RectF({7, 8}, {43, 63})}}};

  for (const auto& data : test_data) {
    ViewportDestinationTestHelper(data[0] /* display_rect */,
                                  data[1] /* expected_rect */);

    // Initialize the surface augmenter now.
    InitializeSurfaceAugmenter();
    ASSERT_TRUE(connection_->surface_augmenter());
  }
}

// Tests viewport destination is set correctly when the augmenter subsurface
// protocol is not available (the destination is rounded), and the protocol is
// available (the destination is set with floating point precision).
TEST_P(WaylandBufferManagerViewportTest, ViewportDestinationInteger) {
  if (!connection_->ShouldUseOverlayDelegation()) {
    GTEST_SKIP();
  }

  constexpr std::array<std::array<gfx::RectF, 2>, 2> test_data = {
      {{gfx::RectF({21, 18}, {7.423, 11.854}), gfx::RectF({21, 18}, {7, 12})},
       {gfx::RectF({7, 8}, {43.562, 63.76}),
        gfx::RectF({7, 8}, {43.562, 63.76})}}};

  for (const auto& data : test_data) {
    ViewportDestinationTestHelper(data[0] /* display_rect */,
                                  data[1] /* expected_rect */);

    // Initialize the surface augmenter now.
    InitializeSurfaceAugmenter();
    ASSERT_TRUE(connection_->surface_augmenter());
  }
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandBufferManagerTest,
                         Values(wl::ServerConfig{}));

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandBufferManagerViewportTest,
                         Values(wl::ServerConfig{}));

INSTANTIATE_TEST_SUITE_P(
    XdgVersionStableTestWithoutExplicitSync,
    WaylandBufferManagerTest,
    Values(wl::ServerConfig{
        .use_explicit_synchronization =
            wl::ShouldUseExplicitSynchronizationProtocol::kNone}));
INSTANTIATE_TEST_SUITE_P(
    XdgVersionStableTestWithExplicitSync,
    WaylandBufferManagerTest,
    Values(wl::ServerConfig{
        .use_explicit_synchronization =
            wl::ShouldUseExplicitSynchronizationProtocol::kUse}));

INSTANTIATE_TEST_SUITE_P(
    XdgVersionStableTestWithSurfaceSubmissionInPixelCoordinatesDisabled,
    WaylandBufferManagerTest,
    Values(wl::ServerConfig{.surface_submission_in_pixel_coordinates = false}));
INSTANTIATE_TEST_SUITE_P(
    XdgVersionStableTestWithViewporterSurfaceScalingEnabled,
    WaylandBufferManagerTest,
    Values(wl::ServerConfig{.surface_submission_in_pixel_coordinates = false,
                            .supports_viewporter_surface_scaling = true}));

}  // namespace ui
