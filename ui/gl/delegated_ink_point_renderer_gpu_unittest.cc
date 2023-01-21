// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/delegated_ink_point_renderer_gpu.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/win/hidden_window.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gl/dc_layer_tree.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/init/gl_factory.h"

namespace gl {
namespace {

// Standard number of microseconds that is put between each DelegatedInkPoint.
const int kMicrosecondsBetweenEachPoint = 10;

class DelegatedInkPointRendererGpuTest : public testing::Test {
 public:
  DelegatedInkPointRendererGpuTest() : parent_window_(ui::GetHiddenWindow()) {}

  DirectCompositionSurfaceWin* surface() { return surface_.get(); }

  DCLayerTree* layer_tree() { return surface()->GetLayerTreeForTesting(); }

  DCLayerTree::DelegatedInkRenderer* ink_renderer() {
    return layer_tree()->GetInkRendererForTesting();
  }

  void SendDelegatedInkPoint(const gfx::DelegatedInkPoint& point) {
    stored_points_[point.pointer_id()].push_back(point);
    ink_renderer()->StoreDelegatedInkPoint(point);
  }

  void SendDelegatedInkPointBasedOnPrevious(uint32_t pointer_id) {
    DCHECK(stored_points_.find(pointer_id) != stored_points_.end());
    DCHECK(!stored_points_[pointer_id].empty());

    auto last_point = stored_points_[pointer_id].back();
    SendDelegatedInkPoint(gfx::DelegatedInkPoint(
        last_point.point() + gfx::Vector2dF(5, 5),
        last_point.timestamp() +
            base::Microseconds(kMicrosecondsBetweenEachPoint),
        last_point.pointer_id()));
  }

  void SendDelegatedInkPointBasedOnPrevious() {
    DCHECK_EQ(stored_points_.size(), 1u);
    SendDelegatedInkPointBasedOnPrevious(stored_points_.begin()->first);
  }

  void SendMetadata(const gfx::DelegatedInkMetadata& metadata) {
    surface()->SetDelegatedInkTrailStartPoint(
        std::make_unique<gfx::DelegatedInkMetadata>(metadata));
  }

  gfx::DelegatedInkMetadata SendMetadataBasedOnStoredPoint(int32_t pointer_id,
                                                           uint64_t point) {
    DCHECK(stored_points_.find(pointer_id) != stored_points_.end());
    DCHECK_GE(stored_points_[pointer_id].size(), point);

    const gfx::DelegatedInkPoint& ink_point = stored_points_[pointer_id][point];
    gfx::DelegatedInkMetadata metadata(
        ink_point.point(), /*diameter=*/3, SK_ColorBLACK, ink_point.timestamp(),
        gfx::RectF(0, 0, 100, 100), /*hovering=*/false);
    SendMetadata(metadata);
    return metadata;
  }

  gfx::DelegatedInkMetadata SendMetadataBasedOnStoredPoint(uint64_t point) {
    DCHECK_EQ(stored_points_.size(), 1u);
    return SendMetadataBasedOnStoredPoint(stored_points_.begin()->first, point);
  }

  void StoredMetadataMatchesSentMetadata(
      const gfx::DelegatedInkMetadata& sent_metadata) {
    gfx::DelegatedInkMetadata* renderer_metadata =
        ink_renderer()->MetadataForTesting();
    EXPECT_TRUE(renderer_metadata);
    EXPECT_EQ(renderer_metadata->point(), sent_metadata.point());
    EXPECT_EQ(renderer_metadata->diameter(), sent_metadata.diameter());
    EXPECT_EQ(renderer_metadata->color(), sent_metadata.color());
    EXPECT_EQ(renderer_metadata->timestamp(), sent_metadata.timestamp());
    EXPECT_EQ(renderer_metadata->presentation_area(),
              sent_metadata.presentation_area());
    EXPECT_EQ(renderer_metadata->is_hovering(), sent_metadata.is_hovering());
  }

