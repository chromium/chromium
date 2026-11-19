// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_device.h"

#include <unistd.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device.h"

namespace ui {

namespace {

// Reports whether it was destroyed on `drm_task_runner`'s thread.
class DestructionTrackingDrmDevice : public FakeDrmDevice {
 public:
  DestructionTrackingDrmDevice(
      base::ScopedFD fd,
      scoped_refptr<base::SingleThreadTaskRunner> drm_task_runner,
      std::optional<bool>* destroyed_on_drm_thread)
      : FakeDrmDevice(base::FilePath("/test/dri/card1"),
                      std::move(fd),
                      std::make_unique<MockGbmDevice>(),
                      /*is_primary_device=*/false),
        drm_task_runner_(std::move(drm_task_runner)),
        destroyed_on_drm_thread_(destroyed_on_drm_thread) {}

 private:
  ~DestructionTrackingDrmDevice() override {
    *destroyed_on_drm_thread_ = drm_task_runner_->RunsTasksInCurrentSequence();
  }

  const scoped_refptr<base::SingleThreadTaskRunner> drm_task_runner_;
  const raw_ptr<std::optional<bool>> destroyed_on_drm_thread_;
};

}  // namespace

class DrmDeviceTest : public testing::Test {
 protected:
  void SetUp() override {
    base::Thread::Options thread_options;
    thread_options.message_pump_type = base::MessagePumpType::IO;
    ASSERT_TRUE(drm_thread_.StartWithOptions(std::move(thread_options)));

    int fds[2] = {-1, -1};
    ASSERT_EQ(0, pipe(fds));
    drm_read_fd_.reset(fds[0]);
    drm_write_fd_.reset(fds[1]);
  }

  void TearDown() override { drm_thread_.Stop(); }

  scoped_refptr<DestructionTrackingDrmDevice>
  CreateDeviceWithWatcherOnDrmThread() {
    scoped_refptr<DestructionTrackingDrmDevice> device;
    base::WaitableEvent event;
    drm_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([&, fd = std::move(drm_read_fd_)]() mutable {
          device = base::MakeRefCounted<DestructionTrackingDrmDevice>(
              std::move(fd), drm_thread_.task_runner(),
              &destroyed_on_drm_thread_);
          device->ResetStateWithDefaultObjects(/*crtc_count=*/1,
                                               /*planes_per_crtc=*/1);
          device->InitializeState(/*use_atomic=*/true);
          device->StartPageFlipWatcherForTesting();
          event.Signal();
        }));
    event.Wait();
    return device;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::Thread drm_thread_{"TestDrmThread"};
  base::ScopedFD drm_read_fd_;
  // Kept open so that `drm_read_fd_` doesn't see EPOLLHUP / EOF, which would
  // make the page flip watcher unregister itself.
  base::ScopedFD drm_write_fd_;
  std::optional<bool> destroyed_on_drm_thread_;
};

// The last reference can be dropped off the DRM thread (e.g. by a
// DrmFramebuffer on the GPU main thread).
TEST_F(DrmDeviceTest, LastRefDroppedOffThread) {
  scoped_refptr<DestructionTrackingDrmDevice> device =
      CreateDeviceWithWatcherOnDrmThread();
  ASSERT_TRUE(device->HasOneRef());

  // Block the DRM thread so that the deletion can only be posted, not run.
  base::WaitableEvent drm_thread_blocked;
  base::WaitableEvent unblock_drm_thread;
  drm_thread_.task_runner()->PostTask(FROM_HERE,
                                      base::BindLambdaForTesting([&]() {
                                        drm_thread_blocked.Signal();
                                        unblock_drm_thread.Wait();
                                      }));
  drm_thread_blocked.Wait();

  device.reset();
  EXPECT_FALSE(destroyed_on_drm_thread_.has_value());

  unblock_drm_thread.Signal();
  drm_thread_.FlushForTesting();

  EXPECT_EQ(destroyed_on_drm_thread_, true);
}

TEST_F(DrmDeviceTest, LastRefDroppedOnDrmThread) {
  scoped_refptr<DestructionTrackingDrmDevice> device =
      CreateDeviceWithWatcherOnDrmThread();
  ASSERT_TRUE(device->HasOneRef());

  drm_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindLambdaForTesting([&, device = std::move(device)]() mutable {
        device.reset();
        // Destroyed synchronously, not via a posted task.
        EXPECT_EQ(destroyed_on_drm_thread_, true);
      }));
  drm_thread_.FlushForTesting();

  EXPECT_EQ(destroyed_on_drm_thread_, true);
}

}  // namespace ui
