// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_thread.h"

#include <utility>

#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device_generator.h"
namespace ui {

namespace {

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

  std::unique_ptr<base::WaitableEvent> PostStubTaskWithWaitableEvent() {
    base::OnceClosure task = base::BindOnce(StubTask);
    auto event = std::make_unique<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    drm_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&DrmThread::RunTaskAfterDeviceReady,
                                  base::Unretained(&drm_thread_),
                                  std::move(task), event.get()));
    return event;
  }

  void PostStubTask(bool* done) {
    *done = false;
    base::OnceClosure task = base::BindOnce(StubTaskWithDoneFeedback, done);
    drm_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&DrmThread::RunTaskAfterDeviceReady,
                                  base::Unretained(&drm_thread_),
                                  std::move(task), nullptr));
  }

  void AddGraphicsDevice() {
    base::FilePath file_path("/dev/null");
    base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_WRITE |
                                   base::File::FLAG_READ);
    drm_device_->AddGraphicsDevice(
        file_path,
        mojo::PlatformHandle(base::ScopedFD(file.TakePlatformFile())));
  }

  base::test::TaskEnvironment env_;
  DrmThread drm_thread_;
  mojo::Remote<ozone::mojom::DrmDevice> drm_device_;
};

TEST_F(DrmThreadTest, RunTaskAfterDeviceReady) {
  bool called = false;

  // Post 2 tasks. One with WaitableEvent and one without. They should block on
  // a graphics device becoming available.
  std::unique_ptr<base::WaitableEvent> event = PostStubTaskWithWaitableEvent();
  PostStubTask(&called);
  drm_thread_.FlushForTesting();
  EXPECT_FALSE(event->IsSignaled());
  EXPECT_FALSE(called);

  // Add the graphics device. The tasks should run.
  AddGraphicsDevice();
  drm_thread_.FlushForTesting();
  ASSERT_TRUE(event->IsSignaled());
  ASSERT_TRUE(called);

  // Now that a graphics device is available, further tasks should execute
  // immediately.
  event = PostStubTaskWithWaitableEvent();
  PostStubTask(&called);
  drm_thread_.FlushForTesting();
  ASSERT_TRUE(event->IsSignaled());
  ASSERT_TRUE(called);
}

// Verifies that we gracefully handle the case where CheckOverlayCapabilities()
// is called on a destroyed window.
TEST_F(DrmThreadTest, CheckOverlayCapabilitiesDestroyedWindow) {
  gfx::AcceleratedWidget widget = 5;
  constexpr gfx::Rect bounds(10, 10);
  constexpr size_t candidates_size = 9;
  std::vector<OverlaySurfaceCandidate> candidates(candidates_size);
  std::vector<OverlayStatus> result;
  AddGraphicsDevice();
  drm_device_->CreateWindow(widget, bounds);
  drm_device_->DestroyWindow(widget);
  drm_device_.FlushForTesting();
  drm_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DrmThread::CheckOverlayCapabilitiesSync,
                                base::Unretained(&drm_thread_), widget,
                                candidates, &result));
  drm_thread_.FlushForTesting();
  EXPECT_EQ(candidates_size, result.size());
  for (const auto& status : result) {
    EXPECT_EQ(OVERLAY_STATUS_NOT, status);
  }
}

}  // namespace ui
