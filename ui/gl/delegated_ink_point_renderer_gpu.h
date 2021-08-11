// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DELEGATED_INK_POINT_RENDERER_GPU_H_
#define UI_GL_DELEGATED_INK_POINT_RENDERER_GPU_H_

#include <dcomp.h>
#include <wrl/client.h>

#include <iterator>
#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/delegated_ink_metadata.h"
#include "ui/gfx/mojom/delegated_ink_point_renderer.mojom.h"

// Required for SFINAE to check if these Win10 types exist.
// TODO(1171374): Remove this when the types are available in the Win10 SDK.
struct IDCompositionInkTrailDevice;
struct IDCompositionDelegatedInkTrail;
struct DCompositionInkTrailPoint;

namespace gl {

namespace {

// Maximum number of points that can be drawn. This is used to limit the total
// number of ink trail tokens that we will store, and the total number of points
// that we will store to provide to the Direct Composition APIs. It should match
// the exact number of points that the OS Compositor will store to draw as part
// of a trail.
constexpr int kMaximumNumberOfPoints = 128;

struct DelegatedInkPointCompare {
  bool operator()(const gfx::DelegatedInkPoint& lhs,
                  const gfx::DelegatedInkPoint& rhs) const {
    return lhs.timestamp() < rhs.timestamp();
  }
};

using DelegatedInkPointTokenMap = base::flat_map<gfx::DelegatedInkPoint,
                                                 absl::optional<unsigned int>,
                                                 DelegatedInkPointCompare>;

}  // namespace

// TODO(1171374): Remove this class and remove templates when the types are
// available in the Win10 SDK.
template <typename InkTrailDevice,
          typename DelegatedInkTrail,
          typename InkTrailPoint,
          typename = void,
          typename = void,
          typename = void>
class DelegatedInkPointRendererGpu {
 public:
  DelegatedInkPointRendererGpu() = default;
  bool Initialize(
      const Microsoft::WRL::ComPtr<IDCompositionDevice2>& dcomp_device,
      const Microsoft::WRL::ComPtr<IDXGISwapChain1>& root_swap_chain) {
    NOTREACHED();
    return false;
  }

  bool HasBeenInitialized() const { return false; }

  IDCompositionVisual* GetInkVisual() const {
    NOTREACHED();
    return nullptr;
  }

  bool DelegatedInkIsSupported(
      const Microsoft::WRL::ComPtr<IDCompositionDevice2>& dcomp_device) const {
    return false;
  }

  void SetDelegatedInkTrailStartPoint(
      std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
    NOTREACHED();
  }

  void InitMessagePipeline(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer> receiver) {
    NOTREACHED();
  }

  void StoreDelegatedInkPoint(const gfx::DelegatedInkPoint& point) {
    NOTREACHED();
  }

  gfx::DelegatedInkMetadata* MetadataForTesting() const {
    NOTREACHED();
    return nullptr;
  }

  DelegatedInkPointTokenMap DelegatedInkPointsForTesting() const {
    NOTREACHED();
    return DelegatedInkPointTokenMap();
  }

  uint64_t InkTrailTokenCountForTesting() const {
    NOTREACHED();
    return 0u;
  }

