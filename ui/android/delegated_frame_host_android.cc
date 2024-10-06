// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/delegated_frame_host_android.h"

#include <iterator>

#include "base/android/build_info.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/slim/layer.h"
#include "cc/slim/layer_tree.h"
#include "cc/slim/surface_layer.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"

namespace ui {

namespace {

scoped_refptr<cc::slim::SurfaceLayer> CreateSurfaceLayer(
    const viz::SurfaceId& primary_surface_id,
    const viz::SurfaceId& fallback_surface_id,
    const gfx::Size& size_in_pixels,
    const cc::DeadlinePolicy& deadline_policy,
    bool surface_opaque) {
  // manager must outlive compositors using it.
  auto layer = cc::slim::SurfaceLayer::Create();
  layer->SetSurfaceId(primary_surface_id, deadline_policy);
  layer->SetOldestAcceptableFallback(fallback_surface_id);
  layer->SetBounds(size_in_pixels);
  layer->SetIsDrawable(true);
  layer->SetContentsOpaque(surface_opaque);

  return layer;
}

// From content::VisibleTimeRequestTrigger::ConsumeAndMergeRequests
// TODO(crbug.com/40203057): Use separate start time for each event.
blink::mojom::RecordContentToVisibleTimeRequestPtr ConsumeAndMergeRequests(
    blink::mojom::RecordContentToVisibleTimeRequestPtr request1,
    blink::mojom::RecordContentToVisibleTimeRequestPtr request2) {
  if (!request1 && !request2)
    return nullptr;

  // Pick any non-null request to merge into.
  blink::mojom::RecordContentToVisibleTimeRequestPtr to;
  blink::mojom::RecordContentToVisibleTimeRequestPtr from;
  if (request1) {
    to = std::move(request1);
    from = std::move(request2);
  } else {
    to = std::move(request2);
    from = std::move(request1);
  }

  if (from) {
    to->event_start_time =
        std::min(to->event_start_time, from->event_start_time);
    to->destination_is_loaded |= from->destination_is_loaded;
    to->show_reason_tab_switching |= from->show_reason_tab_switching;
    to->show_reason_bfcache_restore |= from->show_reason_bfcache_restore;
  }
  return to;
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

  constexpr bool is_transparent = false;
  content_layer_ = CreateSurfaceLayer(
      viz::SurfaceId(), viz::SurfaceId(), gfx::Size(),
      cc::DeadlinePolicy::UseDefaultDeadline(), is_transparent);
  view_->GetLayer()->AddChild(content_layer_);
}

DelegatedFrameHostAndroid::~DelegatedFrameHostAndroid() {
  EvictDelegatedFrame(frame_evictor_->CollectSurfaceIdsForEviction());
  DetachFromCompositor();
  if (owns_frame_sink_id_) {
    host_frame_sink_manager_->InvalidateFrameSinkId(frame_sink_id_, this);
  }
}

void DelegatedFrameHostAndroid::SetIsFrameSinkIdOwner(bool is_owner) {
  if (is_owner == owns_frame_sink_id_) {
    return;
  }

  owns_frame_sink_id_ = is_owner;
  if (owns_frame_sink_id_) {
    host_frame_sink_manager_->RegisterFrameSinkId(
        frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
    host_frame_sink_manager_->SetFrameSinkDebugLabel(
        frame_sink_id_, "DelegatedFrameHostAndroid");
  }
}

void DelegatedFrameHostAndroid::RegisterOffsetTags(
    const cc::BrowserControlsOffsetTagsInfo& tags_info) {
  const viz::OffsetTag top_controls_offset_tag =
      tags_info.top_controls_offset_tag;
  if (!top_controls_offset_tag.IsEmpty()) {
    int top_controls_height = tags_info.top_controls_height;
    viz::OffsetTagConstraints top_controls_constraints(0, 0,
                                                       -top_controls_height, 0);
    content_layer_->RegisterOffsetTag(top_controls_offset_tag,
                                      top_controls_constraints);
  }
}

void DelegatedFrameHostAndroid::UnregisterOffsetTags(
    const cc::BrowserControlsOffsetTagsInfo& tags_info) {
  const viz::OffsetTag top_controls_offset_tag =
      tags_info.top_controls_offset_tag;
  if (!top_controls_offset_tag.IsEmpty()) {
    content_layer_->UnregisterOffsetTag(top_controls_offset_tag);
  }
}

const viz::FrameSinkId& DelegatedFrameHostAndroid::GetFrameSinkId() const {
  return frame_sink_id_;
}

void DelegatedFrameHostAndroid::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const SkBitmap&)> callback,
    bool capture_exact_surface_id) {
  DCHECK(CanCopyFromCompositingSurface());

  const viz::SurfaceId surface_id(frame_sink_id_, local_surface_id_);

  ui::WindowAndroidCompositor::ScopedKeepSurfaceAliveCallback
      keep_surface_alive;
  if (view_->GetWindowAndroid() && view_->GetWindowAndroid()->GetCompositor()) {
    keep_surface_alive = view_->GetWindowAndroid()
                             ->GetCompositor()
                             ->TakeScopedKeepSurfaceAliveCallback(surface_id);
  }

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA,
          viz::CopyOutputRequest::ResultDestination::kSystemMemory,
          base::BindOnce(
              [](base::OnceCallback<void(const SkBitmap&)> copy_result,
                 ui::WindowAndroidCompositor::ScopedKeepSurfaceAliveCallback
                     keep_alive,
                 std::unique_ptr<viz::CopyOutputResult> result) {
                if (keep_alive) {
                  std::move(keep_alive).Run();
                }
                auto scoped_bitmap = result->ScopedAccessSkBitmap();
                std::move(copy_result).Run(scoped_bitmap.GetOutScopedBitmap());
              },
              std::move(callback), std::move(keep_surface_alive)));

