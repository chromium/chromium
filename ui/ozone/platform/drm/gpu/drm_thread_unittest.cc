// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_thread.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/mock_gbm_device.h"

namespace ui {

namespace {

class FakeDrmDeviceGenerator : public DrmDeviceGenerator {
  // DrmDeviceGenerator:
  scoped_refptr<DrmDevice> CreateDevice(const base::FilePath& path,
                                        base::File file,
                                        bool is_primary_device) override {
    auto gbm_device = std::make_unique<MockGbmDevice>();
    return base::MakeRefCounted<MockDrmDevice>(std::move(gbm_device));
  }
};

void StubTask() {}

void StubTaskWithDoneFeedback(bool* done) {
  *done = true;
}

}  // namespace

class DrmThreadTest : public testing::Test {
 protected:
  // Overridden from testing::Test
  void SetUp() override {
    drm_thread_.Start(base::DoNothing(),
                      std::make_unique<FakeDrmDeviceGenerator>());
    drm_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&DrmThread::AddDrmDeviceReceiver,
                                  base::Unretained(&drm_thread_),
                                  drm_device_.BindNewPipeAndPassReceiver()));
    drm_thread_.FlushForTesting();
  }

  std::unique_ptr<base::WaitableEvent> PostStubTaskWithWaitableEvent(
      gfx::AcceleratedWidget window) {
    base::OnceClosure task = base::BindOnce(StubTask);
    auto event = std::make_unique<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    drm_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&DrmThread::RunTaskAfterWindowReady,
                                  base::Unretained(&drm_thread_), window,
                                  std::move(task), event.get()));
    return event;
  }

  void PostStubTask(gfx::AcceleratedWidget window, bool* done) {
    *done = false;
    base::OnceClosure task = base::BindOnce(StubTaskWithDoneFeedback, done);
    drm_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&DrmThread::RunTaskAfterWindowReady,
                                  base::Unretained(&drm_thread_), window,
                                  std::move(task), nullptr));
  }

  void AddGraphicsDevice() {
    base::FilePath file_path("/dev/null");
    base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_WRITE |
                                   base::File::FLAG_READ);
    drm_device_->AddGraphicsDevice(file_path, std::move(file));
  }

  base::test::TaskEnvironment env_;
  DrmThread drm_thread_;
  mojo::Remote<ozone::mojom::DrmDevice> drm_device_;
};

TEST_F(DrmThreadTest, RunTaskAfterWindowReady) {
  constexpr gfx::Rect bounds(10, 10);
  bool called1 = false, called2 = false;
  gfx::AcceleratedWidget widget1 = 1, widget2 = 2;

  // Post a task not blocked on any window. It should still block on a graphics
  // device becoming available.
  PostStubTask(gfx::kNullAcceleratedWidget, &called1);
  drm_thread_.FlushForTesting();
  EXPECT_FALSE(called1);

  // Add the graphics device. The task should run.
  AddGraphicsDevice();
  drm_thread_.FlushForTesting();
  ASSERT_TRUE(called1);

  // Now that a graphics device is available, further tasks that don't block on
  // any window should execute immediately.
  PostStubTask(gfx::kNullAcceleratedWidget, &called1);
  drm_thread_.FlushForTesting();
  ASSERT_TRUE(called1);

  // Post a task blocked on |widget1|. It shouldn't run.
  PostStubTask(widget1, &called1);
  drm_thread_.FlushForTesting();
  ASSERT_FALSE(called1);

  // Post two tasks blocked on |widget2|, one with a WaitableEvent and one
  // without. They shouldn't run.
  std::unique_ptr<base::WaitableEvent> event =
      PostStubTaskWithWaitableEvent(widget2);
  PostStubTask(widget2, &called2);
  drm_thread_.FlushForTesting();
  ASSERT_FALSE(event->IsSignaled());
  ASSERT_FALSE(called2);

  // Now create |widget1|. The first task should run.
  drm_device_->CreateWindow(widget1, bounds);
  drm_thread_.FlushForTesting();
  ASSERT_TRUE(called1);
  ASSERT_FALSE(event->IsSignaled());
  ASSERT_FALSE(called2);

  // Now that |widget1| is created. any further task depending on it should run
  // immediately.
  PostStubTask(widget1, &called1);
  drm_thread_.FlushForTesting();
  ASSERT_TRUE(called1);
  ASSERT_FALSE(event->IsSignaled());
  ASSERT_FALSE(called2);

  // Destroy |widget1| and post a task blocked on it. The task should still run
  // immediately even though the window is destroyed.
  drm_device_->DestroyWindow(widget1);
  PostStubTask(widget1, &called1);
  drm_thread_.FlushForTesting();
  ASSERT_TRUE(called1);
  ASSERT_FALSE(event->IsSignaled());
  ASSERT_FALSE(called2);

  // Create |widget2|. The two blocked tasks should run.
  drm_device_->CreateWindow(widget2, bounds);
  drm_thread_.FlushForTesting();
  ASSERT_TRUE(event->IsSignaled());
  ASSERT_TRUE(called2);

  // Post another task blocked on |widget1| with a WaitableEvent. It should run
  // immediately.
  event = PostStubTaskWithWaitableEvent(widget1);
  drm_thread_.FlushForTesting();
  ASSERT_TRUE(event->IsSignaled());

  // Post another task blocked on |widget2| with a WaitableEvent. It should run
  // immediately.
  event = PostStubTaskWithWaitableEvent(widget2);
  drm_thread_.FlushForTesting();
  ASSERT_TRUE(event->IsSignaled());

  // Destroy |widget2| to avoid failures during tear down.
  drm_device_->DestroyWindow(widget2);
  drm_thread_.FlushForTesting();
}

}  // namespace ui