 protected:
  void SetUp() override {
    // Without this, the following check always fails.
    display_ = gl::init::InitializeGLNoExtensionsOneOff(
        /*init_bindings=*/true,
        /*gpu_preference=*/gl::GpuPreference::kDefault);
    if (!gl::DirectCompositionSupported()) {
      LOG(WARNING)
          << "GL implementation not using DirectComposition, skipping test.";
      return;
    }

    CreateDirectCompositionSurfaceWin();
    if (!surface_->SupportsDelegatedInk()) {
      LOG(WARNING) << "Delegated ink unsupported, skipping test.";
      return;
    }

    CreateGLContext();
    surface_->SetEnableDCLayers(true);

    // Create the swap chain
    constexpr gfx::Size window_size(100, 100);
    EXPECT_TRUE(surface_->Resize(window_size, 1.0, gfx::ColorSpace(), true));
    EXPECT_TRUE(surface_->SetDrawRectangle(gfx::Rect(window_size)));
  }

  void TearDown() override {
    context_ = nullptr;
    if (surface_)
      DestroySurface(std::move(surface_));
    gl::init::ShutdownGL(display_, false);
  }

 private:
  void CreateDirectCompositionSurfaceWin() {
    DirectCompositionSurfaceWin::Settings settings;
    surface_ = base::MakeRefCounted<DirectCompositionSurfaceWin>(
        gl::GLSurfaceEGL::GetGLDisplayEGL(),
        DirectCompositionSurfaceWin::VSyncCallback(), settings);
    EXPECT_TRUE(surface_->Initialize(GLSurfaceFormat()));

    // ImageTransportSurfaceDelegate::AddChildWindowToBrowser() is called in
    // production code here. However, to remove dependency from
    // gpu/ipc/service/image_transport_surface_delegate.h, here we directly
    // executes the required minimum code.
    if (parent_window_)
      ::SetParent(surface_->window(), parent_window_);
  }

  void CreateGLContext() {
    context_ =
        gl::init::CreateGLContext(nullptr, surface_.get(), GLContextAttribs());
    EXPECT_TRUE(context_->MakeCurrent(surface_.get()));
  }

  void DestroySurface(scoped_refptr<DirectCompositionSurfaceWin> surface) {
    scoped_refptr<base::TaskRunner> task_runner =
        surface->GetWindowTaskRunnerForTesting();
    DCHECK(surface->HasOneRef());

    surface = nullptr;

    base::RunLoop run_loop;
    task_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  HWND parent_window_;
  scoped_refptr<DirectCompositionSurfaceWin> surface_;
  scoped_refptr<GLContext> context_;
  raw_ptr<GLDisplay> display_ = nullptr;

  // Used as a reference when making DelegatedInkMetadatas based on previously
  // sent points.
  std::map<int32_t, std::vector<gfx::DelegatedInkPoint>> stored_points_;
};

// Test to confirm that points and tokens are stored and removed correctly based
// on when the metadata and points arrive.
TEST_F(DelegatedInkPointRendererGpuTest, StoreAndRemovePointsAndTokens) {
  if (!surface() || !surface()->SupportsDelegatedInk())
    return;

  // Send some points and make sure they are all stored even with no metadata.
  const int32_t kPointerId = 1;
  SendDelegatedInkPoint(gfx::DelegatedInkPoint(
      gfx::PointF(10, 10), base::TimeTicks::Now(), kPointerId));
  const uint64_t kPointsToStore = 5u;
  for (uint64_t i = 1; i < kPointsToStore; ++i)
    SendDelegatedInkPointBasedOnPrevious();

  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId).size(),
            kPointsToStore);
  EXPECT_EQ(ink_renderer()->InkTrailTokenCountForTesting(), 0u);
  EXPECT_FALSE(ink_renderer()->MetadataForTesting());
  EXPECT_TRUE(ink_renderer()->WaitForNewTrailToDrawForTesting());