  // `CopyOutputRequestCallback` holds a `ReadbackRefCallback` which must only
  // be executed on the UI thread. Since the result callback can be dispatched
  // on any thread by default, explicitly set the result task runner to the
  // current thread.
  request->set_result_task_runner(
      base::SequencedTaskRunner::GetCurrentDefault());

  viz::SetCopyOutoutRequestResultSize(request.get(), src_subrect, output_size,
                                      surface_size_in_pixels_);

  host_frame_sink_manager_->RequestCopyOfOutput(surface_id, std::move(request),
                                                capture_exact_surface_id);
}

bool DelegatedFrameHostAndroid::CanCopyFromCompositingSurface() const {
  return local_surface_id_.is_valid();
}

void DelegatedFrameHostAndroid::EvictDelegatedFrame(
    const std::vector<viz::SurfaceId>& surface_ids) {
  content_layer_->SetSurfaceId(viz::SurfaceId(),
                               cc::DeadlinePolicy::UseDefaultDeadline());
  // If we have a surface from before a navigation, evict it, regardless of
  // visibility state.
  //
  // TODO(crbug.com/40919347): Investigate why guarding the invalid
  // `pre_navigation_local_surface_id_` for Android only.
  if (!pre_navigation_local_surface_id_.is_valid() &&
      (!HasSavedFrame() || frame_evictor_->visible())) {
    return;
  }

  UMA_HISTOGRAM_COUNTS_100("MemoryAndroid.EvictedTreeSize2",
                           surface_ids.size());
  if (surface_ids.empty())
    return;
  host_frame_sink_manager_->EvictSurfaces(surface_ids);
  frame_evictor_->OnSurfaceDiscarded();
  // When surface sync is on, this call will force |client_| to allocate a new
  // LocalSurfaceId which will be embedded the next time the tab is shown. When
  // surface sync is off, the renderer will always allocate a new LocalSurfaceId
  // when it becomes visible just in case the previous LocalSurfaceId is evicted
  // by the browser.
  client_->WasEvicted();
}

viz::FrameEvictorClient::EvictIds
DelegatedFrameHostAndroid::CollectSurfaceIdsForEviction() const {
  viz::FrameEvictorClient::EvictIds ids;
  ids.embedded_ids = client_->CollectSurfaceIdsForEviction();
  return ids;
}

viz::SurfaceId DelegatedFrameHostAndroid::GetCurrentSurfaceId() const {
  return viz::SurfaceId(frame_sink_id_, local_surface_id_);
}

viz::SurfaceId DelegatedFrameHostAndroid::GetPreNavigationSurfaceId() const {
  return viz::SurfaceId(frame_sink_id_, pre_navigation_local_surface_id_);
}

viz::SurfaceId DelegatedFrameHostAndroid::GetFallbackSurfaceIdForTesting()
    const {
  return content_layer_->oldest_acceptable_fallback().value_or(
      viz::SurfaceId());
}

viz::SurfaceId DelegatedFrameHostAndroid::GetCurrentSurfaceIdForTesting()
    const {
  return GetCurrentSurfaceId();
}

