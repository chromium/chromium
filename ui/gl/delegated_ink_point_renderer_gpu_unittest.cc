// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/delegated_ink_point_renderer_gpu.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/win/hidden_window.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gl/dc_layer_tree.h"
#include "ui/gl/dcomp_presenter.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_test_helper.h"

namespace gl {
namespace {

// Standard number of microseconds that is put between each DelegatedInkPoint.
const int kMicrosecondsBetweenEachPoint = 10;

class DelegatedInkPointRendererGpuTest : public testing::Test {
 public:
  DelegatedInkPointRendererGpuTest() : parent_window_(ui::GetHiddenWindow()) {}

  DCompPresenter* presenter() { return presenter_.get(); }

  DCLayerTree* layer_tree() { return presenter()->GetLayerTreeForTesting(); }

  DCLayerTree::DelegatedInkRenderer* ink_renderer() {
    return layer_tree()->GetInkRendererForTesting();
  }

  void SendDelegatedInkPoint(const gfx::DelegatedInkPoint& point) {
    stored_points_[point.pointer_id()].push_back(point);
    ink_renderer()->StoreDelegatedInkPoint(point);
  }

  void SendDelegatedInkPointBasedOnPrevious(uint32_t pointer_id) {
    EXPECT_TRUE(stored_points_.find(pointer_id) != stored_points_.end());
    EXPECT_TRUE(!stored_points_[pointer_id].empty());

    auto last_point = stored_points_[pointer_id].back();
    SendDelegatedInkPoint(gfx::DelegatedInkPoint(
        last_point.point() + gfx::Vector2dF(5, 5),
        last_point.timestamp() +
            base::Microseconds(kMicrosecondsBetweenEachPoint),
        last_point.pointer_id()));
  }

  void SendDelegatedInkPointBasedOnPrevious() {
    EXPECT_EQ(stored_points_.size(), 1u);
    SendDelegatedInkPointBasedOnPrevious(stored_points_.begin()->first);
  }

  void SendMetadata(const gfx::DelegatedInkMetadata& metadata) {
    ink_renderer()->SetDelegatedInkTrailStartPoint(
        std::make_unique<gfx::DelegatedInkMetadata>(metadata));
  }

  // Sends a DelegatedInkMetadata that starts a new trail. this assumes that
  // there is only one `stored_points_` pointer id and that the points are
  // stored ordered by their timestamp.
  void SendNewTrailMetadata() {
    EXPECT_EQ(stored_points_.size(), 1u);
    auto points_it = stored_points_.find(stored_points_.begin()->first);
    EXPECT_TRUE(points_it != stored_points_.end());
    auto points_vec = points_it->second;
    EXPECT_GT(points_vec.size(), 0u);
    auto& last_point = points_vec.back();
    gfx::DelegatedInkMetadata metadata(
        last_point.point() + gfx::Vector2dF(5, 5), /*diameter=*/3,
        SK_ColorBLACK,
        last_point.timestamp() +
            base::Microseconds(kMicrosecondsBetweenEachPoint),
        gfx::RectF(0, 0, 100, 100), /*hovering=*/false);

    ink_renderer()->SetDelegatedInkTrailStartPoint(
        std::make_unique<gfx::DelegatedInkMetadata>(metadata));
  }

  gfx::DelegatedInkMetadata SendMetadataBasedOnStoredPoint(int32_t pointer_id,
                                                           uint64_t point) {
    EXPECT_TRUE(stored_points_.find(pointer_id) != stored_points_.end());
    EXPECT_GE(stored_points_[pointer_id].size(), point);

    const gfx::DelegatedInkPoint& ink_point = stored_points_[pointer_id][point];
    gfx::DelegatedInkMetadata metadata(
        ink_point.point(), /*diameter=*/3, SK_ColorBLACK, ink_point.timestamp(),
        gfx::RectF(0, 0, 100, 100), /*hovering=*/false);
    SendMetadata(metadata);
    return metadata;
  }

  gfx::DelegatedInkMetadata SendMetadataBasedOnStoredPoint(uint64_t point) {
    EXPECT_EQ(stored_points_.size(), 1u);
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
      GTEST_SKIP()
          << "GL implementation not using DirectComposition, skipping test.";
    }

