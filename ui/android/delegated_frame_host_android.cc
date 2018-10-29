// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/delegated_frame_host_android.h"

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/debug/stack_trace.h"
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
    const viz::FrameSinkId& frame_sink_id,
    bool enable_surface_synchronization)
    : frame_sink_id_(frame_sink_id),
      view_(view),
      host_frame_sink_manager_(host_frame_sink_manager),
      client_(client),
      begin_frame_source_(this),
      enable_surface_synchronization_(enable_surface_synchronization),
      enable_viz_(
          base::FeatureList::IsEnabled(features::kVizDisplayCompositor)),
      frame_evictor_(std::make_unique<viz::FrameEvictor>(this)) {
  DCHECK(view_);
  DCHECK(client_);

  if (enable_surface_synchronization_) {
    constexpr bool is_transparent = false;
    content_layer_ = CreateSurfaceLayer(
        viz::SurfaceId(), viz::SurfaceId(), gfx::Size(),
        cc::DeadlinePolicy::UseDefaultDeadline(), is_transparent);
    view_->GetLayer()->AddChild(content_layer_);
  }

  host_frame_sink_manager_->RegisterFrameSinkId(
      frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
  host_frame_sink_manager_->SetFrameSinkDebugLabel(frame_sink_id_,
                                                   "DelegatedFrameHostAndroid");
  CreateCompositorFrameSinkSupport();
}

DelegatedFrameHostAndroid::~DelegatedFrameHostAndroid() {
  EvictDelegatedFrame();
  DetachFromCompositor();
  support_.reset();
  host_frame_sink_manager_->InvalidateFrameSinkId(frame_sink_id_);
}

void DelegatedFrameHostAndroid::SubmitCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame,
    base::Optional<viz::HitTestRegionList> hit_test_region_list) {
  DCHECK(!enable_viz_);

  bool id_changed = (local_surface_id_ != local_surface_id);
  viz::RenderPass* root_pass = frame.render_pass_list.back().get();
  const bool has_transparent_background = root_pass->has_transparent_background;
  const gfx::Size surface_size_in_pixels = frame.size_in_pixels();
  // Reset |content_layer_| only if surface-sync is not used. When surface-sync
  // is turned on, |content_layer_| is updated with the appropriate states (see
  // in EmbedSurface()) instead of being recreated.
  if (!enable_surface_synchronization_ && content_layer_ && id_changed) {
    EvictDelegatedFrame();
  }
  support_->SubmitCompositorFrame(local_surface_id, std::move(frame),
                                  std::move(hit_test_region_list));
  if (enable_surface_synchronization_) {
    DCHECK(content_layer_);
    return;
  }

  if (!content_layer_) {
    local_surface_id_ = local_surface_id;
    surface_size_in_pixels_ = surface_size_in_pixels;
    has_transparent_background_ = has_transparent_background;
    content_layer_ = CreateSurfaceLayer(
        viz::SurfaceId(frame_sink_id_, local_surface_id_),
        viz::SurfaceId(frame_sink_id_, local_surface_id_),
        surface_size_in_pixels_, cc::DeadlinePolicy::UseDefaultDeadline(),
        !has_transparent_background_);
    view_->GetLayer()->AddChild(content_layer_);
  }
  content_layer_->SetContentsOpaque(!has_transparent_background_);

  compositor_attach_until_frame_lock_.reset();

  // If surface synchronization is disabled, SubmitCompositorFrame immediately
  // activates the CompositorFrame and issues OnFirstSurfaceActivation if the
  // |local_surface_id| has changed since the last submission.
  if (content_layer_->bounds() == expected_pixel_size_)
    compositor_pending_resize_lock_.reset();

  if (id_changed)
    frame_evictor_->OnNewSurfaceEmbedded();
}

void DelegatedFrameHostAndroid::DidNotProduceFrame(
    const viz::BeginFrameAck& ack) {
  DCHECK(!enable_viz_);
  support_->DidNotProduceFrame(ack);
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
  if (!content_layer_)
    return;
  content_layer_->SetSurfaceId(viz::SurfaceId(),
                               cc::DeadlinePolicy::UseDefaultDeadline());
  if (!enable_surface_synchronization_) {
    content_layer_->RemoveFromParent();
    content_layer_ = nullptr;
  }
  if (!HasSavedFrame())
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
  if (!content_layer_)
    return;
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
  return content_layer_ && content_layer_->surface_id().is_valid();
}

void DelegatedFrameHostAndroid::CompositorFrameSinkChanged() {
  EvictDelegatedFrame();
  CreateCompositorFrameSinkSupport();
  if (registered_parent_compositor_)
    AttachToCompositor(registered_parent_compositor_);
}

void DelegatedFrameHostAndroid::AttachToCompositor(
    WindowAndroidCompositor* compositor) {
  if (registered_parent_compositor_)
    DetachFromCompositor();
  // If this is the first frame after the compositor became visible, we want to
  // take the compositor lock, preventing compositor frames from being produced
  // until all delegated frames are ready. This improves the resume transition,
  // preventing flashes. Set a 5 second timeout to prevent locking up the
  // browser in cases where the renderer hangs or another factor prevents a
  // frame from being produced. If we already have delegated content, no need
  // to take the lock.
  // If surface synchronization is enabled, then it will block browser UI until
  // a renderer frame is available instead.
  if (!enable_surface_synchronization_ &&
      compositor->IsDrawingFirstVisibleFrame() && !HasDelegatedContent()) {
    compositor_attach_until_frame_lock_ =
        compositor->GetCompositorLock(this, FirstFrameTimeout());
  }
  compositor->AddChildFrameSink(frame_sink_id_);
  if (!enable_viz_)
    client_->SetBeginFrameSource(&begin_frame_source_);
  registered_parent_compositor_ = compositor;
}

