// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_syncobj_timeline.h"

#include <xf86drm.h>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/ozone/platform/wayland/test/mock_drm_syncobj_ioctl_wrapper.h"
#include "ui/ozone/platform/wayland/test/test_fd_factory.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using testing::_;
using testing::Mock;

namespace ui {

class WaylandSyncobjTimelineTest : public WaylandTestSimple {
 public:
  WaylandSyncobjTimelineTest()
      : WaylandTestSimple(
            wl::ServerConfig{.use_linux_drm_syncobj =
                                 wl::ShouldUseLinuxDrmSyncobjProtocol::kUse}) {}

  void SetUp() override {
    WaylandTestSimple::SetUp();
    auto drm = std::make_unique<MockDrmSyncobjIoctlWrapper>();
    drm_ = drm.get();
    connection_->buffer_manager_host()->SetDrmSyncobjWrapper(std::move(drm));
  }

  void TearDown() override {
    // The sync object map should get cleared by calling SyncobjDestroy upon
    // destruction of the syncobj timeline objects.
    EXPECT_TRUE(drm_->syncobjs().empty());
    WaylandTestSimple::TearDown();
  }

 protected:
  std::unique_ptr<WaylandSyncobjAcquireTimeline> CreateAcquireTimeline() {
    return WaylandSyncobjAcquireTimeline::Create(connection_.get());
  }

  std::unique_ptr<WaylandSyncobjReleaseTimeline> CreateReleaseTimeline() {
    return WaylandSyncobjReleaseTimeline::Create(connection_.get());
  }

  wl::TestFdFactory* GetFdFactory() {
    if (!fd_factory_) {
      fd_factory_ = std::make_unique<wl::TestFdFactory>();
    }
    return fd_factory_.get();
  }

  raw_ptr<MockDrmSyncobjIoctlWrapper> drm_ = nullptr;