    std::tie(gl_surface_, context_) =
        GLTestHelper::CreateOffscreenGLSurfaceAndContext();

    CreateDCompPresenter();

    if (!presenter_->SupportsDelegatedInk()) {
      GTEST_SKIP() << "Delegated ink unsupported, skipping test.";
    }

    // Create the swap chain
    constexpr gfx::Size window_size(100, 100);
    EXPECT_TRUE(presenter_->Resize(window_size, 1.0, gfx::ColorSpace(), true));

    ink_renderer()->InitializeForTesting(gl::GetDirectCompositionDevice());
  }

  void TearDown() override {
    context_.reset();
    gl_surface_.reset();
    if (presenter_) {
      DestroyPresenter(std::move(presenter_));
    }
    gl::init::ShutdownGL(display_, false);
  }

 private:
  void CreateDCompPresenter() {
    DCompPresenter::Settings settings;
    presenter_ = base::MakeRefCounted<DCompPresenter>(settings);

    // Add our child window to the root window.
    if (parent_window_)
      ::SetParent(presenter_->GetWindow(), parent_window_);
  }

  void DestroyPresenter(scoped_refptr<DCompPresenter> presenter) {
    scoped_refptr<base::TaskRunner> task_runner =
        presenter->GetWindowTaskRunnerForTesting();
    EXPECT_TRUE(presenter->HasOneRef());

    presenter = nullptr;

    base::RunLoop run_loop;
    task_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  HWND parent_window_;
  scoped_refptr<GLSurface> gl_surface_;
  scoped_refptr<DCompPresenter> presenter_;
  scoped_refptr<GLContext> context_;
  raw_ptr<GLDisplay> display_ = nullptr;

  // Used as a reference when making DelegatedInkMetadatas based on previously
  // sent points.
  std::map<int32_t, std::vector<gfx::DelegatedInkPoint>> stored_points_;
};

// Test to confirm that points and tokens are stored and removed correctly based
// on when the metadata and points arrive.
TEST_F(DelegatedInkPointRendererGpuTest, StoreAndRemovePointsAndTokens) {
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

// Verify that the `points_to_be_drawn_` is set correctly when points are
// added to the API's trail, and that the TimeToDrawPointsMillis histogram is
// reported correctly on draw.
TEST_F(DelegatedInkPointRendererGpuTest, ReportTimeToDraw) {
  const std::string kHistogramName =
      "Renderer.DelegatedInkTrail.OS.TimeToDrawPointsMillis";
  const base::HistogramTester histogram_tester;
  constexpr int32_t kPointerId = 1u;

  EXPECT_TRUE(ink_renderer()->PointstoBeDrawnForTesting().empty());
  ink_renderer()->ReportPointsDrawn();
  // No histogram should be fired if `points_to_be_drawn_` is empty.
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  const base::TimeTicks timestamp = base::TimeTicks::Now();
  SendDelegatedInkPoint(
      gfx::DelegatedInkPoint(gfx::PointF(20, 20), timestamp, kPointerId));
  SendMetadataBasedOnStoredPoint(0);

  // `DrawDelegatedInkPoint` should've added the point's timestamp to
  // `points_to_be_drawn_`.
  EXPECT_EQ(ink_renderer()->PointstoBeDrawnForTesting().size(), 1u);
  EXPECT_EQ(ink_renderer()->PointstoBeDrawnForTesting()[0].timestamp(),
            timestamp);

  // Send another point and expect that the new point's timestamp is added to
  // `points_to_be_drawn_`.
  SendDelegatedInkPointBasedOnPrevious(kPointerId);
  EXPECT_EQ(ink_renderer()->PointstoBeDrawnForTesting().size(), 2u);
  EXPECT_EQ(ink_renderer()->PointstoBeDrawnForTesting()[1].timestamp(),
            timestamp + base::Microseconds(kMicrosecondsBetweenEachPoint));

  ink_renderer()->ReportPointsDrawn();
  // Two histograms should've been fired with the delta between the point's
  // creation times and the function call, and `points_to_be_drawn_` should've
  // been cleared.
  histogram_tester.ExpectTotalCount(kHistogramName, 2);
  EXPECT_TRUE(ink_renderer()->PointstoBeDrawnForTesting().empty());
}
// Test that stale points get removed from `points_to_be_drawn_` when a newer
// metadata is added.
TEST_F(DelegatedInkPointRendererGpuTest,
       PointsToBeDrawnIsClearedWithNewMetadata) {
  constexpr int32_t kPointerId = 1u;
  const base::TimeTicks timestamp = base::TimeTicks::Now();
  SendDelegatedInkPoint(
      gfx::DelegatedInkPoint(gfx::PointF(20, 20), timestamp, kPointerId));
  SendMetadataBasedOnStoredPoint(0);
  EXPECT_EQ(ink_renderer()->PointstoBeDrawnForTesting().size(), 1u);
  // Test that sending a metadata with a timestamp larger than some points will
  // remove those points from the points to be drawn vector.
  SendDelegatedInkPointBasedOnPrevious();
  SendDelegatedInkPointBasedOnPrevious();
  SendDelegatedInkPointBasedOnPrevious();
  SendMetadataBasedOnStoredPoint(3);
  // The new metadata should cause all points but the last one to be deleted.
  EXPECT_EQ(ink_renderer()->PointstoBeDrawnForTesting().size(), 1u);

  // A metadata that starts a new trail should clear all the points in the
  // vector.
  SendNewTrailMetadata();
  EXPECT_EQ(ink_renderer()->PointstoBeDrawnForTesting().size(), 0u);
}

TEST_F(DelegatedInkPointRendererGpuTest, ReportLatencyImprovement) {
  const std::string kHistogramName =
      "Renderer.DelegatedInkTrail.LatencyImprovement.OS.WithoutPrediction";
  const base::HistogramTester histogram_tester;
  constexpr int32_t kPointerId = 1u;

  ink_renderer()->ReportPointsDrawn();
  // No histogram should be fired if `points_to_be_drawn_` is empty.
  EXPECT_TRUE(ink_renderer()->PointstoBeDrawnForTesting().empty());
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  // Create three points, `kMicrosecondsBetweenEachPoint` milliseconds apart
  // from each other (the histogram is measured in ms, microseconds would be
  // too small of a difference).
  gfx::DelegatedInkPoint point_1(gfx::PointF(20, 20), base::TimeTicks::Now(),
                                 kPointerId);
  gfx::DelegatedInkPoint point_2(
      point_1.point() + gfx::Vector2dF(5, 5),
      point_1.timestamp() + base::Milliseconds(kMicrosecondsBetweenEachPoint),
      kPointerId);
  gfx::DelegatedInkPoint point_3(
      point_2.point() + gfx::Vector2dF(5, 5),
      point_2.timestamp() + base::Milliseconds(kMicrosecondsBetweenEachPoint),
      kPointerId);
  SendDelegatedInkPoint(point_1);
  SendDelegatedInkPoint(point_2);
  SendDelegatedInkPoint(point_3);
  SendMetadataBasedOnStoredPoint(0);
  ink_renderer()->ReportPointsDrawn();

  // One histogram should've been fired with the delta between the metadata's
  // creation times and the `point_3`'s timestamp, in milliseconds.
  histogram_tester.ExpectUniqueSample(kHistogramName,
                                      kMicrosecondsBetweenEachPoint * 2, 1);
  EXPECT_TRUE(ink_renderer()->PointstoBeDrawnForTesting().empty());
}

TEST_F(DelegatedInkPointRendererGpuTest, ReportOutstandingPointsToDraw) {
  const std::string kHistogramName =
      "Renderer.DelegatedInkTrail.OS.OutstandingPointsToDraw";
  const base::HistogramTester histogram_tester;
  constexpr int32_t kPointerId = 1u;

  // No histogram should be fired when `metadata_paint_time_` is not set.
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  SendDelegatedInkPoint(gfx::DelegatedInkPoint(
      gfx::PointF(10, 10), base::TimeTicks::Now(), kPointerId));
  ink_renderer()->ReportPointsDrawn();
  SendMetadataBasedOnStoredPoint(0);
  ink_renderer()->ReportPointsDrawn();
  histogram_tester.ExpectUniqueSample(kHistogramName, 1, 1);
  SendDelegatedInkPointBasedOnPrevious();
  SendDelegatedInkPointBasedOnPrevious();
  ink_renderer()->ReportPointsDrawn();
  histogram_tester.ExpectBucketCount(kHistogramName, 2, 1);
  histogram_tester.ExpectBucketCount(kHistogramName, 1, 1);
  SendDelegatedInkPointBasedOnPrevious();
  SendDelegatedInkPointBasedOnPrevious();
  SendDelegatedInkPointBasedOnPrevious();
  ink_renderer()->ReportPointsDrawn();
  histogram_tester.ExpectBucketCount(kHistogramName, 3, 1);
  histogram_tester.ExpectBucketCount(kHistogramName, 2, 1);
  histogram_tester.ExpectBucketCount(kHistogramName, 1, 1);
}

// Test that the histogram `TimeFromDelegatedInkToApiPaint` is fired when a
// point is painted via the Delegated Ink API and then found to match a metadata
// point.
TEST_F(DelegatedInkPointRendererGpuTest, TestTimeFromDelegatedInkToApiPaint) {
  const std::string kHistogramName =
      "Renderer.DelegatedInkTrail.OS.TimeFromDelegatedInkToApiPaint";
  const base::HistogramTester histogram_tester;
  constexpr int32_t kPointerId = 1u;

  ink_renderer()->ReportPointsDrawn();
  // No histogram should be fired when `metadata_paint_time_` is not set.
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  SendDelegatedInkPoint(gfx::DelegatedInkPoint(
      gfx::PointF(10, 10), base::TimeTicks::Now(), kPointerId));
  // This metadata starts the trail and calls `DrawSavedTrailPoints`.
  SendMetadataBasedOnStoredPoint(0);
  // The `painted_time` timestamp should be set for the point with the
  // value of `base::TimeTicks::Now()`.
  ink_renderer()->ReportPointsDrawn();
  gfx::DelegatedInkPoint last_point =
      ink_renderer()->DelegatedInkPointsForTesting(kPointerId).rbegin()->first;
  EXPECT_TRUE(last_point.paint_timestamp().has_value());
  // `metadata_paint_time_` is not set yet, so the histogram should not have
  // been fired.
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  // Simulate receiving another point and painting it.
  SendDelegatedInkPointBasedOnPrevious();
  ink_renderer()->ReportPointsDrawn();
  // A new metadata is received that matches a Delegated Ink point with a
  // `painted_time` timestamp, so a histogram should be fired on next paint.
  SendMetadataBasedOnStoredPoint(1);
  ink_renderer()->ReportPointsDrawn();
  histogram_tester.ExpectTotalCount(kHistogramName, 1);

  // Adding a new point without updating the metadata should not fire a new
  // histogram.
  SendDelegatedInkPointBasedOnPrevious();
  ink_renderer()->ReportPointsDrawn();
  histogram_tester.ExpectTotalCount(kHistogramName, 1);

  // Send the metadata that matches the previous point and verify that a
  // histogram was fired.
  SendMetadataBasedOnStoredPoint(2);
  ink_renderer()->ReportPointsDrawn();
  histogram_tester.ExpectTotalCount(kHistogramName, 2);

  // Add a new point, then send a metadata that does not match it. Then verify
  // that a histogram wasn't fired.
  SendDelegatedInkPointBasedOnPrevious();
  ink_renderer()->ReportPointsDrawn();
  last_point =
      ink_renderer()->DelegatedInkPointsForTesting(kPointerId).rbegin()->first;
  SendMetadata(gfx::DelegatedInkMetadata(
      last_point.point() + gfx::Vector2dF(2, 2), /*diameter=*/3, SK_ColorBLACK,
      last_point.timestamp() + base::Microseconds(20),
      gfx::RectF(0, 0, 100, 100), /*hovering=*/false));
  ink_renderer()->ReportPointsDrawn();
  histogram_tester.ExpectTotalCount(kHistogramName, 2);
}

}  // namespace
}  // namespace gl
