// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/delegated_frame_host_android.h"
#include "base/android/build_info.h"
#include "base/test/test_mock_time_task_runner.h"
#include "cc/layers/layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android_compositor.h"

namespace ui {
namespace {

using ::testing::Return;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Invoke;

class MockDelegatedFrameHostAndroidClient
    : public DelegatedFrameHostAndroid::Client {
 public:
  MOCK_METHOD1(SetBeginFrameSource, void(viz::BeginFrameSource*));
  MOCK_METHOD1(DidReceiveCompositorFrameAck,
               void(const std::vector<viz::ReturnedResource>&));
  MOCK_METHOD1(ReclaimResources,
               void(const std::vector<viz::ReturnedResource>&));
  MOCK_METHOD2(DidPresentCompositorFrame,
               void(uint32_t, const gfx::PresentationFeedback&));
  MOCK_METHOD1(OnFrameTokenChanged, void(uint32_t));
  MOCK_METHOD0(WasEvicted, void());
};

class MockWindowAndroidCompositor : public WindowAndroidCompositor {
 public:
  MOCK_METHOD1(AttachLayerForReadback, void(scoped_refptr<cc::Layer>));
  MOCK_METHOD1(DoRequestCopyOfOutputOnRootLayer, void(viz::CopyOutputRequest*));
  MOCK_METHOD0(SetNeedsAnimate, void());
  MOCK_METHOD0(GetResourceManager, ResourceManager&());
  MOCK_METHOD0(GetFrameSinkId, viz::FrameSinkId());
  MOCK_METHOD1(AddChildFrameSink, void(const viz::FrameSinkId&));
  MOCK_METHOD1(RemoveChildFrameSink, void(const viz::FrameSinkId&));
  MOCK_METHOD2(DoGetCompositorLock,
               CompositorLock*(CompositorLockClient*, base::TimeDelta));
  MOCK_CONST_METHOD0(IsDrawingFirstVisibleFrame, bool());
  MOCK_METHOD1(SetVSyncPaused, void(bool));

  // Helpers for move-only types:
  void RequestCopyOfOutputOnRootLayer(
      std::unique_ptr<viz::CopyOutputRequest> request) override {
    return DoRequestCopyOfOutputOnRootLayer(request.get());
  }

  std::unique_ptr<CompositorLock> GetCompositorLock(
      CompositorLockClient* client,
      base::TimeDelta time_delta) override {
    return std::unique_ptr<CompositorLock>(
        DoGetCompositorLock(client, time_delta));
  }
};

class DelegatedFrameHostAndroidTest : public testing::Test {
 public:
  DelegatedFrameHostAndroidTest()
      : frame_sink_manager_impl_(&shared_bitmap_manager_),
        frame_sink_id_(1, 1),
        task_runner_(new base::TestMockTimeTaskRunner()),
        lock_manager_(task_runner_) {
    host_frame_sink_manager_.SetLocalManager(&frame_sink_manager_impl_);
    frame_sink_manager_impl_.SetLocalClient(&host_frame_sink_manager_);
  }

  void SetUp() override {
    view_.SetLayer(cc::SolidColorLayer::Create());
    frame_host_ = std::make_unique<DelegatedFrameHostAndroid>(
        &view_, &host_frame_sink_manager_, &client_, frame_sink_id_,
        ShouldEnableSurfaceSynchronization());
  }

  void TearDown() override { frame_host_.reset(); }

  virtual bool ShouldEnableSurfaceSynchronization() const { return false; }

  ui::CompositorLock* GetLock(CompositorLockClient* client,
                              base::TimeDelta time_delta) {
    return lock_manager_.GetCompositorLock(client, time_delta, nullptr)
        .release();
  }

  bool IsLocked() const { return lock_manager_.IsLocked(); }