  // Now send metadata that matches the first stored point. This should result
  // in all of the points being drawn and matching tokens stored. None of the
  // points should be removed from the circular deque because they are stored
  // until a metadata arrives with a later timestamp
  gfx::DelegatedInkMetadata metadata = SendMetadataBasedOnStoredPoint(0);

  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId).size(),
            kPointsToStore);
  EXPECT_EQ(ink_renderer()->InkTrailTokenCountForTesting(), kPointsToStore);
  EXPECT_FALSE(ink_renderer()->WaitForNewTrailToDrawForTesting());
  StoredMetadataMatchesSentMetadata(metadata);

  // Now send a metadata that matches a later one of the points. It should
  // result in all of the points up to and including the point that matches
  // the metadata being erased. Then, all remaining points should be drawn and
  // should therefore have tokens associated with them.
  const uint64_t kPointToSend = 3u;
  metadata = SendMetadataBasedOnStoredPoint(kPointToSend);
  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId).size(),
            kPointsToStore - kPointToSend - 1);
  EXPECT_EQ(ink_renderer()->InkTrailTokenCountForTesting(),
            kPointsToStore - kPointToSend - 1);
  StoredMetadataMatchesSentMetadata(metadata);

  // Now send a metadata after all of the stored point to make sure that it
  // results in all the tokens and stored points being erased because a new
  // trail is started.
  gfx::DelegatedInkPoint last_point =
      ink_renderer()->DelegatedInkPointsForTesting(kPointerId).rbegin()->first;
  metadata = gfx::DelegatedInkMetadata(
      last_point.point() + gfx::Vector2dF(2, 2), /*diameter=*/3, SK_ColorBLACK,
      last_point.timestamp() + base::Microseconds(20),
      gfx::RectF(0, 0, 100, 100), /*hovering=*/false);
  SendMetadata(metadata);
  EXPECT_EQ(ink_renderer()->DelegatedInkPointPointerIdCountForTesting(), 0u);
  StoredMetadataMatchesSentMetadata(metadata);
}

// Basic test to confirm that points are drawn as they arrive if they are in the
// presentation area and after the metadata's timestamp.
TEST_F(DelegatedInkPointRendererGpuTest, DrawPointsAsTheyArrive) {
  if (!surface() || !surface()->SupportsDelegatedInk())
    return;

  gfx::DelegatedInkMetadata metadata(
      gfx::PointF(12, 12), /*diameter=*/3, SK_ColorBLACK,
      base::TimeTicks::Now(), gfx::RectF(10, 10, 90, 90), /*hovering=*/false);
  SendMetadata(metadata);

  // Send some points that should all be drawn to ensure that they are all drawn
  // as they arrive.
  const int32_t kPointerId = 1;
  const uint64_t kPointsToSend = 5u;
  for (uint64_t i = 1u; i <= kPointsToSend; ++i) {
    if (i == 1) {
      SendDelegatedInkPoint(gfx::DelegatedInkPoint(
          metadata.point(), metadata.timestamp(), kPointerId));
    } else {
      SendDelegatedInkPointBasedOnPrevious();
    }
    EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId).size(),
              i);
    EXPECT_EQ(ink_renderer()->InkTrailTokenCountForTesting(), i);
  }

  // Now send a point that is outside of the presentation area to ensure that
  // it is not drawn. It will still be stored - this is so if a future metadata
  // arrives with a presentation area that would contain this point, it can
  // still be drawn.
  gfx::DelegatedInkPoint last_point =
      ink_renderer()->DelegatedInkPointsForTesting(kPointerId).rbegin()->first;
  gfx::DelegatedInkPoint outside_point(
      gfx::PointF(5, 5), last_point.timestamp() + base::Microseconds(10),
      /*pointer_id=*/1);
  EXPECT_FALSE(metadata.presentation_area().Contains(outside_point.point()));
  SendDelegatedInkPoint(outside_point);

  const uint64_t kTotalPointsSent = kPointsToSend + 1u;
  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId).size(),
            kTotalPointsSent);
  EXPECT_EQ(ink_renderer()->InkTrailTokenCountForTesting(), kPointsToSend);

  // Then send a metadata with a larger presentation area and timestamp earlier
  // than the above point to confirm it will be the only point drawn, but all
  // the points with later timestamps will be stored.
  const uint64_t kMetadataToSend = 3u;
  SendMetadataBasedOnStoredPoint(kMetadataToSend);
  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId).size(),
            kTotalPointsSent - kMetadataToSend - 1);
  EXPECT_EQ(ink_renderer()->InkTrailTokenCountForTesting(), 1u);
}