viz::SurfaceId
DelegatedFrameHostAndroid::GetFirstSurfaceIdAfterNavigationForTesting() const {
  return viz::SurfaceId(frame_sink_id_,
                        first_local_surface_id_after_navigation_);
}

void DelegatedFrameHostAndroid::ClearFallbackSurfaceForCommitPending() {
  const std::optional<viz::SurfaceId> fallback_surface_id =
      content_layer_->oldest_acceptable_fallback();

  // CommitPending without a target for TakeFallbackContentFrom. Since we cannot
  // guarantee that Navigation will complete, evict our surfaces which are from
  // a previous Navigation.
  if (fallback_surface_id && fallback_surface_id->is_valid()) {
    EvictDelegatedFrame(frame_evictor_->CollectSurfaceIdsForEviction());
    content_layer_->SetOldestAcceptableFallback(viz::SurfaceId());
  }
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

  // If we have a surface from before a navigation, evict it as well.
  if (pre_navigation_local_surface_id_.is_valid() &&
      !first_local_surface_id_after_navigation_.is_valid()) {
    // If we have a valid `pre_navigation_local_surface_id_`, we must not be in
    // BFCache.
    {
      // TODO(https://crbug.com/349073060): Remove the scope when the bug is
      // fixed.
      SCOPED_CRASH_KEY_STRING64("crbug/349073060", "bfc_fallback_crashed",
                                bfcache_fallback_.ToString().c_str());
      SCOPED_CRASH_KEY_STRING64(
          "crbug/349073060", "pre_nav_lsid_crashed",
          pre_navigation_local_surface_id_.ToString().c_str());
      CHECK(!bfcache_fallback_.is_valid());
    }
    EvictDelegatedFrame(frame_evictor_->CollectSurfaceIdsForEviction());
    content_layer_->SetBackgroundColor(SkColors::kTransparent);
  }

  content_layer_->SetOldestAcceptableFallback(
      viz::SurfaceId(frame_sink_id_, first_local_surface_id_after_navigation_));
}

bool DelegatedFrameHostAndroid::HasDelegatedContent() const {
  return content_layer_->surface_id().is_valid();
}

void DelegatedFrameHostAndroid::CompositorFrameSinkChanged() {
  EvictDelegatedFrame(frame_evictor_->CollectSurfaceIdsForEviction());
  if (registered_parent_compositor_)
    AttachToCompositor(registered_parent_compositor_);
}

void DelegatedFrameHostAndroid::AttachToCompositor(
    WindowAndroidCompositor* compositor) {
  if (registered_parent_compositor_)
    DetachFromCompositor();
  compositor->AddFrameSubmissionObserver(client_);
  compositor->AddChildFrameSink(frame_sink_id_);
  registered_parent_compositor_ = compositor;
  if (content_to_visible_time_request_) {
    registered_parent_compositor_
        ->PostRequestSuccessfulPresentationTimeForNextFrame(
            content_to_visible_time_recorder_.TabWasShown(
                /*has_saved_frames=*/true,
                std::move(content_to_visible_time_request_)));
  }
}

void DelegatedFrameHostAndroid::DetachFromCompositor() {
  if (!registered_parent_compositor_)
    return;
  registered_parent_compositor_->RemoveFrameSubmissionObserver(client_);
  registered_parent_compositor_->RemoveChildFrameSink(frame_sink_id_);
  registered_parent_compositor_ = nullptr;
  content_to_visible_time_request_ = nullptr;
}

bool DelegatedFrameHostAndroid::IsPrimarySurfaceEvicted() const {
  return !content_layer_->surface_id().is_valid();
}

bool DelegatedFrameHostAndroid::HasSavedFrame() const {
  return frame_evictor_->has_surface();
}

void DelegatedFrameHostAndroid::WasHidden() {
  CancelSuccessfulPresentationTimeRequest();
  frame_evictor_->SetVisible(false);
}

void DelegatedFrameHostAndroid::WasShown(
    const viz::LocalSurfaceId& new_local_surface_id,
    const gfx::Size& new_size_in_pixels,
    bool is_fullscreen,
    blink::mojom::RecordContentToVisibleTimeRequestPtr
        content_to_visible_time_request) {
  if (content_to_visible_time_request) {
    PostRequestSuccessfulPresentationTimeForNextFrame(
        std::move(content_to_visible_time_request));
  }
  frame_evictor_->SetVisible(true);

  EmbedSurface(
      new_local_surface_id, new_size_in_pixels,
      cc::DeadlinePolicy::UseSpecifiedDeadline(FirstFrameTimeoutFrames()),
      is_fullscreen);
}