 private:
  std::unique_ptr<wl::TestFdFactory> fd_factory_;
};

TEST_F(WaylandSyncobjTimelineTest, Create) {
  ASSERT_TRUE(CreateAcquireTimeline());
  ASSERT_TRUE(CreateReleaseTimeline());
}

TEST_F(WaylandSyncobjTimelineTest, Create_FailOnSyncobjCreate) {
  base::AutoReset<bool> fail_on_create(
      &MockDrmSyncobjIoctlWrapper::fail_on_syncobj_create, true);
  ASSERT_FALSE(CreateAcquireTimeline());
  ASSERT_FALSE(CreateReleaseTimeline());
}

TEST_F(WaylandSyncobjTimelineTest, Create_FailOnSyncobjHandleToFd) {
  base::AutoReset<bool> fail_on_convert_to_fd(
      &MockDrmSyncobjIoctlWrapper::fail_on_syncobj_handle_to_fd, true);
  ASSERT_FALSE(CreateAcquireTimeline());
  ASSERT_FALSE(CreateReleaseTimeline());
}

TEST_F(WaylandSyncobjTimelineTest, ImportSyncFdAtCurrentSyncPoint) {
  constexpr int kSyncFileFd = 123;
  constexpr int kCurrentSyncPoint = 4;
  auto timeline = CreateAcquireTimeline();
  EXPECT_EQ(timeline->sync_point(), static_cast<unsigned>(0));
  for (auto i = 0; i < kCurrentSyncPoint; i++) {
    timeline->IncrementSyncPoint();
  }
  EXPECT_EQ(timeline->sync_point(), static_cast<unsigned>(kCurrentSyncPoint));

  // The second temporary syncobj should import the sync file first
  EXPECT_CALL(*drm_, SyncobjImportSyncFile(2, kSyncFileFd));

  // Now the temporary syncobj should be transferred to the timeline syncobj at
  // current sync point.
  EXPECT_CALL(*drm_, SyncobjTransfer(1, kCurrentSyncPoint, 2, 0, 0));

  ASSERT_TRUE(timeline->ImportSyncFdAtCurrentSyncPoint(kSyncFileFd));
}

TEST_F(WaylandSyncobjTimelineTest,
       ImportSyncFdAtCurrentSyncPoint_FailOnSyncobjImportSyncFile) {
  constexpr int kSyncFileFd = 123;
  auto timeline = CreateAcquireTimeline();
  EXPECT_EQ(timeline->sync_point(), static_cast<unsigned>(0));
  timeline->IncrementSyncPoint();
  EXPECT_EQ(timeline->sync_point(), static_cast<unsigned>(1));

  EXPECT_CALL(*drm_, SyncobjImportSyncFile(2, kSyncFileFd)).WillOnce([] {
    return ENOENT;
  });

  EXPECT_CALL(*drm_, SyncobjTransfer(_, _, _, _, _)).Times(0);

  ASSERT_FALSE(timeline->ImportSyncFdAtCurrentSyncPoint(kSyncFileFd));
}

TEST_F(WaylandSyncobjTimelineTest,
       ImportSyncFdAtCurrentSyncPoint_FailOnSyncobjTransfer) {
  constexpr int kSyncFileFd = 123;
  auto timeline = CreateAcquireTimeline();
  EXPECT_EQ(timeline->sync_point(), static_cast<unsigned>(0));
  timeline->IncrementSyncPoint();
  EXPECT_EQ(timeline->sync_point(), static_cast<unsigned>(1));

  EXPECT_CALL(*drm_, SyncobjImportSyncFile(2, kSyncFileFd));

  EXPECT_CALL(*drm_, SyncobjTransfer(1, 1, 2, 0, 0)).WillOnce([] {
    return ENOENT;
  });

  ASSERT_FALSE(timeline->ImportSyncFdAtCurrentSyncPoint(kSyncFileFd));
}

TEST_F(WaylandSyncobjTimelineTest, ExportCurrentSyncPointToSyncFd) {
  constexpr int kCurrentSyncPoint = 4;
  auto timeline = CreateReleaseTimeline();
  EXPECT_EQ(timeline->sync_point(), static_cast<unsigned>(0));
  for (auto i = 0; i < kCurrentSyncPoint; i++) {
    timeline->IncrementSyncPoint();
  }
  EXPECT_EQ(timeline->sync_point(), static_cast<unsigned>(kCurrentSyncPoint));

  // First the syncobj should be transferred to a temporary binary syncobj.
  EXPECT_CALL(*drm_, SyncobjTransfer(2, 0, 1, kCurrentSyncPoint, 0));

  base::ScopedFD sync_file_fd = GetFdFactory()->CreateFd();
  // Then the file should be exported as a sync file
  EXPECT_CALL(*drm_, SyncobjExportSyncFile(2, _))
      .WillOnce([fd = sync_file_fd.get()](uint32_t, int* sync_file_fd) {
        *sync_file_fd = HANDLE_EINTR(dup(fd));
        return 0;
      });

  ASSERT_TRUE(timeline->ExportCurrentSyncPointToSyncFd().is_valid());
}

TEST_F(WaylandSyncobjTimelineTest,
       ExportCurrentSyncPointToSyncFd_FailOnSyncobjTransfer) {
  constexpr int kCurrentSyncPoint = 1;
  auto timeline = CreateReleaseTimeline();
  EXPECT_EQ(timeline->sync_point(), static_cast<unsigned>(0));
  timeline->IncrementSyncPoint();
  EXPECT_EQ(timeline->sync_point(), static_cast<unsigned>(kCurrentSyncPoint));

  EXPECT_CALL(*drm_, SyncobjTransfer(2, 0, 1, kCurrentSyncPoint, 0))
      .WillOnce([] { return ENOENT; });

  EXPECT_CALL(*drm_, SyncobjExportSyncFile(_, _)).Times(0);

  ASSERT_FALSE(timeline->ExportCurrentSyncPointToSyncFd().is_valid());
}

TEST_F(WaylandSyncobjTimelineTest,
       ExportCurrentSyncPointToSyncFd_FailOnSyncobjExportSyncFile) {
  constexpr int kCurrentSyncPoint = 1;
  auto timeline = CreateReleaseTimeline();
  timeline->IncrementSyncPoint();
  EXPECT_EQ(timeline->sync_point(), static_cast<unsigned>(kCurrentSyncPoint));

  EXPECT_CALL(*drm_, SyncobjTransfer(2, 0, 1, kCurrentSyncPoint, 0));

  EXPECT_CALL(*drm_, SyncobjExportSyncFile(2, _))
      .WillOnce([](uint32_t, int* sync_file_fd) { return ENOENT; });

  ASSERT_FALSE(timeline->ExportCurrentSyncPointToSyncFd().is_valid());
}

TEST_F(WaylandSyncobjTimelineTest, WaitForFenceAvailableAtCurrentSyncPoint) {
  constexpr int kCurrentSyncPoint = 1;
  auto timeline = CreateReleaseTimeline();
  timeline->IncrementSyncPoint();
  EXPECT_EQ(timeline->sync_point(), static_cast<unsigned>(kCurrentSyncPoint));

  base::RunLoop run_loop;
  base::ScopedFD fd;

  auto callback = base::BindOnce(
      [](base::RepeatingClosure quit_closure, base::ScopedFD* fd_out,
         base::ScopedFD fd) {
        *fd_out = std::move(fd);
        quit_closure.Run();
      },
      run_loop.QuitClosure(), &fd);

  // When eventfd is set notify fence is available immediately by writing to it.
  EXPECT_CALL(*drm_, SyncobjEventfd(1, kCurrentSyncPoint, _,
                                    DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE))
      .WillOnce([](uint32_t, uint64_t, int ev_fd, uint32_t) {
        uint64_t value = 1;
        HANDLE_EINTR(write(ev_fd, &value, sizeof(value)));
        return 0;
      });

  // Ensure exporting current sync point works.
  base::ScopedFD sync_file_fd = GetFdFactory()->CreateFd();
  EXPECT_CALL(*drm_, SyncobjTransfer(2, 0, 1, kCurrentSyncPoint, 0));
  EXPECT_CALL(*drm_, SyncobjExportSyncFile(2, _))
      .WillOnce([fd = sync_file_fd.get()](uint32_t, int* sync_file_fd) {
        *sync_file_fd = HANDLE_EINTR(dup(fd));
        return 0;
      });

  timeline->WaitForFenceAvailableAtCurrentSyncPoint(std::move(callback));
  run_loop.Run();
  Mock::VerifyAndClearExpectations(drm_);
  EXPECT_TRUE(fd.is_valid());

  // Now calling again for the same sync point should not require eventfd.
  fd.reset();
  auto callback2 =
      base::BindOnce([](base::ScopedFD* fd_out,
                        base::ScopedFD fd) { *fd_out = std::move(fd); },
                     &fd);
  EXPECT_CALL(*drm_, SyncobjTransfer(3, 0, 1, kCurrentSyncPoint, 0));
  EXPECT_CALL(*drm_, SyncobjExportSyncFile(3, _))
      .WillOnce([fd = sync_file_fd.get()](uint32_t, int* sync_file_fd) {
        *sync_file_fd = HANDLE_EINTR(dup(fd));
        return 0;
      });
  timeline->WaitForFenceAvailableAtCurrentSyncPoint(std::move(callback2));
  Mock::VerifyAndClearExpectations(drm_);
  EXPECT_TRUE(fd.is_valid());
}

TEST_F(
    WaylandSyncobjTimelineTest,
    WaitForFenceAvailableAtCurrentSyncPoint_RecoverFromOnSyncobjEventfdFailure) {
  constexpr int kCurrentSyncPoint = 1;
  auto timeline = CreateReleaseTimeline();
  timeline->IncrementSyncPoint();
  EXPECT_EQ(timeline->sync_point(), static_cast<unsigned>(kCurrentSyncPoint));

  base::RunLoop run_loop;
  base::ScopedFD fd;

  auto callback = base::BindOnce(
      [](base::RepeatingClosure quit_closure, base::ScopedFD* fd_out,
         base::ScopedFD fd) {
        *fd_out = std::move(fd);
        quit_closure.Run();
      },
      run_loop.QuitClosure(), &fd);

  // Return error from SyncobjEventfd().
  EXPECT_CALL(*drm_, SyncobjEventfd(1, kCurrentSyncPoint, _,
                                    DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE))
      .WillOnce([](uint32_t, uint64_t, int ev_fd, uint32_t) { return ENOENT; });

  // Ensure exporting current sync point works.
  base::ScopedFD sync_file_fd = GetFdFactory()->CreateFd();
  EXPECT_CALL(*drm_, SyncobjTransfer(2, 0, 1, kCurrentSyncPoint, 0));
  EXPECT_CALL(*drm_, SyncobjExportSyncFile(2, _))
      .WillOnce([fd = sync_file_fd.get()](uint32_t, int* sync_file_fd) {
        *sync_file_fd = HANDLE_EINTR(dup(fd));
        return 0;
      });

  timeline->WaitForFenceAvailableAtCurrentSyncPoint(std::move(callback));
  run_loop.Run();
  Mock::VerifyAndClearExpectations(drm_);
  EXPECT_TRUE(fd.is_valid());
}

}  // namespace ui
