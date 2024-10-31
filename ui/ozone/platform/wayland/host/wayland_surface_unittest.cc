// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_surface.h"

#include <drm_fourcc.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include "base/posix/eintr_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_handle.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"
#include "ui/ozone/platform/wayland/host/wayland_syncobj_timeline.h"
#include "ui/ozone/platform/wayland/test/mock_drm_syncobj_ioctl_wrapper.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/mock_zwp_linux_dmabuf.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/test_zwp_linux_buffer_params.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

namespace ui {
namespace {

using ::testing::ElementsAre;

using testing::_;
using testing::Mock;

constexpr gfx::Size kDefaultSize(1024, 768);

MATCHER_P(IsSameFile, fd2, "") {
  struct stat stat1, stat2;
  if (fstat(arg, &stat1) == -1 || fstat(fd2, &stat2) == -1) {
    return false;
  }
  return (stat1.st_ino == stat2.st_ino) && (stat1.st_dev == stat2.st_dev);
}

using WaylandSurfaceTest = WaylandTest;

TEST_P(WaylandSurfaceTest, SurfaceReenterOutput) {
  WaylandSurface* wayland_surface = window_->root_surface();
  EXPECT_TRUE(wayland_surface->entered_outputs().empty());

  // Client side WaylandOutput id.
  const uint32_t output_id =
      screen_->GetOutputIdForDisplayId(screen_->GetPrimaryDisplay().id());

  const uint32_t surface_id_ = window_->root_surface()->get_surface_id();

  PostToServerAndWait([surface_id_](wl::TestWaylandServerThread* server) {
    wl_surface_send_enter(
        server->GetObject<wl::MockSurface>(surface_id_)->resource(),
        server->output()->resource());
  });
  EXPECT_THAT(wayland_surface->entered_outputs(), ElementsAre(output_id));

  // Send enter again, but entered outputs should not have duplicate values.
  PostToServerAndWait([surface_id_](wl::TestWaylandServerThread* server) {
    wl_surface_send_enter(
        server->GetObject<wl::MockSurface>(surface_id_)->resource(),
        server->output()->resource());
  });
  EXPECT_THAT(wayland_surface->entered_outputs(), ElementsAre(output_id));
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandSurfaceTest,
                         ::testing::Values(wl::ServerConfig{}));
#else
INSTANTIATE_TEST_SUITE_P(
    XdgVersionStableTestWithAuraShell,
    WaylandSurfaceTest,
    ::testing::Values(
        wl::ServerConfig{
            .enable_aura_shell = wl::EnableAuraShellProtocol::kEnabled},
        wl::ServerConfig{
            .enable_aura_shell = wl::EnableAuraShellProtocol::kEnabled}));
#endif

}  // namespace

class WaylandSurfaceExplicitSyncTest : public WaylandTestSimple {
 public:
  WaylandSurfaceExplicitSyncTest()
      : WaylandTestSimple(
            wl::ServerConfig{.use_linux_drm_syncobj =
                                 wl::ShouldUseLinuxDrmSyncobjProtocol::kUse}) {}

  void SetUp() override {
    WaylandTestSimple::SetUp();
    surface_id_ = window_->root_surface()->get_surface_id();
    auto drm = std::make_unique<MockDrmSyncobjIoctlWrapper>();
    drm_ = drm.get();
    connection_->buffer_manager_host()->SetDrmSyncobjWrapper(std::move(drm));
    EXPECT_TRUE(connection_->buffer_manager_host());
    auto interface_ptr = connection_->buffer_manager_host()->BindInterface();
    buffer_manager_gpu_->Initialize(std::move(interface_ptr), {},
                                    /*supports_dma_buf=*/false,
                                    /*supports_viewporter=*/true,
                                    /*supports_acquire_fence=*/false,
                                    /*supports_overlays=*/true, 0,
                                    /*supports_single_pixel_buffer=*/true,
                                    /*server_version=*/{});
  }