void DelegatedFrameHostAndroid::EmbedSurface(
    const viz::LocalSurfaceId& new_local_surface_id,
    const gfx::Size& new_size_in_pixels,
    cc::DeadlinePolicy deadline_policy,
    bool is_fullscreen) {
  TRACE_EVENT2("viz", "DelegatedFrameHostAndroid::EmbedSurface", "surface_id",
               new_local_surface_id.ToString(), "deadline_policy",
               deadline_policy.ToString());

  // We should never attempt to embed an invalid surface. Catch this here to
  // track down the root cause. Otherwise we will have vague crashes later on
  // at serialization time.
  CHECK(new_local_surface_id.is_valid());

  // Confirm that there is a valid fallback surface on, otherwise we need to
  // adjust deadline times. To avoid displaying invalid content.
  bool has_fallback_surface =
      (content_layer_->oldest_acceptable_fallback() &&
       content_layer_->oldest_acceptable_fallback()->is_valid());
  SetLocalSurfaceId(new_local_surface_id);
  // The embedding of a new surface completes the navigation process.
  pre_navigation_local_surface_id_ = viz::LocalSurfaceId();
  // Navigations performed while hidden delay embedding until transitioning to
  // becoming visible. So we may not have a valid surace when DidNavigate is
  // called. Cache the first surface here so we have the correct oldest surface
  // to fallback to.
  if (!first_local_surface_id_after_navigation_.is_valid())
    first_local_surface_id_after_navigation_ = local_surface_id_;
  surface_size_in_pixels_ = new_size_in_pixels;

  viz::SurfaceId current_primary_surface_id = content_layer_->surface_id();
  viz::SurfaceId new_primary_surface_id(frame_sink_id_, local_surface_id_);

  if (!frame_evictor_->visible() || is_fullscreen) {
    // For fullscreen or when tab is hidden  we don't want to display old sized
    // content. So we advance the fallback forcing viz to fallback to blank
    // screen if renderer won't submit frame in time. See
    // https://crbug.com/1088369 and  https://crbug.com/813157
    //
    // An empty content layer bounds indicates this renderer has never been made
    // visible. This is the case for pre-rendered contents. Don't use the
    // primary id as fallback since it's guaranteed to have no content. See
    // crbug.com/1218238.
    if (!content_layer_->bounds().IsEmpty() &&
        surface_size_in_pixels_ != content_layer_->bounds() &&
        (has_fallback_surface || bfcache_fallback_.is_valid())) {
      content_layer_->SetOldestAcceptableFallback(new_primary_surface_id);
      // We default to black background for fullscreen case.
      content_layer_->SetBackgroundColor(
          is_fullscreen ? SkColors::kBlack : SkColors::kTransparent);

      // Invalidates `bfcache_fallback_`, resize-while-hidden has given us the
      // latest `local_surface_id_`.
      bfcache_fallback_ =
          viz::ParentLocalSurfaceIdAllocator::InvalidLocalSurfaceId();
    }
  }

  if (!frame_evictor_->visible()) {
    // Don't update the SurfaceLayer when invisible to avoid blocking on
    // renderers that do not submit CompositorFrames. Next time the renderer
    // is visible, EmbedSurface will be called again. See WasShown.
    return;
  }

  frame_evictor_->OnNewSurfaceEmbedded();

  if (bfcache_fallback_.is_valid()) {
    // Inform Viz to show the primary surface with new ID asap; if the new
    // surface isn't ready, use the fallback.
    deadline_policy = cc::DeadlinePolicy::UseSpecifiedDeadline(0u);
    content_layer_->SetOldestAcceptableFallback(
        viz::SurfaceId(frame_sink_id_, bfcache_fallback_));
    bfcache_fallback_ =
        viz::ParentLocalSurfaceIdAllocator::InvalidLocalSurfaceId();
  }

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
    // If there is not a valid current surface, nor a valid fallback, we want to
    // produce new content as soon as possible. To avoid displaying invalide
    // content, such as surfaces from before a navigation.
    if (!has_fallback_surface)
      deadline_policy = cc::DeadlinePolicy::UseSpecifiedDeadline(0u);
    content_layer_->SetSurfaceId(new_primary_surface_id, deadline_policy);
    content_layer_->SetBounds(new_size_in_pixels);
  }
}

void DelegatedFrameHostAndroid::RequestSuccessfulPresentationTimeForNextFrame(
    blink::mojom::RecordContentToVisibleTimeRequestPtr
        content_to_content_to_visible_time_request) {
  PostRequestSuccessfulPresentationTimeForNextFrame(
      std::move(content_to_content_to_visible_time_request));
}

