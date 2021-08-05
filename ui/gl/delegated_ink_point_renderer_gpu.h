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

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/bindings/receiver.h"
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

  base::circular_deque<gfx::DelegatedInkPoint> DelegatedInkPointsForTesting()
      const {
    NOTREACHED();
    return base::circular_deque<gfx::DelegatedInkPoint>();
  }

  base::flat_map<base::TimeTicks, unsigned int> InkTrailTokensForTesting()
      const {
    NOTREACHED();
    return base::flat_map<base::TimeTicks, unsigned int>();
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
    // drawn as part of that trail (meaning it is found in |ink_trail_tokens_|),
    // simply remove all the points up to the point that |metadata| describes.
    // The only exception to this is if the color on |metadata| doesn't match
    // |metadata_|'s color - then a new trail needs to be started to describe
    // the new color.
    auto metadata_token = ink_trail_tokens_.find(metadata->timestamp());
    if (metadata_ && metadata->color() == metadata_->color() &&
        metadata_token != ink_trail_tokens_.end()) {
      bool remove_trail_points_failed = TraceEventOnFailure(
          delegated_ink_trail_->RemoveTrailPoints(metadata_token->second),
          "DelegatedInkPointRendererGpu::SetDelegatedInkTrailStartPoint - "
          "Failed to remove trail points.");
      if (!remove_trail_points_failed &&
          UpdateVisualClip(metadata->presentation_area())) {
        ink_trail_tokens_.erase(ink_trail_tokens_.begin(),
                                std::next(metadata_token));
        metadata_ = std::move(metadata);
        RemoveSavedPointsOlderThanMetadata();
        return;
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
    ink_trail_tokens_.clear();
    metadata_ = std::move(metadata);
    DrawSavedTrailPoints();
  }

  void StoreDelegatedInkPoint(const gfx::DelegatedInkPoint& point) override {
    TRACE_EVENT1("gpu", "DelegatedInkPointRendererGpu::StoreDelegatedInkPoint",
                 "delegated ink point", point.ToString());

    DCHECK(delegated_ink_points_.empty() ||
           point.timestamp() > delegated_ink_points_.back().timestamp());

    if (metadata_ && point.timestamp() < metadata_->timestamp())
      return;

    // Always save the point so that it can be drawn later. This allows points
    // that arrive before the first metadata makes it here to be drawn after
    // StartNewTrail is called. If we get to the maximum number of points that
    // we will store, start erasing the oldest ones first. This matches what the
    // OS compositor does internally when it hits the max number of points.
    if (delegated_ink_points_.size() == kMaximumNumberOfPoints)
      delegated_ink_points_.pop_front();
    delegated_ink_points_.push_back(point);

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

  const base::circular_deque<gfx::DelegatedInkPoint>&
  DelegatedInkPointsForTesting() const {
    return delegated_ink_points_;
  }

  const base::flat_map<base::TimeTicks, unsigned int>&
  InkTrailTokensForTesting() const {
    return ink_trail_tokens_;
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

  void RemoveSavedPointsOlderThanMetadata() {
    DCHECK(metadata_);
    while (!delegated_ink_points_.empty() &&
           delegated_ink_points_.front().timestamp() < metadata_->timestamp()) {
      delegated_ink_points_.pop_front();
    }
  }

  void DrawSavedTrailPoints() {
    RemoveSavedPointsOlderThanMetadata();

    for (const gfx::DelegatedInkPoint& point : delegated_ink_points_)
      DrawDelegatedInkPoint(point);
  }

  void DrawDelegatedInkPoint(const gfx::DelegatedInkPoint& point) {
    if (wait_for_new_trail_to_draw_ || !metadata_ ||
        !metadata_->presentation_area().Contains(point.point())) {
      return;
    }

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
      return;
    }

    if (ink_trail_tokens_.size() == kMaximumNumberOfPoints)
      ink_trail_tokens_.erase(ink_trail_tokens_.begin());

    ink_trail_tokens_.insert({point.timestamp(), token});
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

  // All the points that have arrived in StoreDelegatedInkPoint(). These are
  // stored in increasing timestamp order, and are not guaranteed to be
  // currently drawn to the screen. They are not guaranteed to be currently on
  // the screen because all points that arrive are stored, even if no metadata
  // has arrived in SetDelegatedInkTrailStartPoint(), the DelegatedInkPoint
  // is outside of |metadata_|'s presentation area, or we are waiting for a new
  // SetDelegatedInkTrailStartPoint() call. The only exception is if a point
  // arrives with a timestamp earlier than |metadata_|'s, then we just bail
  // without even saving it. Each time a new trail is started, all points in
  // |delegated_ink_points_| are iterated over and either removed from the list
  // because their timestamp is earlier than |metadata_|'s, or we send them
  // through StoreDelegatedInkPoint() again.
  base::circular_deque<gfx::DelegatedInkPoint> delegated_ink_points_;

  // Tokens received when adding points to the trail. The tokens are then later
  // used to remove points from the trail. The timestamps should always match
  // all the points that are being drawn except the first point of the trail and
  // no more.
  base::flat_map<base::TimeTicks, unsigned int> ink_trail_tokens_;

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
