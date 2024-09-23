// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_DELEGATED_FRAME_HOST_ANDROID_H_
#define UI_ANDROID_DELEGATED_FRAME_HOST_ANDROID_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/layers/deadline_policy.h"
#include "components/viz/client/frame_evictor.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "third_party/blink/public/common/page/content_to_visible_time_reporter.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom.h"
#include "ui/android/ui_android_export.h"
#include "ui/android/window_android_compositor.h"

namespace cc::slim {
class SurfaceLayer;
}

namespace viz {
class HostFrameSinkManager;
}  // namespace viz

namespace ui {
class ViewAndroid;
class WindowAndroidCompositor;

class UI_ANDROID_EXPORT DelegatedFrameHostAndroid
    : public viz::HostFrameSinkClient,
      public viz::FrameEvictorClient {
 public:
  class Client : public WindowAndroidCompositor::FrameSubmissionObserver {
   public:
    ~Client() override {}
    virtual void OnFrameTokenChanged(uint32_t frame_token,
                                     base::TimeTicks activation_time) = 0;
    virtual void WasEvicted() = 0;
    virtual void OnSurfaceIdChanged() = 0;
    virtual std::vector<viz::SurfaceId> CollectSurfaceIdsForEviction()
        const = 0;
  };

  DelegatedFrameHostAndroid(ViewAndroid* view,
                            viz::HostFrameSinkManager* host_frame_sink_manager,
                            Client* client,
                            const viz::FrameSinkId& frame_sink_id);

  DelegatedFrameHostAndroid(const DelegatedFrameHostAndroid&) = delete;
  DelegatedFrameHostAndroid& operator=(const DelegatedFrameHostAndroid&) =
      delete;

  ~DelegatedFrameHostAndroid() override;

  static int64_t TimeDeltaToFrames(base::TimeDelta delta) {
    return base::ClampRound<int64_t>(delta /
                                     viz::BeginFrameArgs::DefaultInterval());
  }

  // Wait up to 5 seconds for the first frame to be produced. Having Android
  // display a placeholder for a longer period of time is preferable to drawing
  // nothing, and the first frame can take a while on low-end systems.
  static constexpr base::TimeDelta FirstFrameTimeout() {
    return base::Seconds(5);
  }
  static int64_t FirstFrameTimeoutFrames() {
    return TimeDeltaToFrames(FirstFrameTimeout());
  }

  // Wait up to 175 milliseconds for a frame of the correct size to be produced.
  // Android OS will only wait 200 milliseconds, so we limit this to make sure
  // that Viz is able to produce the latest frame from the Browser before the OS
  // stops waiting. Otherwise a rotated version of the previous frame will be
  // displayed with a large black region where there is no content yet.
  static constexpr base::TimeDelta ResizeTimeout() {
    return base::Milliseconds(175);
  }
  static int64_t ResizeTimeoutFrames() {
    return TimeDeltaToFrames(ResizeTimeout());
  }

  void ClearFallbackSurfaceForCommitPending();
  // Advances the fallback surface to the first surface after navigation. This
  // ensures that stale surfaces are not presented to the user for an indefinite
  // period of time.
  void ResetFallbackToFirstNavigationSurface();

  bool HasDelegatedContent() const;

  const cc::slim::SurfaceLayer* content_layer() const {
    return content_layer_.get();
  }

  const viz::FrameSinkId& GetFrameSinkId() const;

  // Should only be called when the host has a content layer. Use this for one-
  // off screen capture, not for video. Always provides ResultFormat::RGBA,
  // ResultDestination::kSystemMemory CopyOutputResults.
  // `capture_exact_surface_id` indicates if the `CopyOutputRequest` will be
  // issued against a specific surface or not.
  void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback,
      bool capture_exact_surface_id);
  bool CanCopyFromCompositingSurface() const;

  void CompositorFrameSinkChanged();

  // Called when this DFH is attached/detached from a parent browser compositor
  // and needs to be attached to the surface hierarchy.
  void AttachToCompositor(WindowAndroidCompositor* compositor);
  void DetachFromCompositor();

  bool IsPrimarySurfaceEvicted() const;
  bool HasSavedFrame() const;
  void WasHidden();
  void WasShown(const viz::LocalSurfaceId& local_surface_id,
                const gfx::Size& size_in_pixels,
                bool is_fullscreen,
                blink::mojom::RecordContentToVisibleTimeRequestPtr
                    content_to_visible_time_request);
  void EmbedSurface(const viz::LocalSurfaceId& new_local_surface_id,
                    const gfx::Size& new_size_in_pixels,
                    cc::DeadlinePolicy deadline_policy,
                    bool is_fullscreen);

  // Called to request the presentation time for the next frame or cancel any
  // requests when the RenderWidget's visibility state is not changing. If the
  // visibility state is changing call WasHidden or WasShown instead.
  void RequestSuccessfulPresentationTimeForNextFrame(
      blink::mojom::RecordContentToVisibleTimeRequestPtr
          content_to_visible_time_request);
  void CancelSuccessfulPresentationTimeRequest();