void DelegatedFrameHostAndroid::CancelSuccessfulPresentationTimeRequest() {
  content_to_visible_time_request_.reset();
  content_to_visible_time_recorder_.TabWasHidden();
}

void DelegatedFrameHostAndroid::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  NOTREACHED_IN_MIGRATION();
}

void DelegatedFrameHostAndroid::OnFrameTokenChanged(
    uint32_t frame_token,
    base::TimeTicks activation_time) {
  client_->OnFrameTokenChanged(frame_token, activation_time);
}

viz::SurfaceId DelegatedFrameHostAndroid::SurfaceId() const {
  return viz::SurfaceId(frame_sink_id_, local_surface_id_);
}

void DelegatedFrameHostAndroid::SetLocalSurfaceId(
    const viz::LocalSurfaceId& local_surface_id) {
  local_surface_id_ = local_surface_id;
  client_->OnSurfaceIdChanged();
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

  // If we explicitly tell a BFCached View and its `DelegatedFrameHostAndroid`
  // to use a specific fallback, discard the preserved fallback for BFCache.
  // During the BFCache activation (`EmbedSurface`) we will be using the primary
  // surface's smallest ID as the fallback.
  bfcache_fallback_ =
      viz::ParentLocalSurfaceIdAllocator::InvalidLocalSurfaceId();

  // TODO(crbug.com/40278354): Investigate why on Android we use the
  // primary ID unconditionally, which is different on `DelegatedFrameHost`.
  content_layer_->SetOldestAcceptableFallback(
      other->content_layer_->surface_id().ToSmallestId());
}

void DelegatedFrameHostAndroid::DidNavigate() {
  first_local_surface_id_after_navigation_ = local_surface_id_;
}

void DelegatedFrameHostAndroid::DidNavigateMainFramePreCommit() {
  // We are navigating to a different page, so the current |local_surface_id_|
  // and the fallback option of |first_local_surface_id_after_navigation_| are
  // no longer valid, as they represent older content from a different source.
  //
  // Cache the current |local_surface_id_| so that if navigation fails we can
  // evict it when transitioning to becoming visible.
  //
  // If the current page enters BFCache, `pre_navigation_local_surface_id_` will
  // be restored as the primary `LocalSurfaceId` for this
  // `DelegatedFrameHostAndroid`.
  pre_navigation_local_surface_id_ = local_surface_id_;
  first_local_surface_id_after_navigation_ = viz::LocalSurfaceId();
  SetLocalSurfaceId(viz::LocalSurfaceId());
}

void DelegatedFrameHostAndroid::DidEnterBackForwardCache() {
  if (local_surface_id_.is_valid()) {
    // `EmbedSurface` can be called after `DidNavigateMainFramePreCommit` and
    // before `DidEnterBackForwardCache`. This can happen if there is an
    // on-going Hi-DPI capture on the old frame (see
    // `WebContentsFrameTracker::RenderFrameHostChanged()`).
    //
    // The `EmbedSurface` will invalidate `pre_navigation_local_surface_id_`. In
    // this case we shouldn't restore the `local_surface_id_` nor
    // `bfcache_fallback_`because the surface should embed the latest
    // `local_surface_id_`.
    CHECK(!pre_navigation_local_surface_id_.is_valid());
    CHECK(!bfcache_fallback_.is_valid());
  } else {
    SetLocalSurfaceId(pre_navigation_local_surface_id_);
    bfcache_fallback_ = pre_navigation_local_surface_id_;
    pre_navigation_local_surface_id_ = viz::LocalSurfaceId();
  }
}

void DelegatedFrameHostAndroid::
    PostRequestSuccessfulPresentationTimeForNextFrame(
        blink::mojom::RecordContentToVisibleTimeRequestPtr
            content_to_visible_time_request) {
  // Since we could receive multiple requests while awaiting
  // `registered_parent_compositor_` we merge them.
  auto request =
      ConsumeAndMergeRequests(std::move(content_to_visible_time_request_),
                              std::move(content_to_visible_time_request));

  if (!registered_parent_compositor_) {
    content_to_visible_time_request_ = std::move(request);
    return;
  }

  registered_parent_compositor_
      ->PostRequestSuccessfulPresentationTimeForNextFrame(
          content_to_visible_time_recorder_.TabWasShown(
              /*has_saved_frames=*/true, std::move(request)));
}

}  // namespace ui
