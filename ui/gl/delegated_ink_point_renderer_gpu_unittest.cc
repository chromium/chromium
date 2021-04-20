// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/delegated_ink_point_renderer_gpu.h"

#include <memory>

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/win/hidden_window.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gl/dc_layer_tree.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/init/gl_factory.h"

namespace gl {
namespace {

class DelegatedInkPointRendererGpuTest : public testing::Test {
 public:
  DelegatedInkPointRendererGpuTest() : parent_window_(ui::GetHiddenWindow()) {}

  DirectCompositionSurfaceWin* surface() { return surface_.get(); }

  DCLayerTree* layer_tree() { return surface()->GetLayerTreeForTesting(); }

  DCLayerTree::DelegatedInkRenderer* ink_renderer() {
    return layer_tree()->GetInkRendererForTesting();
  }

  void SendDelegatedInkPointBasedOnPrevious() {
    const base::circular_deque<gfx::DelegatedInkPoint>& ink_points =
        ink_renderer()->DelegatedInkPointsForTesting();
    DCHECK(!ink_points.empty());

    auto last_point = ink_points.back();
    ink_renderer()->StoreDelegatedInkPoint(gfx::DelegatedInkPoint(
        last_point.point() + gfx::Vector2dF(5, 5),
        last_point.timestamp() + base::TimeDelta::FromMicroseconds(10),
        last_point.pointer_id()));
  }

  void SendMetadata(const gfx::DelegatedInkMetadata& metadata) {
    surface()->SetDelegatedInkTrailStartPoint(
        std::make_unique<gfx::DelegatedInkMetadata>(metadata));
  }

  gfx::DelegatedInkMetadata SendMetadataBasedOnStoredPoint(uint64_t point) {
    const base::circular_deque<gfx::DelegatedInkPoint>& ink_points =
        ink_renderer()->DelegatedInkPointsForTesting();
    EXPECT_GE(ink_points.size(), point);

    auto ink_point = ink_points[point];
    gfx::DelegatedInkMetadata metadata(
        ink_point.point(), /*diameter=*/3, SK_ColorBLACK, ink_point.timestamp(),
        gfx::RectF(0, 0, 100, 100), /*hovering=*/false);
    SendMetadata(metadata);
    return metadata;
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
    gl::init::InitializeGLNoExtensionsOneOff(/*init_bindings=*/true);
    if (!QueryDirectCompositionDevice(QueryD3D11DeviceObjectFromANGLE())) {
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
    gl::init::ShutdownGL(false);
  }

 private:
  void CreateDirectCompositionSurfaceWin() {
    DirectCompositionSurfaceWin::Settings settings;
    surface_ = base::MakeRefCounted<DirectCompositionSurfaceWin>(
        parent_window_, DirectCompositionSurfaceWin::VSyncCallback(), settings);
    EXPECT_TRUE(surface_->Initialize(GLSurfaceFormat()));

    // ImageTransportSurfaceDelegate::DidCreateAcceleratedSurfaceChildWindow()
    // is called in production code here. However, to remove dependency from
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
};

// Test to confirm that points and tokens are stored and removed correctly based
// on when the metadata and points arrive.
TEST_F(DelegatedInkPointRendererGpuTest, StoreAndRemovePointsAndTokens) {
  if (!surface() || !surface()->SupportsDelegatedInk())
    return;

  // Send some points and make sure they are all stored even with no metadata.
  ink_renderer()->StoreDelegatedInkPoint(
      gfx::DelegatedInkPoint(gfx::PointF(10, 10), base::TimeTicks::Now(), 1));
  const uint64_t kPointsToStore = 5u;
  for (uint64_t i = 1; i < kPointsToStore; ++i)
    SendDelegatedInkPointBasedOnPrevious();

  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting().size(),
            kPointsToStore);
  EXPECT_TRUE(ink_renderer()->InkTrailTokensForTesting().empty());
  EXPECT_FALSE(ink_renderer()->MetadataForTesting());
  EXPECT_TRUE(ink_renderer()->WaitForNewTrailToDrawForTesting());

  // Now send metadata that matches the first stored point. This should result
  // in all of the points being drawn and matching tokens stored. None of the
  // points should be removed from the circular deque because they are stored
  // until a metadata arrives with a later timestamp
  gfx::DelegatedInkMetadata metadata = SendMetadataBasedOnStoredPoint(0);

  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting().size(),
            kPointsToStore);
  EXPECT_EQ(ink_renderer()->InkTrailTokensForTesting().size(), kPointsToStore);
  EXPECT_FALSE(ink_renderer()->WaitForNewTrailToDrawForTesting());
  StoredMetadataMatchesSentMetadata(metadata);

