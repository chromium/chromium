// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/delegated_frame_host_android.h"

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/logging.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/surfaces/surface.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"

namespace ui {

namespace {

scoped_refptr<cc::SurfaceLayer> CreateSurfaceLayer(
    const viz::SurfaceId& primary_surface_id,
    const viz::SurfaceId& fallback_surface_id,
    const gfx::Size& size_in_pixels,
    const cc::DeadlinePolicy& deadline_policy,
    bool surface_opaque) {
  // manager must outlive compositors using it.
  auto layer = cc::SurfaceLayer::Create();
  layer->SetSurfaceId(primary_surface_id, deadline_policy);
  layer->SetOldestAcceptableFallback(fallback_surface_id);
  layer->SetBounds(size_in_pixels);
  layer->SetIsDrawable(true);
  layer->SetContentsOpaque(surface_opaque);
  layer->SetSurfaceHitTestable(true);

  return layer;
}

}  // namespace

DelegatedFrameHostAndroid::DelegatedFrameHostAndroid(
    ui::ViewAndroid* view,
    viz::HostFrameSinkManager* host_frame_sink_manager,
    Client* client,
    const viz::FrameSinkId& frame_sink_id)
    : frame_sink_id_(frame_sink_id),
      view_(view),
      host_frame_sink_manager_(host_frame_sink_manager),
      client_(client),
      frame_evictor_(std::make_unique<viz::FrameEvictor>(this)) {
  DCHECK(view_);
  DCHECK(client_);
  DCHECK(features::IsVizDisplayCompositorEnabled());

  constexpr bool is_transparent = false;
  content_layer_ = CreateSurfaceLayer(
      viz::SurfaceId(), viz::SurfaceId(), gfx::Size(),
      cc::DeadlinePolicy::UseDefaultDeadline(), is_transparent);
  view_->GetLayer()->AddChild(content_layer_);

  host_frame_sink_manager_->RegisterFrameSinkId(
      frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
  host_frame_sink_manager_->SetFrameSinkDebugLabel(frame_sink_id_,
                                                   "DelegatedFrameHostAndroid");
}

DelegatedFrameHostAndroid::~DelegatedFrameHostAndroid() {
  EvictDelegatedFrame();
  DetachFromCompositor();
  host_frame_sink_manager_->InvalidateFrameSinkId(frame_sink_id_);
}

const viz::FrameSinkId& DelegatedFrameHostAndroid::GetFrameSinkId() const {
  return frame_sink_id_;
}

void DelegatedFrameHostAndroid::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  DCHECK(CanCopyFromCompositingSurface());

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
          base::BindOnce(
              [](base::OnceCallback<void(const SkBitmap&)> callback,
                 std::unique_ptr<viz::CopyOutputResult> result) {
                std::move(callback).Run(result->AsSkBitmap());
              },
              std::move(callback)));

  if (!src_subrect.IsEmpty())
    request->set_area(src_subrect);
  if (!output_size.IsEmpty()) {
    // The CopyOutputRequest API does not allow fixing the output size. Instead
    // we have the set area and scale in such a way that it would result in the
    // desired output size.
    if (!request->has_area())
      request->set_area(gfx::Rect(surface_size_in_pixels_));
    request->set_result_selection(gfx::Rect(output_size));
    const gfx::Rect& area = request->area();
    // Viz would normally return an empty result for an empty area.
    // However, this guard here is still necessary to protect against setting
    // an illegal scaling ratio.
    if (area.IsEmpty())
      return;
    request->SetScaleRatio(
        gfx::Vector2d(area.width(), area.height()),
        gfx::Vector2d(output_size.width(), output_size.height()));
  }

  host_frame_sink_manager_->RequestCopyOfOutput(
      viz::SurfaceId(frame_sink_id_, local_surface_id_), std::move(request));
}

bool DelegatedFrameHostAndroid::CanCopyFromCompositingSurface() const {
  return local_surface_id_.is_valid();
}

void DelegatedFrameHostAndroid::EvictDelegatedFrame() {
  content_layer_->SetSurfaceId(viz::SurfaceId(),
                               cc::DeadlinePolicy::UseDefaultDeadline());
  if (!HasSavedFrame() || frame_evictor_->visible())
    return;
  std::vector<viz::SurfaceId> surface_ids = {
      viz::SurfaceId(frame_sink_id_, local_surface_id_)};
  host_frame_sink_manager_->EvictSurfaces(surface_ids);
  frame_evictor_->OnSurfaceDiscarded();
  // When surface sync is on, this call will force |client_| to allocate a new
  // LocalSurfaceId which will be embedded the next time the tab is shown. When
  // surface sync is off, the renderer will always allocate a new LocalSurfaceId
  // when it becomes visible just in case the previous LocalSurfaceId is evicted
  // by the browser.
  client_->WasEvicted();
}

void DelegatedFrameHostAndroid::ResetFallbackToFirstNavigationSurface() {
  // Don't update the fallback if it's already newer than the first id after
  // navigation.
  if (content_layer_->oldest_acceptable_fallback() &&
      content_layer_->oldest_acceptable_fallback()->frame_sink_id() ==
          frame_sink_id_ &&
      content_layer_->oldest_acceptable_fallback()
          ->local_surface_id()
          .IsSameOrNewerThan(first_local_surface_id_after_navigation_)) {
    return;
  }
  content_layer_->SetOldestAcceptableFallback(
      viz::SurfaceId(frame_sink_id_, first_local_surface_id_after_navigation_));
}

bool DelegatedFrameHostAndroid::HasDelegatedContent() const {
  return content_layer_->surface_id().is_valid();
}

void DelegatedFrameHostAndroid::CompositorFrameSinkChanged() {
  EvictDelegatedFrame();
  if (registered_parent_compositor_)
    AttachToCompositor(registered_parent_compositor_);
}

