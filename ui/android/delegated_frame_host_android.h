// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_DELEGATED_FRAME_HOST_ANDROID_H_
#define UI_ANDROID_DELEGATED_FRAME_HOST_ANDROID_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/layers/deadline_policy.h"
#include "components/viz/client/frame_evictor.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "ui/android/ui_android_export.h"

namespace cc {
class SurfaceLayer;
enum class SurfaceDrawStatus;
}  // namespace cc

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
  class Client {
   public:
    virtual ~Client() {}
    virtual void OnFrameTokenChanged(uint32_t frame_token) = 0;
    virtual void WasEvicted() = 0;
  };

  DelegatedFrameHostAndroid(ViewAndroid* view,
                            viz::HostFrameSinkManager* host_frame_sink_manager,
                            Client* client,
                            const viz::FrameSinkId& frame_sink_id);

  ~DelegatedFrameHostAndroid() override;

  // Wait up to 5 seconds for the first frame to be produced. Having Android
  // display a placeholder for a longer period of time is preferable to drawing
  // nothing, and the first frame can take a while on low-end systems.
  static constexpr base::TimeDelta FirstFrameTimeout() {
    return base::TimeDelta::FromSeconds(5);
  }
  static constexpr int64_t FirstFrameTimeoutFrames() {
    return FirstFrameTimeout() / viz::BeginFrameArgs::DefaultInterval();
  }

  // Wait up to 1 second for a frame of the correct size to be produced. Android
  // OS will only wait 4 seconds, so we limit this to 1 second to make sure we
  // have always produced a frame before the OS stops waiting.
  static constexpr base::TimeDelta ResizeTimeout() {
    return base::TimeDelta::FromSeconds(1);
  }
  static constexpr int64_t ResizeTimeoutFrames() {
    return ResizeTimeout() / viz::BeginFrameArgs::DefaultInterval();
  }

  // FrameEvictorClient implementation.
  void EvictDelegatedFrame() override;

  // Advances the fallback surface to the first surface after navigation. This
  // ensures that stale surfaces are not presented to the user for an indefinite
  // period of time.
  void ResetFallbackToFirstNavigationSurface();

  bool HasDelegatedContent() const;

  cc::SurfaceLayer* content_layer_for_testing() { return content_layer_.get(); }

  const viz::FrameSinkId& GetFrameSinkId() const;

  // Should only be called when the host has a content layer. Use this for one-
  // off screen capture, not for video. Always provides RGBA_BITMAP
  // CopyOutputResults.
  void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback);
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
                const gfx::Size& size_in_pixels);
  void EmbedSurface(const viz::LocalSurfaceId& new_local_surface_id,
                    const gfx::Size& new_size_in_pixels,
                    cc::DeadlinePolicy deadline_policy);

  // Returns the ID for the current Surface. Returns an invalid ID if no
  // surface exists (!HasDelegatedContent()).
  viz::SurfaceId SurfaceId() const;

  bool HasPrimarySurface() const;
  bool HasFallbackSurface() const;

  void TakeFallbackContentFrom(DelegatedFrameHostAndroid* other);

  void DidNavigate();

 private:
  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token) override;

  void ProcessCopyOutputRequest(
      std::unique_ptr<viz::CopyOutputRequest> request);

  const viz::FrameSinkId frame_sink_id_;

  ViewAndroid* view_;

  viz::HostFrameSinkManager* const host_frame_sink_manager_;
  WindowAndroidCompositor* registered_parent_compositor_ = nullptr;
  Client* client_;

  scoped_refptr<cc::SurfaceLayer> content_layer_;

  // Whether we've received a frame from the renderer since navigating.
  // Only used when surface synchronization is on.
  viz::LocalSurfaceId first_local_surface_id_after_navigation_;

  // The LocalSurfaceId of the currently embedded surface. If surface sync is
  // on, this surface is not necessarily active.
  viz::LocalSurfaceId local_surface_id_;

  // The size of the above surface (updated at the same time).
  gfx::Size surface_size_in_pixels_;

  std::unique_ptr<viz::FrameEvictor> frame_evictor_;

  DISALLOW_COPY_AND_ASSIGN(DelegatedFrameHostAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_DELEGATED_FRAME_HOST_ANDROID_H_
