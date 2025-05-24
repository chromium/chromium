// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_frame_manager.h"

#include <memory>

#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/scoped_wl_array.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::_;
using ::testing::Mock;

namespace ui {

namespace {

base::ScopedFD MakeFD() {
  base::FilePath temp_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&temp_path));
  auto file =
      base::File(temp_path, base::File::FLAG_READ | base::File::FLAG_WRITE |
                                base::File::FLAG_CREATE_ALWAYS);
  return base::ScopedFD(file.TakePlatformFile());
}

constexpr int kWidth = 800;
constexpr int kHeight = 600;
constexpr size_t kLength = 1024 * 768 * 4;
constexpr gfx::Size kBufferSize = {1024, 768};

}  // namespace

class WaylandFrameManagerTest : public WaylandTestSimple {
 public:
  void SetUp() override {
    WaylandTestSimple::SetUp();
    frame_manager_ =
        std::make_unique<WaylandFrameManager>(window_.get(), connection_.get());
    // Frame callbacks get reset on the client side when they are skipped. So
    // make MockSurface aware of that.
    PostToServerAndWait([id = window_->root_surface()->get_surface_id()](
                            wl::TestWaylandServerThread* server) {
      auto* mock_surface = server->GetObject<wl::MockSurface>(id);
      mock_surface->AllowResettingFrameCallback();
    });
  }

 protected:
  void InitBufferManager() {
    EXPECT_TRUE(connection_->buffer_manager_host());
    auto interface_ptr = connection_->buffer_manager_host()->BindInterface();
    buffer_manager_gpu_->Initialize(std::move(interface_ptr), {},
                                    /*supports_dma_buf=*/false,
                                    /*supports_viewporter=*/true,
                                    /*supports_acquire_fence=*/false,
                                    /*supports_overlays=*/true,
                                    /*supports_single_pixel_buffer=*/true);
  }

  void RecordFrame() {
    buffer_manager_gpu_->CreateShmBasedBuffer(MakeFD(), kLength, kBufferSize,
                                              buffer_id_);
    base::RunLoop().RunUntilIdle();

    wl::WaylandOverlayConfig config;
    config.buffer_id = buffer_id_;
    config.bounds_rect = {0, 0, kWidth, kHeight};
    auto* surface = window_->root_surface();
    std::unique_ptr<WaylandFrame> frame =
        std::make_unique<WaylandFrame>(surface, std::move(config));
    frame_manager_->RecordFrame(std::move(frame));
    ++buffer_id_;
  }

  void TestSurfaceCommits(bool expect_surface_commits) {
    // Setup wl_buffers.
    InitBufferManager();
    buffer_manager_gpu_->CreateShmBasedBuffer(MakeFD(), kLength, kBufferSize,
                                              buffer_id_);
    base::RunLoop().RunUntilIdle();

    wl::WaylandOverlayConfig config;
    config.buffer_id = buffer_id_;
    config.bounds_rect = {0, 0, kWidth, kHeight};

    auto* surface = window_->root_surface();
    std::unique_ptr<WaylandFrame> frame =
        std::make_unique<WaylandFrame>(surface, std::move(config));

    PostToServerAndWait(
        [id = window_->root_surface()->get_surface_id(),
         expect_surface_commits](wl::TestWaylandServerThread* server) {
          auto* mock_surface = server->GetObject<wl::MockSurface>(id);
          int call_count = expect_surface_commits ? 1 : 0;
          EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(call_count);
          EXPECT_CALL(*mock_surface, Frame(_)).Times(call_count);
          EXPECT_CALL(*mock_surface, Commit()).Times(call_count);
        });
    frame_manager_->RecordFrame(std::move(frame));
    EXPECT_EQ(0u, NumPendingFrames());
    EXPECT_EQ(1u, NumSubmittedFrames());
    if (expect_surface_commits) {
      EXPECT_TRUE(LastSubmittedFrameHasFrameCallback());
      EXPECT_TRUE(LastSubmittedFrameHasSubmittedBuffers());
    } else {
      EXPECT_FALSE(LastSubmittedFrameHasFrameCallback());
      EXPECT_FALSE(LastSubmittedFrameHasSubmittedBuffers());
    }
  }

  size_t NumPendingFrames() { return frame_manager_->pending_frames_.size(); }

  size_t NumSubmittedFrames() {
    return frame_manager_->submitted_frames_.size();
  }

  bool LastSubmittedFrameHasFrameCallback() {
    return !!frame_manager_->submitted_frames_.back()->wl_frame_callback;
  }

  bool LastSubmittedFrameAcked() {
    return frame_manager_->submitted_frames_.back()->submission_acked;
  }

  bool LastSubmittedFrameHasSubmittedBuffers() {
    return !frame_manager_->submitted_frames_.back()->submitted_buffers.empty();
  }