  // Now send a metadata that matches a later one of the points. It should
  // result in some of the stored points being erased, and one more token erased
  // than points erased. This is because we don't need to store the token of the
  // point that exactly matches the metadata.
  const uint64_t kPointToSend = 3u;
  metadata = SendMetadataBasedOnStoredPoint(kPointToSend);
  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting().size(),
            kPointsToStore - kPointToSend);
  // Subtract one extra because the token for the point that matches the new
  // metadata is erased too.
  EXPECT_EQ(ink_renderer()->InkTrailTokensForTesting().size(),
            kPointsToStore - kPointToSend - 1);
  StoredMetadataMatchesSentMetadata(metadata);

  // Now send a metadata after all of the stored point to make sure that it
  // results in all the tokens and stored points being erased because a new
  // trail is started.
  gfx::DelegatedInkPoint last_point =
      ink_renderer()->DelegatedInkPointsForTesting().back();
  metadata = gfx::DelegatedInkMetadata(
      last_point.point() + gfx::Vector2dF(2, 2), /*diameter=*/3, SK_ColorBLACK,
      last_point.timestamp() + base::TimeDelta::FromMicroseconds(20),
      gfx::RectF(0, 0, 100, 100), /*hovering=*/false);
  SendMetadata(metadata);
  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting().size(), 0u);
  EXPECT_EQ(ink_renderer()->InkTrailTokensForTesting().size(), 0u);
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
  const uint64_t kPointsToSend = 5u;
  for (uint64_t i = 1u; i <= kPointsToSend; ++i) {
    if (i == 1) {
      ink_renderer()->StoreDelegatedInkPoint(gfx::DelegatedInkPoint(
          metadata.point(), metadata.timestamp(), /*pointer_id=*/1));
    } else {
      SendDelegatedInkPointBasedOnPrevious();
    }
    EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting().size(), i);
    EXPECT_EQ(ink_renderer()->InkTrailTokensForTesting().size(), i);
  }

  // Now send a point that is outside of the presentation area to ensure that
  // it is not drawn. It will still be stored - this is so if a future metadata
  // arrives with a presentation area that would contain this point, it can
  // still be drawn.
  gfx::DelegatedInkPoint last_point =
      ink_renderer()->DelegatedInkPointsForTesting().back();
  gfx::DelegatedInkPoint outside_point(
      gfx::PointF(5, 5),
      last_point.timestamp() + base::TimeDelta::FromMicroseconds(10),
      /*pointer_id=*/1);
  EXPECT_FALSE(metadata.presentation_area().Contains((outside_point.point())));
  ink_renderer()->StoreDelegatedInkPoint(outside_point);

  const uint64_t kTotalPointsSent = kPointsToSend + 1u;
  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting().size(),
            kTotalPointsSent);
  EXPECT_EQ(ink_renderer()->InkTrailTokensForTesting().size(), kPointsToSend);

  // Then send a metadata with a larger presentation area and timestamp earlier
  // than the above point to confirm it will be the only point drawn, but all
  // the points with later timestamps will be stored.
  const uint64_t kMetadataToSend = 3u;
  SendMetadataBasedOnStoredPoint(kMetadataToSend);
  EXPECT_EQ(ink_renderer()->DelegatedInkPointsForTesting().size(),
            kTotalPointsSent - kMetadataToSend);
  EXPECT_EQ(ink_renderer()->InkTrailTokensForTesting().size(), 1u);
}

}  // namespace
}  // namespace gl