void DelegatedFrameHostAndroid::DetachFromCompositor() {
  if (!registered_parent_compositor_)
    return;
  compositor_attach_until_frame_lock_.reset();
  compositor_pending_resize_lock_.reset();
  if (!enable_viz_) {
    client_->SetBeginFrameSource(nullptr);
    support_->SetNeedsBeginFrame(false);
  }
  registered_parent_compositor_->RemoveChildFrameSink(frame_sink_id_);
  registered_parent_compositor_ = nullptr;
}

bool DelegatedFrameHostAndroid::IsPrimarySurfaceEvicted() const {
  return !content_layer_ || !content_layer_->surface_id().is_valid();
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

  if (!enable_surface_synchronization_)
    return;

  EmbedSurface(
      new_local_surface_id, new_size_in_pixels,
      cc::DeadlinePolicy::UseSpecifiedDeadline(FirstFrameTimeoutFrames()));
}

void DelegatedFrameHostAndroid::EmbedSurface(
    const viz::LocalSurfaceId& new_local_surface_id,
    const gfx::Size& new_size_in_pixels,
    cc::DeadlinePolicy deadline_policy) {
  if (!enable_surface_synchronization_)
    return;

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

void DelegatedFrameHostAndroid::PixelSizeWillChange(
    const gfx::Size& pixel_size) {
  if (enable_surface_synchronization_)
    return;

  // We never take the resize lock unless we're on O+, as previous versions of
  // Android won't wait for us to produce the correct sized frame and will end
  // up looking worse.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_OREO) {
    return;
  }

  expected_pixel_size_ = pixel_size;
  if (content_layer_ && registered_parent_compositor_) {
    if (content_layer_->bounds() != expected_pixel_size_) {
      compositor_pending_resize_lock_ =
          registered_parent_compositor_->GetCompositorLock(this,
                                                           ResizeTimeout());
    }
  }
}

void DelegatedFrameHostAndroid::DidReceiveCompositorFrameAck(
    const std::vector<viz::ReturnedResource>& resources) {
  client_->DidReceiveCompositorFrameAck(resources);
}

void DelegatedFrameHostAndroid::DidPresentCompositorFrame(
    uint32_t presentation_token,
    const gfx::PresentationFeedback& feedback) {
  client_->DidPresentCompositorFrame(presentation_token, feedback);
}

void DelegatedFrameHostAndroid::OnBeginFrame(const viz::BeginFrameArgs& args) {
  if (enable_viz_) {
    NOTREACHED();
    return;
  }
  begin_frame_source_.OnBeginFrame(args);
}

void DelegatedFrameHostAndroid::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  client_->ReclaimResources(resources);
}

void DelegatedFrameHostAndroid::OnBeginFramePausedChanged(bool paused) {
  begin_frame_source_.OnSetBeginFrameSourcePaused(paused);
}

void DelegatedFrameHostAndroid::OnNeedsBeginFrames(bool needs_begin_frames) {
  DCHECK(!enable_viz_);
  support_->SetNeedsBeginFrame(needs_begin_frames);
}

void DelegatedFrameHostAndroid::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  NOTREACHED();
}

void DelegatedFrameHostAndroid::OnFrameTokenChanged(uint32_t frame_token) {
  client_->OnFrameTokenChanged(frame_token);
}

void DelegatedFrameHostAndroid::CompositorLockTimedOut() {}

void DelegatedFrameHostAndroid::CreateCompositorFrameSinkSupport() {
  if (enable_viz_)
    return;

  constexpr bool is_root = false;
  constexpr bool needs_sync_points = true;
  support_.reset();
  support_ = host_frame_sink_manager_->CreateCompositorFrameSinkSupport(
      this, frame_sink_id_, is_root, needs_sync_points);
}

viz::SurfaceId DelegatedFrameHostAndroid::SurfaceId() const {
  return viz::SurfaceId(frame_sink_id_, local_surface_id_);
}

bool DelegatedFrameHostAndroid::HasPrimarySurface() const {
  return content_layer_ && content_layer_->surface_id().is_valid();
}

bool DelegatedFrameHostAndroid::HasFallbackSurface() const {
  return content_layer_ && content_layer_->oldest_acceptable_fallback() &&
         content_layer_->oldest_acceptable_fallback()->is_valid();
}

void DelegatedFrameHostAndroid::TakeFallbackContentFrom(
    DelegatedFrameHostAndroid* other) {
  if (HasFallbackSurface() || !other->HasPrimarySurface())
    return;

  if (enable_surface_synchronization_) {
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
    return;
  }

  if (content_layer_) {
    content_layer_->SetSurfaceId(
        *other->content_layer_->oldest_acceptable_fallback(),
        cc::DeadlinePolicy::UseDefaultDeadline());
  } else {
    const auto& surface_id = other->SurfaceId();
    local_surface_id_ = surface_id.local_surface_id();
    surface_size_in_pixels_ = other->surface_size_in_pixels_;
    has_transparent_background_ = other->has_transparent_background_;
    content_layer_ = CreateSurfaceLayer(
        surface_id, surface_id, other->content_layer_->bounds(),
        cc::DeadlinePolicy::UseDefaultDeadline(),
        other->content_layer_->contents_opaque());
    view_->GetLayer()->AddChild(content_layer_);
  }
  content_layer_->SetOldestAcceptableFallback(
      *other->content_layer_->oldest_acceptable_fallback());
}

void DelegatedFrameHostAndroid::DidNavigate() {
  if (!enable_surface_synchronization_)
    return;

  first_local_surface_id_after_navigation_ = local_surface_id_;
}

}  // namespace ui