  void SendFrameCallback() {
    PostToServerAndWait([id = window_->root_surface()->get_surface_id()](
                            wl::TestWaylandServerThread* server) {
      auto* mock_surface = server->GetObject<wl::MockSurface>(id);
      mock_surface->SendFrameCallback();
    });
  }

  std::unique_ptr<WaylandFrameManager> frame_manager_;
  size_t buffer_id_ = 1;
};

// Tests that frame callbacks are waited on during video capture if there is no
// frame callback timeout.
TEST_F(WaylandFrameManagerTest, WaitForFrameCallbacks_VideoCapturingNoTimeout) {
  InitBufferManager();

  frame_manager_->SetVideoCapture();

  // The first frame should be submitted.
  RecordFrame();
  EXPECT_EQ(0u, NumPendingFrames());
  EXPECT_EQ(1u, NumSubmittedFrames());

  // The second frame should be pending until frame callback is received from
  // the previous frame.
  RecordFrame();
  EXPECT_EQ(1u, NumPendingFrames());
  EXPECT_EQ(1u, NumSubmittedFrames());
}

// Tests frame callbacks are waited on when there is a timeout and video is not
// being captured.
TEST_F(WaylandFrameManagerTest,
       WaitForFrameCallbacks_TimeoutVideoNotCapturing) {
  InitBufferManager();

  frame_manager_->SetVideoCapture();
  frame_manager_->SetVideoCapture();
  frame_manager_->ReleaseVideoCapture();
  frame_manager_->ReleaseVideoCapture();
  frame_manager_->FrameCallbackTimeout();

  // The first frame should be submitted.
  RecordFrame();
  EXPECT_EQ(0u, NumPendingFrames());
  EXPECT_EQ(1u, NumSubmittedFrames());

  // The second frame should be pending until frame callback is received from
  // the previous frame.
  RecordFrame();
  EXPECT_EQ(1u, NumPendingFrames());
  EXPECT_EQ(1u, NumSubmittedFrames());
}

// Tests frame callbacks are NOT waited on when there is a timeout during video
// capture.
TEST_F(WaylandFrameManagerTest, FrameCallbacksSkipped_TimeoutVideoCapturing) {
  InitBufferManager();

  // Ensure at least one video capture exists.
  frame_manager_->SetVideoCapture();
  frame_manager_->SetVideoCapture();
  frame_manager_->ReleaseVideoCapture();
  // Trigger frame callback timeout, to detect the freeze.
  frame_manager_->FrameCallbackTimeout();

  // The first frame should be submitted.
  RecordFrame();
  EXPECT_EQ(0u, NumPendingFrames());
  EXPECT_EQ(1u, NumSubmittedFrames());

  // The second frame should be submitted without waiting for frame callback
  // from the first one to be received.
  RecordFrame();
  EXPECT_EQ(0u, NumPendingFrames());
  EXPECT_EQ(2u, NumSubmittedFrames());

  // Send last frame callback, which removes the freeze.
  SendFrameCallback();

  // Now this frame should be submitted as the frame callback for the last
  // submitted frame was received.
  RecordFrame();
  EXPECT_EQ(0u, NumPendingFrames());
  EXPECT_EQ(3u, NumSubmittedFrames());

  // Now it should go back to waiting as there is no longer a freeze.
  RecordFrame();
  EXPECT_EQ(1u, NumPendingFrames());
  EXPECT_EQ(3u, NumSubmittedFrames());
}

// Tests that frames are unblocked when both video capture state and frame
// callback frozen state become true and we transition to fallback rendering.
TEST_F(WaylandFrameManagerTest,
       UnblockFrames_OnTransitionToSkipFrameCallbacks) {
  // Setup wl_buffers.
  InitBufferManager();
  RecordFrame();
  EXPECT_EQ(1u, NumSubmittedFrames());
  EXPECT_TRUE(LastSubmittedFrameHasFrameCallback());
  EXPECT_TRUE(LastSubmittedFrameHasSubmittedBuffers());
  EXPECT_EQ(0u, NumPendingFrames());
  EXPECT_TRUE(LastSubmittedFrameHasFrameCallback());

  // Ensure pending frame
  RecordFrame();
  EXPECT_EQ(1u, NumSubmittedFrames());
  EXPECT_EQ(1u, NumPendingFrames());

  // Trigger timeout
  frame_manager_->FrameCallbackTimeout();

  // Ensure at least one video capture is active.
  frame_manager_->SetVideoCapture();

  // The empty pending frame should be cleared.
  EXPECT_EQ(0u, NumPendingFrames());

  // The second frame should be submitted without waiting for a frame callback
  // from the first one.
  EXPECT_EQ(2u, NumSubmittedFrames());
}