// Confirm that points with different pointer ids are handled correctly.
TEST_F(DelegatedInkPointRendererGpuTest, MultiplePointerIds) {
  if (!surface() || !surface()->SupportsDelegatedInk())
    return;

  const int32_t kPointerId1 = 1;
  const int32_t kPointerId2 = 2;

  // First send a few points with different pointer ids to make sure they are
  // stored separately and correctly. Separate the timestamps so that we can
  // test how sending metadatas impacts them each separately.
  const base::TimeTicks timestamp = base::TimeTicks::Now();
  const int kPointerId2PointsAheadOfPointerId1 = 3;
  SendDelegatedInkPoint(
      gfx::DelegatedInkPoint(gfx::PointF(16, 22), timestamp, kPointerId1));
  SendDelegatedInkPoint(gfx::DelegatedInkPoint(
      gfx::PointF(40, 13.3f),
      timestamp + base::Microseconds(kPointerId2PointsAheadOfPointerId1 *
                                     kMicrosecondsBetweenEachPoint),
      kPointerId2));

  uint64_t pointer_id_1_count = 1u;
  uint64_t pointer_id_2_count = 1u;
  const uint64_t kTotalPoints = 9u;
  for (uint64_t i = 0u; i < kTotalPoints; ++i) {
    if (i % 2 == 0) {
      SendDelegatedInkPointBasedOnPrevious(kPointerId1);
      pointer_id_1_count++;
    } else {
      SendDelegatedInkPointBasedOnPrevious(kPointerId2);
      pointer_id_2_count++;
    }
  }

  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId1).size(),
            pointer_id_1_count);
  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId2).size(),
            pointer_id_2_count);

  // Send a metadata matching one of the points with |kPointerId1|. This should
  // erase all points with a timestamp before that from the trail with
  // |kPointerId1| but none from the other pointer id.
  const uint64_t kMetadataToSendForPointerId1 = 1u;
  SendMetadataBasedOnStoredPoint(kPointerId1, kMetadataToSendForPointerId1);
  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId1).size(),
            pointer_id_1_count - kMetadataToSendForPointerId1);
  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId2).size(),
            pointer_id_2_count);

  // Now send a metadata matching one of the points with |kPointerId2|. Since
  // |kPointerId2| points have timestamps greater than |kPointerId1|, it should
  // cause some points with |kPointerId1| to be erased too.
  const uint64_t kMetadataToSendForPointerId2 = 2u;
  SendMetadataBasedOnStoredPoint(kPointerId2, kMetadataToSendForPointerId2);
  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId1).size(),
            pointer_id_1_count - kPointerId2PointsAheadOfPointerId1 -
                kMetadataToSendForPointerId2);
  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId2).size(),
            pointer_id_2_count - kMetadataToSendForPointerId2);

  // Add another pointer id into the mix and make sure the counts are all what
  // we expect.
  const int32_t kPointerId3 = 3;
  const int kPointerId3PointsAheadOfPointerId1 = 5;
  SendDelegatedInkPoint(gfx::DelegatedInkPoint(
      gfx::PointF(23, 64),
      timestamp + base::Microseconds(kPointerId3PointsAheadOfPointerId1 *
                                     kMicrosecondsBetweenEachPoint),
      kPointerId3));

  const uint64_t kPointsWithPointerId3 = 4u;
  for (uint64_t i = 1u; i < kPointsWithPointerId3; ++i)
    SendDelegatedInkPointBasedOnPrevious(kPointerId3);

  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId1).size(),
            pointer_id_1_count - kPointerId2PointsAheadOfPointerId1 -
                kMetadataToSendForPointerId2);
  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId2).size(),
            pointer_id_2_count - kMetadataToSendForPointerId2);
  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId3).size(),
            kPointsWithPointerId3);

  // Then send a point with |kPointerId3| that is beyond all the points with
  // |kPointerId1| to make sure that |kPointerId1| is removed from the flat map
  // completely.
  const uint64_t kMetadataToSendForPointerId3 = 1u;
  SendMetadataBasedOnStoredPoint(kPointerId3, kMetadataToSendForPointerId3);
  EXPECT_EQ(ink_renderer()->DelegatedInkPointPointerIdCountForTesting(), 2u);
  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId2).size(),
            pointer_id_2_count - kMetadataToSendForPointerId2 -
                kMetadataToSendForPointerId3);
  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting(kPointerId3).size(),
            kPointsWithPointerId3 - kMetadataToSendForPointerId3);
}