void DelegatedFrameHostAndroid::AttachToCompositor(
    WindowAndroidCompositor* compositor) {
  if (registered_parent_compositor_)
    DetachFromCompositor();
  compositor->AddChildFrameSink(frame_sink_id_);
  registered_parent_compositor_ = compositor;
}

void DelegatedFrameHostAndroid::DetachFromCompositor() {
  if (!registered_parent_compositor_)
    return;
  registered_parent_compositor_->RemoveChildFrameSink(frame_sink_id_);
  registered_parent_compositor_ = nullptr;
}

bool DelegatedFrameHostAndroid::IsPrimarySurfaceEvicted() const {
  return !content_layer_->surface_id().is_valid();
}

bool DelegatedFrameHostAndroid::HasSavedFrame() const {
  return frame_evictor_->has_surface();
}

void DelegatedFrameHostAndroid::WasHidden() {
  frame_evictor_->SetVisible(false);
}

void DelegatedFrameHostAndroid::WasShown(
    const viz::LocalSurfaceId& new_local_surface_id,
    const gfx::Size& new_size_in_pixels) {
  frame_evictor_->SetVisible(true);

  EmbedSurface(
      new_local_surface_id, new_size_in_pixels,
      cc::DeadlinePolicy::UseSpecifiedDeadline(FirstFrameTimeoutFrames()));
}

void DelegatedFrameHostAndroid::EmbedSurface(
    const viz::LocalSurfaceId& new_local_surface_id,
    const gfx::Size& new_size_in_pixels,
    cc::DeadlinePolicy deadline_policy) {
  // We should never attempt to embed an invalid surface. Catch this here to
  // track down the root cause. Otherwise we will have vague crashes later on
  // at serialization time.
  CHECK(new_local_surface_id.is_valid());

  local_surface_id_ = new_local_surface_id;
  surface_size_in_pixels_ = new_size_in_pixels;

  viz::SurfaceId current_primary_surface_id = content_layer_->surface_id();
  viz::SurfaceId new_primary_surface_id(frame_sink_id_, local_surface_id_);

  if (!frame_evictor_->visible()) {
    // If the tab is resized while hidden, advance the fallback so that the next
    // time user switches back to it the page is blank. This is preferred to
    // showing contents of old size. Don't call EvictDelegatedFrame to avoid
    // races when dragging tabs across displays. See https://crbug.com/813157.
    if (surface_size_in_pixels_ != content_layer_->bounds() &&
        content_layer_->oldest_acceptable_fallback() &&
        content_layer_->oldest_acceptable_fallback()->is_valid()) {
      content_layer_->SetOldestAcceptableFallback(new_primary_surface_id);
    }
    // Don't update the SurfaceLayer when invisible to avoid blocking on
    // renderers that do not submit CompositorFrames. Next time the renderer
    // is visible, EmbedSurface will be called again. See WasShown.
    return;
  }

  frame_evictor_->OnNewSurfaceEmbedded();

  if (!current_primary_surface_id.is_valid() ||
      current_primary_surface_id.local_surface_id() != local_surface_id_) {
    if (base::android::BuildInfo::GetInstance()->sdk_int() <
        base::android::SDK_VERSION_OREO) {
      // On version of Android earlier than Oreo, we would like to produce new
      // content as soon as possible or the OS will create an additional black
      // gutter. We only reset the deadline on the first frame (no bounds yet
      // specified) or on resize, and only if the deadline policy is not
      // infinite.
      if (deadline_policy.policy_type() !=
              cc::DeadlinePolicy::kUseInfiniteDeadline &&
          (content_layer_->bounds().IsEmpty() ||
           content_layer_->bounds() != surface_size_in_pixels_)) {
        deadline_policy = cc::DeadlinePolicy::UseSpecifiedDeadline(0u);
      }
    }
    content_layer_->SetSurfaceId(new_primary_surface_id, deadline_policy);
    content_layer_->SetBounds(new_size_in_pixels);
  }
}

void DelegatedFrameHostAndroid::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  NOTREACHED();
}

void DelegatedFrameHostAndroid::OnFrameTokenChanged(uint32_t frame_token) {
  client_->OnFrameTokenChanged(frame_token);
}

viz::SurfaceId DelegatedFrameHostAndroid::SurfaceId() const {
  return viz::SurfaceId(frame_sink_id_, local_surface_id_);
}

bool DelegatedFrameHostAndroid::HasPrimarySurface() const {
  return content_layer_->surface_id().is_valid();
}

bool DelegatedFrameHostAndroid::HasFallbackSurface() const {
  return content_layer_->oldest_acceptable_fallback() &&
         content_layer_->oldest_acceptable_fallback()->is_valid();
}

void DelegatedFrameHostAndroid::TakeFallbackContentFrom(
    DelegatedFrameHostAndroid* other) {
  if (HasFallbackSurface() || !other->HasPrimarySurface())
    return;

  const viz::SurfaceId& other_primary = other->content_layer_->surface_id();
  const base::Optional<viz::SurfaceId>& other_fallback =
      other->content_layer_->oldest_acceptable_fallback();
  viz::SurfaceId desired_fallback;
  if (!other->HasFallbackSurface() ||
      !other_primary.IsSameOrNewerThan(*other_fallback)) {
    desired_fallback = other_primary.ToSmallestId();
  } else {
    desired_fallback = *other_fallback;
  }
  content_layer_->SetOldestAcceptableFallback(
      other->content_layer_->surface_id().ToSmallestId());
}

void DelegatedFrameHostAndroid::DidNavigate() {
  first_local_surface_id_after_navigation_ = local_surface_id_;
}

}  // namespace ui
