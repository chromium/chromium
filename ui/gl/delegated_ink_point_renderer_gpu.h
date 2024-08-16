// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DELEGATED_INK_POINT_RENDERER_GPU_H_
#define UI_GL_DELEGATED_INK_POINT_RENDERER_GPU_H_

#include <dcomp.h>
#include <wrl/client.h>

#include <optional>
#include <vector>

#include "base/check_is_test.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/gfx/delegated_ink_metadata.h"
#include "ui/gfx/delegated_ink_point.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/mojom/delegated_ink_point_renderer.mojom.h"
#include "ui/gl/dc_layer_overlay_params.h"
#include "ui/gl/gl_export.h"

namespace gl {

namespace {
struct DelegatedInkPointCompare {
  bool operator()(const gfx::DelegatedInkPoint& lhs,
                  const gfx::DelegatedInkPoint& rhs) const {
    return lhs.timestamp() < rhs.timestamp();
  }
};
}  // namespace

// On construction, this class will create a new visual for the visual tree with
// an IDCompositionDelegatedInk object as the contents. This will be added as
// a child of the root surface visual in the tree, and the trail will be drawn
// to it. It is a child of the root surface visual because this visual contains
// the swapchain, and there will be no transforms applied to the delegated ink
// visual this way.
// For more information about the design of this class and using the OS APIs,
// view the design doc here: https://aka.ms/GPUBackedDesignDoc
class GL_EXPORT DelegatedInkPointRendererGpu
    : public gfx::mojom::DelegatedInkPointRenderer {
 public:
  DelegatedInkPointRendererGpu();
  ~DelegatedInkPointRendererGpu() override;

  using DelegatedInkPointTokenMap = base::flat_map<gfx::DelegatedInkPoint,
                                                   std::optional<unsigned int>,
                                                   DelegatedInkPointCompare>;

  void InitMessagePipeline(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
          pending_receiver);

  bool DelegatedInkIsSupported(
      const Microsoft::WRL::ComPtr<IDCompositionDevice2>& dcomp_device) const;

  void SetDelegatedInkTrailStartPoint(
      std::unique_ptr<gfx::DelegatedInkMetadata> metadata);

  void StoreDelegatedInkPoint(const gfx::DelegatedInkPoint& point) override;

  void ResetPrediction() override;

  gfx::DelegatedInkMetadata* MetadataForTesting() const {
    CHECK_IS_TEST();
    return metadata_.get();
  }

  // Creates an overlay for the current frame's Delegated Ink Metadata.
  // If the Delegated Ink Renderer can not be initialized, it returns a
  // nullptr. The ink trail will synchronize its updates with |root_swap_chain|
  // if present, otherwise it will synchronize with DComp commit.
  std::unique_ptr<DCLayerOverlayParams> MakeDelegatedInkOverlay(
      IDCompositionDevice2* dcomp_device2,
      IDXGISwapChain1* root_swap_chain,
      std::unique_ptr<gfx::DelegatedInkMetadata> metadata);

  uint64_t InkTrailTokenCountForTesting() const;

  uint64_t DelegatedInkPointPointerIdCountForTesting() const {
    CHECK_IS_TEST();
    return delegated_ink_points_.size();
  }

  bool CheckForPointerIdForTesting(int32_t pointer_id) const;

  const DelegatedInkPointTokenMap& DelegatedInkPointsForTesting(
      int32_t pointer_id) {
    CHECK_IS_TEST();
    DCHECK(delegated_ink_points_.find(pointer_id) !=
           delegated_ink_points_.end());
    return delegated_ink_points_[pointer_id];
  }

  bool WaitForNewTrailToDrawForTesting() const {
    CHECK_IS_TEST();
    return wait_for_new_trail_to_draw_;
  }

  std::vector<gfx::DelegatedInkPoint> PointstoBeDrawnForTesting() const {
    CHECK_IS_TEST();
    return points_to_be_drawn_;
  }

  uint64_t GetMaximumNumberOfPointerIdsForTesting() const;

  void InitializeForTesting(IDCompositionDevice2* dcomp_device2);

  // This function is called after Delegated Ink's points are submitted to be
  // drawn on screen, and fires a histogram with the time between points' event
  // creation and the points' draw submission to the OS.
  void ReportPointsDrawn();

 private:
  bool Initialize(IDCompositionDevice2* dcomp_device2,
                  IDXGISwapChain1* root_swap_chain);

  void EraseExcessPointerIds();

  std::optional<int32_t> GetPointerIdForMetadata();

  void DrawSavedTrailPoints();

  bool DrawDelegatedInkPoint(const gfx::DelegatedInkPoint& point);

  // The delegated ink trail object that the ink trail is drawn on. This is the
  // content of the ink visual.
  Microsoft::WRL::ComPtr<IDCompositionDelegatedInkTrail> delegated_ink_trail_;

  // The swap chain whose updates |delegated_ink_trail_| is synchronized with.
  // If null, |delegated_ink_trail_| is synchronized with DComp commit.
  raw_ptr<IDXGISwapChain1> swap_chain_ = nullptr;

  Microsoft::WRL::ComPtr<IDCompositionInkTrailDevice> ink_trail_device_;

  // The most recent metadata received. The metadata marks the last point of
  // the app rendered stroke, which corresponds to the first point of the
  // delegated ink trail that will be drawn.
  std::unique_ptr<gfx::DelegatedInkMetadata> metadata_;

  // A base::flat_map of all the points that have arrived in
  // StoreDelegatedInkPoint() with a timestamp greater than or equal to that of
  // |metadata_|, where the key is the DelegatedInkPoint that was received, and
  // the value is an optional token. The value will be null until the
  // DelegatedInkPoint is added to the trail, at which point the value is the
  // token that was returned by AddTrailPoints. The elements of the flat_map are
  // sorted by the timestamp of the DelegatedInkPoint. All points are stored in
  // here until we receive a |metadata_|, then any DelegatedInkPoints that have
  // a timestamp earlier than |metadata_|'s are removed, and any new
  // DelegatedInkPoints that may arrive with an earlier timestamp are ignored.
  // Then as each new |metadata| arrives in SetDelegatedInkTrailStartPoint(),
  // we remove any old elements with earlier timestamps, up to and including the
  // element that matches the DelegatedInkMetadata.
  base::flat_map<int32_t, DelegatedInkPointTokenMap> delegated_ink_points_;

  // Cached pointer id of the most recently drawn trail.
  std::optional<int32_t> pointer_id_;

  // Flag to know if new DelegatedInkPoints that arrive should be drawn
  // immediately or if they should wait for a new trail to be started. Set to
  // true when ResetPrediction is called, as this tells us that something about
  // the pointerevents in the browser process changed. When that happens, we
  // should continue to remove points for any metadata that arrive with
  // timestamps that match DelegatedInkPoints that arrived before the
  // ResetPrediction call. However, once a metadata arrives with a timestamp
  // after the ResetPrediction, then we know that inking should continue, so a
  // new trail is started and we try to draw everything in
  // |delegated_ink_points_|.
  bool wait_for_new_trail_to_draw_ = true;

  // Set to true when DelegatedInkPointRendererGPU has been (re)initialized.
  // This can happen due to the first metadata being received, or change in
  // root surface swap chain or DirectCompositionDevice.
  bool force_new_ink_trail_ = false;

  mojo::Receiver<gfx::mojom::DelegatedInkPointRenderer> receiver_{this};

  // Holds the timestamp of points added to the Delegated Ink's Trail to measure
  // the time between point creation and draw operation called.
  std::vector<gfx::DelegatedInkPoint> points_to_be_drawn_;

  // The timestamp in which a Delegated Ink point that matches the metadata was
  // first painted.
  std::optional<base::TimeTicks> metadata_paint_time_;
};

}  // namespace gl

#endif  // UI_GL_DELEGATED_INK_POINT_RENDERER_GPU_H_