// Tests video capture should not affect surface commit if window is not
// suspended and is active.
TEST_F(WaylandFrameManagerTest, SurfaceCommitted_WindowActiveNotSuspended) {
  WaylandWindow::WindowStates window_states;
  window_states.is_activated = true;
  window_states.is_suspended = false;
  window_->HandleToplevelConfigure(kWidth, kHeight, window_states);

  frame_manager_->SetVideoCapture();

  TestSurfaceCommits(/*expect_surface_commits=*/true);
}

// Tests surface commits are sent as usual when window is suspended but video is
// not being captured.
TEST_F(WaylandFrameManagerTest,
       SurfaceCommitted_WindowInactiveVideoNotCapturing) {
  WaylandWindow::WindowStates window_states;
  window_states.is_activated = true;
  window_states.is_suspended = true;
  window_->HandleToplevelConfigure(kWidth, kHeight, window_states);

  // Capture count should be zero.
  frame_manager_->SetVideoCapture();
  frame_manager_->SetVideoCapture();
  frame_manager_->ReleaseVideoCapture();
  frame_manager_->ReleaseVideoCapture();

  TestSurfaceCommits(/*expect_surface_commits=*/true);
}

// Tests that swaps are ACKed immediately without involving the compositor when
// window is suspended during video capture.
TEST_F(WaylandFrameManagerTest,
       AckSwapWithoutCommit_WindowSuspendedVideoCapturing) {
  // Make window suspended
  WaylandWindow::WindowStates window_states;
  window_states.is_activated = true;
  window_states.is_suspended = true;
  window_->HandleToplevelConfigure(kWidth, kHeight, window_states);

  // Ensure at least one video capture is active.
  frame_manager_->SetVideoCapture();
  frame_manager_->SetVideoCapture();
  frame_manager_->ReleaseVideoCapture();

  TestSurfaceCommits(/*expect_surface_commits=*/false);

  constexpr uint32_t kBufferId2 = 2;
  buffer_manager_gpu_->CreateShmBasedBuffer(MakeFD(), kLength, kBufferSize,
                                            kBufferId2);
  base::RunLoop().RunUntilIdle();

  wl::WaylandOverlayConfig config2;
  config2.buffer_id = kBufferId2;
  config2.bounds_rect = {0, 0, kWidth, kHeight};
  auto frame2 = std::make_unique<WaylandFrame>(window_->root_surface(),
                                               std::move(config2));

  PostToServerAndWait([id = window_->root_surface()->get_surface_id()](
                          wl::TestWaylandServerThread* server) {
    auto* mock_surface = server->GetObject<wl::MockSurface>(id);
    EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(0);
    EXPECT_CALL(*mock_surface, Frame(_)).Times(0);
    EXPECT_CALL(*mock_surface, Commit()).Times(0);
  });
  frame_manager_->RecordFrame(std::move(frame2));
  // The second frame should be submitted and ACK-ed and the first frame should
  // be cleared.
  EXPECT_EQ(0u, NumPendingFrames());
  EXPECT_EQ(1u, NumSubmittedFrames());
  EXPECT_FALSE(LastSubmittedFrameHasFrameCallback());
  EXPECT_FALSE(LastSubmittedFrameHasSubmittedBuffers());
  EXPECT_TRUE(LastSubmittedFrameAcked());
}

TEST_F(WaylandFrameManagerTest, UnblockFrames_OnTransitionToSwapWithoutCommit) {
  // Setup wl_buffers.
  InitBufferManager();
  RecordFrame();
  EXPECT_EQ(1u, NumSubmittedFrames());
  EXPECT_EQ(0u, NumPendingFrames());
  EXPECT_TRUE(LastSubmittedFrameHasFrameCallback());

  // Ensure pending frame
  RecordFrame();
  EXPECT_EQ(1u, NumSubmittedFrames());
  EXPECT_EQ(1u, NumPendingFrames());

  // Send frame callback and record another frame to end up with 2 submitted
  // frames and one pending.
  SendFrameCallback();
  RecordFrame();
  EXPECT_EQ(2u, NumSubmittedFrames());
  EXPECT_EQ(1u, NumPendingFrames());

  // Make window suspended
  WaylandWindow::WindowStates window_states;
  window_states.is_suspended = true;
  window_->HandleToplevelConfigure(kWidth, kHeight, window_states);

  // Ensure at least one video capture is active.
  frame_manager_->SetVideoCapture();

  // There should be no more pending frames.
  EXPECT_EQ(0u, NumPendingFrames());

  // 2 submitted frames should be cleared and one pending frame should be
  // submitted.
  EXPECT_EQ(1u, NumSubmittedFrames());
  EXPECT_FALSE(LastSubmittedFrameHasFrameCallback());
  EXPECT_FALSE(LastSubmittedFrameHasSubmittedBuffers());
  EXPECT_TRUE(LastSubmittedFrameAcked());
}

}  // namespace ui