  bool WaitForNewTrailToDrawForTesting() const {
    NOTREACHED();
    return false;
  }
};

// This class will call the OS delegated ink trail APIs when they become
// available. Currently using SFINAE to land changes ahead of the SDK, so that
// the OS APIs can be used immediately.
// TODO(1171374): Remove the template SFINAE.
//
// On construction, this class will create a new visual for the visual tree with
// an IDCompositionDelegatedInk object as the contents. This will be added as
// a child of the root surface visual in the tree, and the trail will be drawn
// to it. It is a child of the root surface visual because this visual contains
// the swapchain, and there will be no transforms applied to the delegated ink
// visual this way.
// For more information about the design of this class and using the OS APIs,
// view the design doc here: https://aka.ms/GPUBackedDesignDoc
template <typename InkTrailDevice,
          typename DelegatedInkTrail,
          typename InkTrailPoint>
class DelegatedInkPointRendererGpu<InkTrailDevice,
                                   DelegatedInkTrail,
                                   InkTrailPoint,
                                   decltype(typeid(InkTrailDevice), void()),
                                   decltype(typeid(DelegatedInkTrail), void()),
                                   decltype(typeid(InkTrailPoint), void())>
    : public gfx::mojom::DelegatedInkPointRenderer {
 public:
  DelegatedInkPointRendererGpu() = default;

  void InitMessagePipeline(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
          pending_receiver) {
    // The remote end of this pipeline exists on a per-tab basis, so if tab A
    // is using the feature and then tab B starts trying to use it, a new
    // PendingReceiver will arrive here while |receiver_| is still bound to the
    // remote in tab A. Because of this possibility, we unconditionally reset
    // |receiver_| before trying to bind anything here. This is fine to do
    // because we only ever need to actively receive points from one tab as only
    // one tab can be in the foreground and actively inking at a time.
    receiver_.reset();

    receiver_.Bind(std::move(pending_receiver));
  }

  bool Initialize(
      const Microsoft::WRL::ComPtr<IDCompositionDevice2>& dcomp_device2,
      const Microsoft::WRL::ComPtr<IDXGISwapChain1>& root_swap_chain) {
    if (dcomp_device_ == dcomp_device2.Get() &&
        swap_chain_ == root_swap_chain.Get() && HasBeenInitialized()) {
      return true;
    }

    dcomp_device_ = dcomp_device2.Get();
    swap_chain_ = root_swap_chain.Get();

    Microsoft::WRL::ComPtr<InkTrailDevice> ink_trail_device;
    TraceEventOnFailure(dcomp_device2.As(&ink_trail_device),
                        "DelegatedInkPointRendererGpu::Initialize - "
                        "DCompDevice2 as InkTrailDevice failed");
    if (!ink_trail_device)
      return false;

    TraceEventOnFailure(
        ink_trail_device->CreateDelegatedInkTrailForSwapChain(
            root_swap_chain.Get(), &delegated_ink_trail_),
        "DelegatedInkPointRendererGpu::Initialize - Failed to create "
        "delegated ink trail.");
    if (!delegated_ink_trail_)
      return false;

    Microsoft::WRL::ComPtr<IDCompositionDevice> dcomp_device;
    TraceEventOnFailure(dcomp_device2.As(&dcomp_device),
                        "DelegatedInkPointRendererGpu::Initialize - "
                        "DCompDevice2 as DCompDevice failed");
    if (!dcomp_device)
      return false;

    TraceEventOnFailure(dcomp_device->CreateVisual(&ink_visual_),
                        "DelegatedInkPointRendererGpu::Initialize - "
                        "Failed to create ink visual.");
    if (!ink_visual_)
      return false;

    if (TraceEventOnFailure(
            ink_visual_->SetContent(delegated_ink_trail_.Get()),
            "DelegatedInkPointRendererGpu::Initialize - SetContent failed")) {
      // Initialization has failed because SetContent failed. However, we must
      // reset the members so that HasBeenInitialized() does not return true
      // when queried.
      ink_visual_.Reset();
      delegated_ink_trail_.Reset();
      return false;
    }

    return true;
  }

  bool HasBeenInitialized() const {
    return ink_visual_ && delegated_ink_trail_;
  }

  IDCompositionVisual* GetInkVisual() const { return ink_visual_.Get(); }

  bool DelegatedInkIsSupported(
      const Microsoft::WRL::ComPtr<IDCompositionDevice2>& dcomp_device) const {
    Microsoft::WRL::ComPtr<InkTrailDevice> ink_trail_device;
    HRESULT hr = dcomp_device.As(&ink_trail_device);
    return hr == S_OK;
  }

  void SetDelegatedInkTrailStartPoint(
      std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
    TRACE_EVENT1("gpu",
                 "DelegatedInkPointRendererGpu::SetDelegatedInkTrailStartPoint",
                 "metadata", metadata->ToString());

    DCHECK(ink_visual_);
    DCHECK(delegated_ink_trail_);

    // If a trail has already been started and |metadata| is a point that was
    // drawn as part of that trail (meaning |delegated_ink_points_| contains a
    // key with the same timestamp as |metadata| and it has a valid token),
    // simply remove all the points up to the point that |metadata| describes.
    // However, if the colors of |metadata_| and |metadata| don't match or the
    // location of the DelegatedInkPoint in |delegated_ink_points_| doesn't
    // match |metadata|, then we will need to start a new trail to ensure that
    // the color matches or that we are able to draw a new trail in the correct
    // location.
    auto point_matching_metadata_it = delegated_ink_points_.find(
        gfx::DelegatedInkPoint(metadata->point(), metadata->timestamp()));
    if (metadata_ && metadata->color() == metadata_->color() &&
        point_matching_metadata_it != delegated_ink_points_.end()) {
      gfx::DelegatedInkPoint point_matching_metadata =
          point_matching_metadata_it->first;
      absl::optional<unsigned int> token = point_matching_metadata_it->second;
      if (point_matching_metadata.MatchesDelegatedInkMetadata(metadata.get()) &&
          token) {
        bool remove_trail_points_failed = TraceEventOnFailure(
            delegated_ink_trail_->RemoveTrailPoints(token.value()),
            "DelegatedInkPointRendererGpu::SetDelegatedInkTrailStartPoint - "
            "Failed to remove trail points.");
        if (!remove_trail_points_failed &&
            UpdateVisualClip(metadata->presentation_area())) {
          // Remove all points up to and including the point that matches
          // |metadata|. No need to hold on to the point that matches metadata
          // because we've already added it to AddTrailPoints previously, and
          // the next valid |metadata| is guaranteed to be after it.
          delegated_ink_points_.erase(delegated_ink_points_.begin(),
                                      std::next(point_matching_metadata_it));
          metadata_ = std::move(metadata);
          return;
        }
      }
    }

    if (!UpdateVisualClip(metadata->presentation_area()))
      return;

    D2D1_COLOR_F d2d1_color;
    const float kMaxRGBAValue = 255.f;
    d2d1_color.a = SkColorGetA(metadata->color()) / kMaxRGBAValue;
    d2d1_color.r = SkColorGetR(metadata->color()) / kMaxRGBAValue;
    d2d1_color.g = SkColorGetG(metadata->color()) / kMaxRGBAValue;
    d2d1_color.b = SkColorGetB(metadata->color()) / kMaxRGBAValue;
    if (TraceEventOnFailure(
            delegated_ink_trail_->StartNewTrail(d2d1_color),
            "DelegatedInkPointRendererGpu::SetDelegatedInkTrailStartPoint - "
            "Failed to start a new trail.")) {
      return;
    }

    wait_for_new_trail_to_draw_ = false;
    metadata_ = std::move(metadata);
    DrawSavedTrailPoints();
  }

  void StoreDelegatedInkPoint(const gfx::DelegatedInkPoint& point) override {
    TRACE_EVENT1("gpu", "DelegatedInkPointRendererGpu::StoreDelegatedInkPoint",
                 "delegated ink point", point.ToString());

    DCHECK(delegated_ink_points_.empty() ||
           point.timestamp() >
               delegated_ink_points_.rbegin()->first.timestamp());

    if (metadata_ && point.timestamp() < metadata_->timestamp())
      return;

    // Always save the point so that it can be drawn later. This allows points
    // that arrive before the first metadata makes it here to be drawn after
    // StartNewTrail is called. If we get to the maximum number of points that
    // we will store, start erasing the oldest ones first. This matches what the
    // OS compositor does internally when it hits the max number of points.
    if (delegated_ink_points_.size() == kMaximumNumberOfPoints)
      delegated_ink_points_.erase(delegated_ink_points_.begin());
    delegated_ink_points_.insert({point, absl::nullopt});

    DrawDelegatedInkPoint(point);
  }

  void ResetPrediction() override {
    // Don't reset |metadata_| here so that RemoveTrailPoints() can continue
    // to be called as the final metadata(s) arrive.
    // TODO(1052145): Start predicting points and reset it here.
    wait_for_new_trail_to_draw_ = true;
  }

  gfx::DelegatedInkMetadata* MetadataForTesting() const {
    return metadata_.get();
  }

  uint64_t InkTrailTokenCountForTesting() const {
    uint64_t valid_tokens = 0u;
    for (const auto& it : delegated_ink_points_) {
      if (it.second)
        valid_tokens++;
    }
    return valid_tokens;
  }

  const DelegatedInkPointTokenMap& DelegatedInkPointsForTesting() const {
    return delegated_ink_points_;
  }

  bool WaitForNewTrailToDrawForTesting() const {
    return wait_for_new_trail_to_draw_;
  }

 private:
  // Note that this returns true if the HRESULT is anything other than S_OK,
  // meaning that it returns true when an event is traced (because of a
  // failure).
  static bool TraceEventOnFailure(HRESULT hr, const char* name) {
    if (SUCCEEDED(hr))
      return false;

    TRACE_EVENT_INSTANT1("gpu", name, TRACE_EVENT_SCOPE_THREAD, "hr", hr);
    return true;
  }

  bool UpdateVisualClip(const gfx::RectF& new_presentation_area) {
    if (metadata_ && metadata_->presentation_area() == new_presentation_area)
      return true;

    // If (0,0) of a visual is clipped out, it can result in delegated ink not
    // being drawn at all. This is more common when DComp Surfaces are enabled,
    // but doesn't negatively impact things when the swapchain is used, so just
    // offset the visual instead of clipping the top left corner in all cases.
    ink_visual_->SetOffsetX(new_presentation_area.x());
    ink_visual_->SetOffsetY(new_presentation_area.y());

    D2D_RECT_F clip_rect;
    clip_rect.bottom = new_presentation_area.bottom();
    clip_rect.right = new_presentation_area.right();
    // If setting the clip or committing failed, we want to bail early so that a
    // trail can't incorrectly appear over things on the user's screen.
    return SUCCEEDED(ink_visual_->SetClip(clip_rect)) &&
           SUCCEEDED(dcomp_device_->Commit());
  }

  void DrawSavedTrailPoints() {
    DCHECK(metadata_);

    // Remove all points that have a timestamp earlier than |metadata_|'s, since
    // we know that we won't need to draw them. This is subtly different than
    // the erasing done in SetDelegatedInkTrailStartPoint() - there the point
    // matching the metadata is removed, here it is not. The reason for this
    // difference is because there we know that it matches |metadata_| and
    // therefore it cannot possibly be drawn again, so it is safe to remove.
    // Here however, we are erasing all points up to, but not including, the
    // first point with a timestamp equal to or greater than |metadata_|'s so
    // that we can then check that point to confirm that it matches |metadata_|
    // before deciding to draw an ink trail. The point could then be erased
    // after it is successfully checked against |metadata_| and drawn, it just
    // isn't for simplicity's sake.
    delegated_ink_points_.erase(
        delegated_ink_points_.begin(),
        delegated_ink_points_.lower_bound(gfx::DelegatedInkPoint(
            metadata_->point(), metadata_->timestamp())));

    // Now, the very first point must match |metadata_|, and as long as it does
    // we can continue to draw everything else. If at any point something can't
    // or fails to draw though, don't attempt to draw anything after it so that
    // the trail can match the user's actual stroke.
    if (!delegated_ink_points_.empty() &&
        delegated_ink_points_.begin()->first.MatchesDelegatedInkMetadata(
            metadata_.get())) {
      for (auto it = delegated_ink_points_.begin();
           it != delegated_ink_points_.end(); ++it) {
        if (!DrawDelegatedInkPoint(it->first))
          break;
      }
    }
  }

  bool DrawDelegatedInkPoint(const gfx::DelegatedInkPoint& point) {
    // Always wait for a new trail to be started before attempting to draw
    // anything, even if |metadata_| exists.
    if (wait_for_new_trail_to_draw_)
      return false;

    DCHECK(metadata_);
    if (!metadata_->presentation_area().Contains(point.point()))
      return false;

    DCHECK(delegated_ink_trail_);

    InkTrailPoint ink_point;
    ink_point.radius = metadata_->diameter() / 2.f;
    // In order to account for the visual offset, the point must be offset in
    // the opposite direction.
    ink_point.x = point.point().x() - metadata_->presentation_area().x();
    ink_point.y = point.point().y() - metadata_->presentation_area().y();
    unsigned int token;

    // AddTrailPoints() can accept and draw more than one InkTrailPoint per
    // call. However, all the points get lumped together in the one token then,
    // which means that they can only be removed all together. This may be fine
    // in some scenarios, but in the vast majority of cases we will need to
    // remove one point at a time, so we choose to only add one InkTrailPoint at
    // a time.
    if (TraceEventOnFailure(
            delegated_ink_trail_->AddTrailPoints(&ink_point,
                                                 /*inkPointsCount*/ 1, &token),
            "DelegatedInkPointRendererGpu::DrawDelegatedInkPoint - Failed to "
            "add trail point")) {
      // TODO(1052145): Start predicting points.
      return false;
    }

    delegated_ink_points_[point] = token;
    return true;
  }

  // The visual within the tree that will contain the delegated ink trail. It
  // should be a child of the root surface visual.
  Microsoft::WRL::ComPtr<IDCompositionVisual> ink_visual_;

  // The delegated ink trail object that the ink trail is drawn on. This is the
  // content of the ink visual.
  Microsoft::WRL::ComPtr<DelegatedInkTrail> delegated_ink_trail_;

  // Remember the dcomp device and swap chain used to create
  // |delegated_ink_trail_| and |ink_visual_| so that we can avoid recreating
  // them when it isn't necessary.
  IDCompositionDevice2* dcomp_device_ = nullptr;
  IDXGISwapChain1* swap_chain_ = nullptr;

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
  DelegatedInkPointTokenMap delegated_ink_points_;

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

  mojo::Receiver<gfx::mojom::DelegatedInkPointRenderer> receiver_{this};
};

}  // namespace gl

#endif  // UI_GL_DELEGATED_INK_POINT_RENDERER_GPU_H_