  // Returns the ID for the current Surface. Returns an invalid ID if no
  // surface exists (!HasDelegatedContent()).
  viz::SurfaceId SurfaceId() const;

  bool HasPrimarySurface() const;
  bool HasFallbackSurface() const;

  void TakeFallbackContentFrom(DelegatedFrameHostAndroid* other);

  // Called when navigation has completed, and this DelegatedFrameHost is
  // visible. A new Surface will have been embedded at this point. If navigation
  // is done while hidden, this will be called upon becoming visible.
  void DidNavigate();

  // Navigation to a different page than the current one has begun. This is
  // called regardless of the visibility of the page. Caches the current
  // LocalSurfaceId information so that old content can be evicted if
  // navigation fails to complete.
  void DidNavigateMainFramePreCommit();

  // Called when the page has just entered BFCache.
  void DidEnterBackForwardCache();

  viz::SurfaceId GetFallbackSurfaceIdForTesting() const;

  viz::SurfaceId GetCurrentSurfaceIdForTesting() const;

  viz::SurfaceId GetPreNavigationSurfaceIdForTesting() const {
    return GetPreNavigationSurfaceId();
  }

  viz::SurfaceId GetFirstSurfaceIdAfterNavigationForTesting() const;

  void SetIsFrameSinkIdOwner(bool is_owner);

  void RegisterOffsetTags(const cc::BrowserControlsOffsetTagsInfo& tags_info);
  void UnregisterOffsetTags(const cc::BrowserControlsOffsetTagsInfo& tags_info);

 private:
  // FrameEvictorClient implementation.
  void EvictDelegatedFrame(
      const std::vector<viz::SurfaceId>& surface_ids) override;
  viz::FrameEvictorClient::EvictIds CollectSurfaceIdsForEviction()
      const override;
  viz::SurfaceId GetCurrentSurfaceId() const override;
  viz::SurfaceId GetPreNavigationSurfaceId() const override;

  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token,
                           base::TimeTicks activation_time) override;

  void ProcessCopyOutputRequest(
      std::unique_ptr<viz::CopyOutputRequest> request);

  void SetLocalSurfaceId(const viz::LocalSurfaceId& local_surface_id);

  // We cannot guarantee to be attached to `registered_parent_compositor_` when
  // either WasShown or RequestSuccessfulPresentationTimeForNextFrame is called.
  // In such cases we enqueue the request and attempt again to send it once the
  // compositor has been attached.
  void PostRequestSuccessfulPresentationTimeForNextFrame(
      blink::mojom::RecordContentToVisibleTimeRequestPtr
          content_to_visible_time_request);

  const viz::FrameSinkId frame_sink_id_;

  raw_ptr<ViewAndroid> view_;

  const raw_ptr<viz::HostFrameSinkManager> host_frame_sink_manager_;
  raw_ptr<WindowAndroidCompositor> registered_parent_compositor_ = nullptr;
  raw_ptr<Client> client_;

  scoped_refptr<cc::slim::SurfaceLayer> content_layer_;

  // Whether we've received a frame from the renderer since navigating.
  // Only used when surface synchronization is on.
  viz::LocalSurfaceId first_local_surface_id_after_navigation_;

  // While navigating we have no active |local_surface_id_|. Track the one from
  // before a navigation, because if the navigation fails to complete, we will
  // need to evict its surface. If the old page enters BFCache, this id is used
  // to restore `local_surface_id_`.
  viz::LocalSurfaceId pre_navigation_local_surface_id_;

  // The fallback ID for BFCache restore. It is set when `this` enters the
  // BFCache and is cleared when resize-while-hidden (which supplies with a
  // latest fallback ID) or after it is used in `EmbedSurface`.
  viz::LocalSurfaceId bfcache_fallback_;

  // The LocalSurfaceId of the currently embedded surface. If surface sync is
  // on, this surface is not necessarily active.
  //
  // TODO(crbug.com/40274223): this value is a copy of what the browser
  // wants to embed. The source of truth is stored else where. We should
  // consider de-dup this ID.
  viz::LocalSurfaceId local_surface_id_;

  // The size of the above surface (updated at the same time).
  gfx::Size surface_size_in_pixels_;

  // If `registered_parent_compositor_` is not attached when we receive a
  // request, we save it and attempt again to send it once the compositor has
  // been attached.
  blink::mojom::RecordContentToVisibleTimeRequestPtr
      content_to_visible_time_request_;
  blink::ContentToVisibleTimeReporter content_to_visible_time_recorder_;

  std::unique_ptr<viz::FrameEvictor> frame_evictor_;

  // Speculative RenderWidgetHostViews can start with a FrameSinkId owned by the
  // currently committed RenderWidgetHostView. Ownership is transferred when the
  // navigation is committed. This bit tracks whether this
  // DelegatedFrameHostAndroid owns its FrameSinkId.
  bool owns_frame_sink_id_ = false;
};

}  // namespace ui

#endif  // UI_ANDROID_DELEGATED_FRAME_HOST_ANDROID_H_