  void SubmitCompositorFrame(const gfx::Size& frame_size = gfx::Size(10, 10)) {
    viz::CompositorFrame frame =
        viz::CompositorFrameBuilder()
            .AddRenderPass(gfx::Rect(frame_size), gfx::Rect(frame_size))
            .Build();
    frame_host_->SubmitCompositorFrame(allocator_.GenerateId(),
                                       std::move(frame), base::nullopt);
  }

  void SetUpValidFrame(const gfx::Size& frame_size) {
    EXPECT_CALL(compositor_, IsDrawingFirstVisibleFrame())
        .WillOnce(Return(true));
    EXPECT_CALL(compositor_, DoGetCompositorLock(frame_host_.get(), _))
        .WillOnce(Invoke(this, &DelegatedFrameHostAndroidTest::GetLock));
    frame_host_->AttachToCompositor(&compositor_);
    EXPECT_TRUE(IsLocked());

    SubmitCompositorFrame(frame_size);
    EXPECT_FALSE(IsLocked());
  }

 protected:
  MockWindowAndroidCompositor compositor_;
  ui::ViewAndroid view_;
  viz::ServerSharedBitmapManager shared_bitmap_manager_;
  viz::FrameSinkManagerImpl frame_sink_manager_impl_;
  viz::HostFrameSinkManager host_frame_sink_manager_;
  MockDelegatedFrameHostAndroidClient client_;
  viz::FrameSinkId frame_sink_id_;
  viz::ParentLocalSurfaceIdAllocator allocator_;
  std::unique_ptr<DelegatedFrameHostAndroid> frame_host_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  CompositorLockManager lock_manager_;
};

class DelegatedFrameHostAndroidSurfaceSynchronizationTest
    : public DelegatedFrameHostAndroidTest {
 public:
  DelegatedFrameHostAndroidSurfaceSynchronizationTest() = default;
  ~DelegatedFrameHostAndroidSurfaceSynchronizationTest() override = default;

  bool ShouldEnableSurfaceSynchronization() const override { return true; }
};

// Resize lock is only enabled on O+.
static bool IsResizeLockEnabled() {
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
         base::android::SDK_VERSION_OREO;
}

// If surface synchronization is enabled then we should not be acquiring a
// compositor lock on attach.
TEST_F(DelegatedFrameHostAndroidSurfaceSynchronizationTest,
       NoCompositorLockOnAttach) {
  EXPECT_CALL(compositor_, IsDrawingFirstVisibleFrame()).Times(0);
  EXPECT_CALL(compositor_, DoGetCompositorLock(_, _)).Times(0);
  frame_host_->AttachToCompositor(&compositor_);
}

// If surface synchronization is off, and we are doing a cross-process
// navigation, then both the primary and fallback surface IDs need to be
// updated together.
TEST_F(DelegatedFrameHostAndroidTest, TakeFallbackContentFromUpdatesPrimary) {
  EXPECT_FALSE(frame_host_->SurfaceId().is_valid());
  // Submit a compositor frame to ensure we have delegated content.
  SubmitCompositorFrame();

  EXPECT_TRUE(frame_host_->SurfaceId().is_valid());
  std::unique_ptr<DelegatedFrameHostAndroid> other_frame_host =
      std::make_unique<DelegatedFrameHostAndroid>(
          &view_, &host_frame_sink_manager_, &client_, viz::FrameSinkId(2, 2),
          ShouldEnableSurfaceSynchronization());

  EXPECT_FALSE(other_frame_host->SurfaceId().is_valid());

  other_frame_host->TakeFallbackContentFrom(frame_host_.get());

  EXPECT_TRUE(other_frame_host->SurfaceId().is_valid());
  EXPECT_EQ(other_frame_host->content_layer_for_testing()->surface_id(),
            other_frame_host->content_layer_for_testing()
                ->oldest_acceptable_fallback());
}

TEST_F(DelegatedFrameHostAndroidTest, CompositorLockDuringFirstFrame) {
  // Attach during the first frame, lock will be taken.
  EXPECT_CALL(compositor_, IsDrawingFirstVisibleFrame()).WillOnce(Return(true));
  EXPECT_CALL(compositor_, DoGetCompositorLock(frame_host_.get(), _))
      .WillOnce(Invoke(this, &DelegatedFrameHostAndroidTest::GetLock));
  frame_host_->AttachToCompositor(&compositor_);
  EXPECT_TRUE(IsLocked());

  // Lock should be released when we submit a compositor frame.
  SubmitCompositorFrame();
  EXPECT_FALSE(IsLocked());
}

TEST_F(DelegatedFrameHostAndroidTest, CompositorLockDuringLaterFrame) {
  // Attach after the first frame, lock will not be taken.
  EXPECT_CALL(compositor_, IsDrawingFirstVisibleFrame())
      .WillOnce(Return(false));
  EXPECT_CALL(compositor_, DoGetCompositorLock(_, _)).Times(0);
  frame_host_->AttachToCompositor(&compositor_);
}

TEST_F(DelegatedFrameHostAndroidTest, CompositorLockWithDelegatedContent) {
  // Submit a compositor frame to ensure we have delegated content.
  SubmitCompositorFrame();

  // Even though it's the first frame, we won't take the lock as we already have
  // delegated content.
  EXPECT_CALL(compositor_, IsDrawingFirstVisibleFrame()).WillOnce(Return(true));
  EXPECT_CALL(compositor_, DoGetCompositorLock(_, _)).Times(0);
  frame_host_->AttachToCompositor(&compositor_);
}

TEST_F(DelegatedFrameHostAndroidTest, CompositorLockReleasedWithDetach) {
  // Attach during the first frame, lock will be taken.
  EXPECT_CALL(compositor_, IsDrawingFirstVisibleFrame()).WillOnce(Return(true));
  EXPECT_CALL(compositor_, DoGetCompositorLock(frame_host_.get(), _))
      .WillOnce(Invoke(this, &DelegatedFrameHostAndroidTest::GetLock));
  frame_host_->AttachToCompositor(&compositor_);
  EXPECT_TRUE(IsLocked());

  // Lock should be released when we detach.
  frame_host_->DetachFromCompositor();
  EXPECT_FALSE(IsLocked());
}

TEST_F(DelegatedFrameHostAndroidTest, ResizeLockBasic) {
  // Resize lock is only enabled on O+.
  if (!IsResizeLockEnabled())
    return;

  SetUpValidFrame(gfx::Size(10, 10));

  // Tell the frame host to resize, it should take a lock.
  EXPECT_CALL(compositor_, DoGetCompositorLock(frame_host_.get(), _))
      .WillOnce(Invoke(this, &DelegatedFrameHostAndroidTest::GetLock));
  frame_host_->PixelSizeWillChange(gfx::Size(50, 50));
  EXPECT_TRUE(IsLocked());

  // Submit a frame of the wrong size, nothing should change.
  SubmitCompositorFrame(gfx::Size(20, 20));
  EXPECT_TRUE(IsLocked());

  // Submit a frame with the right size, the lock should release.
  SubmitCompositorFrame(gfx::Size(50, 50));
  EXPECT_FALSE(IsLocked());
}

TEST_F(DelegatedFrameHostAndroidTest, ResizeLockNotTakenIfNoSizeChange) {
  // Resize lock is only enabled on O+.
  if (!IsResizeLockEnabled())
    return;

  SetUpValidFrame(gfx::Size(10, 10));

  // Tell the frame host to resize to the existing size, nothing should happen.
  frame_host_->PixelSizeWillChange(gfx::Size(10, 10));
  EXPECT_FALSE(IsLocked());
}

TEST_F(DelegatedFrameHostAndroidTest, ResizeLockReleasedWithDetach) {
  // Resize lock is only enabled on O+.
  if (!IsResizeLockEnabled())
    return;

  SetUpValidFrame(gfx::Size(10, 10));

  // Tell the frame host to resize, it should take a lock.
  EXPECT_CALL(compositor_, DoGetCompositorLock(frame_host_.get(), _))
      .WillOnce(Invoke(this, &DelegatedFrameHostAndroidTest::GetLock));
  frame_host_->PixelSizeWillChange(gfx::Size(50, 50));
  EXPECT_TRUE(IsLocked());

  // Lock should be released when we detach.
  frame_host_->DetachFromCompositor();
  EXPECT_FALSE(IsLocked());
}

TEST_F(DelegatedFrameHostAndroidTest, TestBothCompositorLocks) {
  // Resize lock is only enabled on O+.
  if (!IsResizeLockEnabled())
    return;

  // Attach during the first frame, first lock will be taken.
  EXPECT_CALL(compositor_, IsDrawingFirstVisibleFrame()).WillOnce(Return(true));
  EXPECT_CALL(compositor_, DoGetCompositorLock(frame_host_.get(), _))
      .WillOnce(Invoke(this, &DelegatedFrameHostAndroidTest::GetLock));
  frame_host_->AttachToCompositor(&compositor_);
  EXPECT_TRUE(IsLocked());

  // Tell the frame host to resize, it should take a second lock.
  EXPECT_CALL(compositor_, DoGetCompositorLock(frame_host_.get(), _))
      .WillOnce(Invoke(this, &DelegatedFrameHostAndroidTest::GetLock));
  frame_host_->PixelSizeWillChange(gfx::Size(50, 50));
  EXPECT_TRUE(IsLocked());

  // Submit a compositor frame of the right size, both locks should release.
  SubmitCompositorFrame(gfx::Size(50, 50));
  EXPECT_FALSE(IsLocked());
}

// Make sure frame evictor is notified of the newly embedded surface after
// WasShown.
TEST_F(DelegatedFrameHostAndroidSurfaceSynchronizationTest, EmbedWhileHidden) {
  {
    EXPECT_CALL(client_, WasEvicted());
    frame_host_->EvictDelegatedFrame();
  }
  EXPECT_FALSE(frame_host_->HasSavedFrame());
  viz::LocalSurfaceId id = allocator_.GenerateId();
  gfx::Size size(100, 100);
  frame_host_->WasHidden();
  frame_host_->EmbedSurface(id, size, cc::DeadlinePolicy::UseDefaultDeadline());
  EXPECT_FALSE(frame_host_->HasSavedFrame());
  frame_host_->WasShown(id, size);
  EXPECT_TRUE(frame_host_->HasSavedFrame());
}

// Verify that when a source rect or output size is not provided to
// CopyFromCompositingSurface, the corresponding values in CopyOutputRequest
// are also not initialized.
TEST_F(DelegatedFrameHostAndroidSurfaceSynchronizationTest,
       FullSurfaceCapture) {
  // First embed a surface to make sure we have something to copy from.
  viz::LocalSurfaceId id = allocator_.GenerateId();
  gfx::Size size(100, 100);
  frame_host_->EmbedSurface(id, size, cc::DeadlinePolicy::UseDefaultDeadline());

  // Request readback without source rect or output size specified.
  frame_host_->CopyFromCompositingSurface(gfx::Rect(), gfx::Size(),
                                          base::DoNothing());

  // Make sure the resulting CopyOutputRequest does not have its area or result
  // selection set.
  const std::vector<
      std::pair<viz::LocalSurfaceId, std::unique_ptr<viz::CopyOutputRequest>>>&
      requests = frame_sink_manager_impl_.GetFrameSinkForId(frame_sink_id_)
                     ->copy_output_requests_for_testing();
  ASSERT_EQ(1u, requests.size());
  viz::CopyOutputRequest* request = requests[0].second.get();
  EXPECT_FALSE(request->has_area());
  EXPECT_FALSE(request->has_result_selection());
}

}  // namespace
}  // namespace ui