  void CreateDmabufBasedBuffer(uint32_t buffer_id,
                               const gfx::Size& size = kDefaultSize,
                               const std::vector<uint32_t>& strides = {1},
                               const std::vector<uint32_t>& offsets = {2},
                               const std::vector<uint64_t>& modifiers = {3},
                               uint32_t format = DRM_FORMAT_R8,
                               uint32_t planes_count = 1) {
    buffer_manager_gpu_->CreateDmabufBasedBuffer(
        GetFdFactory()->CreateFd(), kDefaultSize, strides, offsets, modifiers,
        format, planes_count, buffer_id);

    base::RunLoop().RunUntilIdle();

    PostToServerAndWait([](wl::TestWaylandServerThread* server) {
      auto params_vector = server->zwp_linux_dmabuf_v1()->buffer_params();
      // To ensure, no other buffers are created, test the size of the
      // vector.
      EXPECT_EQ(params_vector.size(), static_cast<unsigned>(1));

      for (wl::TestZwpLinuxBufferParamsV1* mock_params : params_vector) {
        zwp_linux_buffer_params_v1_send_created(mock_params->resource(),
                                                mock_params->buffer_resource());
      }
    });
  }

 protected:
  gfx::GpuFenceHandle CreateFence() {
    auto fd = fd_factory_->CreateFd();
    gfx::GpuFenceHandle fence;
    fence.Adopt(std::move(fd));
    return fence;
  }

  wl::TestFdFactory* GetFdFactory() {
    if (!fd_factory_) {
      fd_factory_ = std::make_unique<wl::TestFdFactory>();
    }
    return fd_factory_.get();
  }

  void VerifyAndClearExpectations() {
    Mock::VerifyAndClearExpectations(drm_);
    PostToServerAndWait([this](wl::TestWaylandServerThread* server) {
      Mock::VerifyAndClearExpectations(
          server->GetObject<wl::MockSurface>(surface_id_)
              ->linux_drm_syncobj_surface());
    });
  }

  uint32_t surface_id_;
  raw_ptr<MockDrmSyncobjIoctlWrapper> drm_ = nullptr;