// Make sure that the DelegatedInkPoint with the earliest timestamp is removed
// if we have reached the maximum number of pointer ids.
TEST_F(DelegatedInkPointRendererGpuTest, MaximumPointerIds) {
  if (!surface() || !surface()->SupportsDelegatedInk())
    return;

  // First add DelegatedInkPoints with unique pointer ids up to the limit and
  // make sure they are all correctly added separately.
  const base::TimeTicks kEarliestTimestamp =
      base::TimeTicks::Now() -
      base::Microseconds(kMicrosecondsBetweenEachPoint);
  const int32_t kEarliestTimestampPointerId = 4;

  base::TimeTicks timestamp = base::TimeTicks::Now();
  gfx::PointF point(34.4f, 20);
  const uint64_t kMaxNumberOfPointerIds =
      ink_renderer()->GetMaximumNumberOfPointerIdsForTesting();
  for (uint64_t pointer_id = 0; pointer_id < kMaxNumberOfPointerIds;
       ++pointer_id) {
    SendDelegatedInkPoint(gfx::DelegatedInkPoint(
        point,
        pointer_id == kEarliestTimestampPointerId ? kEarliestTimestamp
                                                  : timestamp,
        pointer_id));
    point += gfx::Vector2dF(5, 5);
    timestamp += base::Microseconds(kMicrosecondsBetweenEachPoint);
  }

  EXPECT_EQ(ink_renderer()->DelegatedInkPointPointerIdCountForTesting(),
            kMaxNumberOfPointerIds);
  EXPECT_TRUE(
      ink_renderer()->CheckForPointerIdForTesting(kEarliestTimestampPointerId));

  // Now send one more with a later timestamp to ensure that the one with the
  // earliest timestamp was removed.
  SendDelegatedInkPoint(
      gfx::DelegatedInkPoint(point, timestamp, kMaxNumberOfPointerIds));

  EXPECT_EQ(ink_renderer()->DelegatedInkPointPointerIdCountForTesting(),
            kMaxNumberOfPointerIds);
  EXPECT_FALSE(
      ink_renderer()->CheckForPointerIdForTesting(kEarliestTimestampPointerId));

  // Finally, add a point with a earlier timestamp than everything else and make
  // sure that it is not added to the map of pointer ids.
  point += gfx::Vector2dF(5, 5);
  timestamp = kEarliestTimestamp + base::Microseconds(5);
  const int32_t kEarlyTimestampPointerId = kMaxNumberOfPointerIds + 1;
  SendDelegatedInkPoint(
      gfx::DelegatedInkPoint(point, timestamp, kEarlyTimestampPointerId));

  EXPECT_EQ(ink_renderer()->DelegatedInkPointPointerIdCountForTesting(),
            kMaxNumberOfPointerIds);
  EXPECT_FALSE(
      ink_renderer()->CheckForPointerIdForTesting(kEarlyTimestampPointerId));
}

}  // namespace
}  // namespace gl