 private:
  std::unique_ptr<wl::TestFdFactory> fd_factory_;
};

TEST_F(WaylandSurfaceExplicitSyncTest, ConfigureWithExplicitSync) {
  constexpr uint32_t kBufferId = 1;
  const auto& syncobjs = drm_->syncobjs();
  constexpr uint32_t kBufferReleaseSyncObj = 1;

  // Create a buffer.
  CreateDmabufBasedBuffer(kBufferId);

  // Creating the buffer should create the first syncobj for the release
  // timeline.
  EXPECT_EQ(drm_->syncobjs().size(), static_cast<unsigned>(1));
  auto it = syncobjs.find(kBufferReleaseSyncObj);
  EXPECT_NE(it, syncobjs.end());
  int buffer_release_syncobj = it->second.get();

  auto* surface = window_->root_surface();
  EXPECT_TRUE(surface->AttachBuffer(
      connection_->buffer_manager_host()->EnsureBufferHandle(surface,
                                                             kBufferId)));
  surface->set_acquire_fence(CreateFence());

  surface->EnsureAcquireTimeline();

  // The second syncobj is created for the surface acquire timeline.
  EXPECT_EQ(drm_->syncobjs().size(), static_cast<unsigned>(2));
  constexpr uint32_t kSurfaceAcquireSyncObj = 2;
  it = syncobjs.find(kSurfaceAcquireSyncObj);
  EXPECT_NE(it, syncobjs.end());
  int surface_acquire_syncobj = it->second.get();

  surface->EnsureSurfaceSync();

  PostToServerAndWait([this, surface_acquire_syncobj, buffer_release_syncobj](
                          wl::TestWaylandServerThread* server) {
    auto* syncobj_surface = server->GetObject<wl::MockSurface>(surface_id_)
                                ->linux_drm_syncobj_surface();
    EXPECT_TRUE(syncobj_surface);
    EXPECT_CALL(*syncobj_surface,
                SetAcquirePoint(IsSameFile(surface_acquire_syncobj), 1));
    EXPECT_CALL(*syncobj_surface,
                SetReleasePoint(IsSameFile(buffer_release_syncobj), 1));
  });
  EXPECT_TRUE(surface->ApplyPendingState().value());
  VerifyAndClearExpectations();

  // There should still only be 2 syncobjs.
  EXPECT_EQ(drm_->syncobjs().size(), static_cast<unsigned>(2));
}

TEST_F(WaylandSurfaceExplicitSyncTest, ConfigureWithExplicitSyncAndCommit) {
  constexpr int kCurrentSyncPoint = 1;
  constexpr uint32_t kBufferId = 1;

  // Create a buffer.
  CreateDmabufBasedBuffer(kBufferId);

  auto* surface = window_->root_surface();
  EXPECT_TRUE(surface->AttachBuffer(
      connection_->buffer_manager_host()->EnsureBufferHandle(surface,
                                                             kBufferId)));
  surface->set_acquire_fence(CreateFence());

  base::RunLoop run_loop;
  wl_buffer* buffer = nullptr;
  base::ScopedFD fd;

  auto callback = base::BindOnce(
      [](base::RepeatingClosure quit_closure, wl_buffer** buffer_out,
         base::ScopedFD* fd_out, wl_buffer* buffer, base::ScopedFD fd) {
        *buffer_out = buffer;
        *fd_out = std::move(fd);
        quit_closure.Run();
      },
      run_loop.QuitClosure(), &buffer, &fd);

  surface->RequestExplicitRelease(std::move(callback));

  // Explicit sync should be set when pending state is applied.
  EXPECT_TRUE(surface->ApplyPendingState().value());
  VerifyAndClearExpectations();

  // When eventfd is set notify fence is available immediately by writing to it.
  EXPECT_CALL(*drm_, SyncobjEventfd(_, kCurrentSyncPoint, _, _))
      .WillOnce([](uint32_t, uint64_t, int ev_fd, uint32_t) {
        uint64_t value = 1;
        HANDLE_EINTR(write(ev_fd, &value, sizeof(value)));
        return 0;
      });

  // Ensure exporting current sync point works.
  base::ScopedFD sync_file_fd = GetFdFactory()->CreateFd();
  EXPECT_CALL(*drm_, SyncobjExportSyncFile(_, _))
      .WillOnce([fd = sync_file_fd.get()](uint32_t, int* sync_file_fd) {
        *sync_file_fd = HANDLE_EINTR(dup(fd));
        return 0;
      });

  surface->Commit();
  connection_->Flush();
  run_loop.Run();
  EXPECT_EQ(connection_->buffer_manager_host()
                ->GetBufferHandle(surface, kBufferId)
                ->buffer(),
            buffer);
  EXPECT_TRUE(fd.is_valid());
}

// Tests that if acquire timeline creation fails, explicit sync is not set and
// explicit release callback is cleared.
TEST_F(WaylandSurfaceExplicitSyncTest,
       ExplicitSyncNotSet_AcquireTimelineCreationFailed) {
  constexpr uint32_t kBufferId = 1;

  // Create a buffer.
  CreateDmabufBasedBuffer(kBufferId);

  auto* surface = window_->root_surface();
  EXPECT_TRUE(surface->AttachBuffer(
      connection_->buffer_manager_host()->EnsureBufferHandle(surface,
                                                             kBufferId)));
  surface->set_acquire_fence(CreateFence());

  surface->EnsureSurfaceSync();
  PostToServerAndWait([this](wl::TestWaylandServerThread* server) {
    auto* syncobj_surface = server->GetObject<wl::MockSurface>(surface_id_)
                                ->linux_drm_syncobj_surface();
    EXPECT_TRUE(syncobj_surface);
    EXPECT_CALL(*syncobj_surface, SetAcquirePoint(_, _)).Times(0);
    EXPECT_CALL(*syncobj_surface, SetReleasePoint(_, _)).Times(0);
  });

  surface->RequestExplicitRelease(
      base::BindOnce([](wl_buffer*, base::ScopedFD) {}));
  // No acquire timeline should be set at this point.
  EXPECT_FALSE(surface->acquire_timeline_);
  {
    // Ensure acquire timeline creation fails when applying pending state for
    // the first time.
    base::AutoReset<bool> fail_on_create(
        &MockDrmSyncobjIoctlWrapper::fail_on_syncobj_create, true);
    EXPECT_TRUE(surface->ApplyPendingState().value());
  }
  VerifyAndClearExpectations();
  EXPECT_TRUE(surface->next_explicit_release_request_.is_null());
}

// Tests that if release timeline creation fails, explicit sync is not set and
// explicit release callback is cleared.
TEST_F(WaylandSurfaceExplicitSyncTest,
       ExplicitSyncNotSet_ReleaseTimelineCreationFailed) {
  constexpr uint32_t kBufferId = 1;
  {
    // Ensure release timeline creation fails when creating the buffer.
    base::AutoReset<bool> fail_on_create(
        &MockDrmSyncobjIoctlWrapper::fail_on_syncobj_create, true);
    CreateDmabufBasedBuffer(kBufferId);
  }

  auto* surface = window_->root_surface();
  EXPECT_TRUE(surface->AttachBuffer(
      connection_->buffer_manager_host()->EnsureBufferHandle(surface,
                                                             kBufferId)));
  surface->set_acquire_fence(CreateFence());

  surface->EnsureSurfaceSync();
  PostToServerAndWait([this](wl::TestWaylandServerThread* server) {
    auto* syncobj_surface = server->GetObject<wl::MockSurface>(surface_id_)
                                ->linux_drm_syncobj_surface();
    EXPECT_TRUE(syncobj_surface);
    EXPECT_CALL(*syncobj_surface, SetAcquirePoint(_, _)).Times(0);
    EXPECT_CALL(*syncobj_surface, SetReleasePoint(_, _)).Times(0);
  });

  surface->RequestExplicitRelease(
      base::BindOnce([](wl_buffer*, base::ScopedFD) {}));
  EXPECT_TRUE(surface->ApplyPendingState().value());
  VerifyAndClearExpectations();
  EXPECT_TRUE(surface->next_explicit_release_request_.is_null());
}

// Tests that if initial acquire fence is not set, explicit sync is not set and
// explicit release callback is cleared.
TEST_F(WaylandSurfaceExplicitSyncTest,
       ExplicitSyncNotSet_InitialAcquireFenceNotSet) {
  constexpr uint32_t kBufferId = 1;
  CreateDmabufBasedBuffer(kBufferId);

  auto* surface = window_->root_surface();
  EXPECT_TRUE(surface->AttachBuffer(
      connection_->buffer_manager_host()->EnsureBufferHandle(surface,
                                                             kBufferId)));

  surface->EnsureSurfaceSync();
  PostToServerAndWait([this](wl::TestWaylandServerThread* server) {
    auto* syncobj_surface = server->GetObject<wl::MockSurface>(surface_id_)
                                ->linux_drm_syncobj_surface();
    EXPECT_TRUE(syncobj_surface);
    EXPECT_CALL(*syncobj_surface, SetAcquirePoint(_, _)).Times(0);
    EXPECT_CALL(*syncobj_surface, SetReleasePoint(_, _)).Times(0);
  });

  surface->RequestExplicitRelease(
      base::BindOnce([](wl_buffer*, base::ScopedFD) {}));
  EXPECT_TRUE(surface->ApplyPendingState().value());
  VerifyAndClearExpectations();
  EXPECT_TRUE(surface->next_explicit_release_request_.is_null());
}

// Tests that if initial acquire fence import fails, explicit sync is not set
// and explicit release callback is cleared.
TEST_F(WaylandSurfaceExplicitSyncTest,
       ExplicitNotSyncSet_InitialAcquireFenceImportFail) {
  constexpr uint32_t kBufferId = 1;
  CreateDmabufBasedBuffer(kBufferId);

  auto* surface = window_->root_surface();
  EXPECT_TRUE(surface->AttachBuffer(
      connection_->buffer_manager_host()->EnsureBufferHandle(surface,
                                                             kBufferId)));
  surface->EnsureAcquireTimeline();

  surface->set_acquire_fence(CreateFence());

  surface->EnsureSurfaceSync();

  PostToServerAndWait([this](wl::TestWaylandServerThread* server) {
    auto* syncobj_surface = server->GetObject<wl::MockSurface>(surface_id_)
                                ->linux_drm_syncobj_surface();
    EXPECT_TRUE(syncobj_surface);
    EXPECT_CALL(*syncobj_surface, SetAcquirePoint(_, _)).Times(0);
    EXPECT_CALL(*syncobj_surface, SetReleasePoint(_, _)).Times(0);
  });

  surface->RequestExplicitRelease(
      base::BindOnce([](wl_buffer*, base::ScopedFD) {}));

  EXPECT_CALL(*drm_, SyncobjImportSyncFile(_, _)).WillOnce([] {
    return ENOENT;
  });
  EXPECT_TRUE(surface->ApplyPendingState().value());
  VerifyAndClearExpectations();
  EXPECT_TRUE(surface->next_explicit_release_request_.is_null());
}

// Tests that previous acquire sync point is used if a fence is not passed in a
// subsequent frame.
TEST_F(WaylandSurfaceExplicitSyncTest,
       ExplicitSyncSet_SubsequentAcquireFenceNotSet) {
  constexpr uint32_t kBufferId = 1;
  CreateDmabufBasedBuffer(kBufferId);
  constexpr uint32_t kBufferReleaseSyncObj = 1;
  int buffer_release_syncobj = drm_->syncobjs().at(kBufferReleaseSyncObj).get();

  auto* surface = window_->root_surface();
  EXPECT_TRUE(surface->AttachBuffer(
      connection_->buffer_manager_host()->EnsureBufferHandle(surface,
                                                             kBufferId)));
  surface->EnsureAcquireTimeline();
  // Ensure initial acquire sync point is set to 1.
  surface->acquire_timeline_->IncrementSyncPoint();

  surface->EnsureSurfaceSync();
  constexpr uint32_t kSurfaceAcquireSyncObj = 2;
  int surface_acquire_syncobj =
      drm_->syncobjs().at(kSurfaceAcquireSyncObj).get();

  PostToServerAndWait([this, surface_acquire_syncobj, buffer_release_syncobj](
                          wl::TestWaylandServerThread* server) {
    auto* syncobj_surface = server->GetObject<wl::MockSurface>(surface_id_)
                                ->linux_drm_syncobj_surface();
    EXPECT_TRUE(syncobj_surface);
    EXPECT_CALL(*syncobj_surface,
                SetAcquirePoint(IsSameFile(surface_acquire_syncobj), 1));
    EXPECT_CALL(*syncobj_surface,
                SetReleasePoint(IsSameFile(buffer_release_syncobj), 1));
  });

  surface->RequestExplicitRelease(
      base::BindOnce([](wl_buffer*, base::ScopedFD) {}));
  EXPECT_TRUE(surface->ApplyPendingState().value());
  VerifyAndClearExpectations();
  EXPECT_FALSE(surface->next_explicit_release_request_.is_null());
}

// Tests that explicit sync is not set and ApplySurfaceConfigure fails if fence
// is passed but importing the fence fails.
TEST_F(WaylandSurfaceExplicitSyncTest,
       ExplicitSyncNotSet_SubsequentAcquireFenceImportFail) {
  constexpr uint32_t kBufferId = 1;
  CreateDmabufBasedBuffer(kBufferId);

  auto* surface = window_->root_surface();
  EXPECT_TRUE(surface->AttachBuffer(
      connection_->buffer_manager_host()->EnsureBufferHandle(surface,
                                                             kBufferId)));
  surface->EnsureAcquireTimeline();
  // Ensure initial acquire sync point is set to 1.
  surface->acquire_timeline_->IncrementSyncPoint();

  surface->set_acquire_fence(CreateFence());

  surface->EnsureSurfaceSync();

  PostToServerAndWait([this](wl::TestWaylandServerThread* server) {
    auto* syncobj_surface = server->GetObject<wl::MockSurface>(surface_id_)
                                ->linux_drm_syncobj_surface();
    EXPECT_TRUE(syncobj_surface);
    EXPECT_CALL(*syncobj_surface, SetAcquirePoint(_, _)).Times(0);
    EXPECT_CALL(*syncobj_surface, SetReleasePoint(_, _)).Times(0);
  });

  surface->RequestExplicitRelease(
      base::BindOnce([](wl_buffer*, base::ScopedFD) {}));

  EXPECT_CALL(*drm_, SyncobjImportSyncFile(_, _)).WillOnce([] {
    return ENOENT;
  });
  EXPECT_FALSE(surface->ApplyPendingState().has_value());
  VerifyAndClearExpectations();
  EXPECT_TRUE(surface->next_explicit_release_request_.is_null());
}

}  // namespace ui
