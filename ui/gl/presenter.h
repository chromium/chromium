// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_PRESENTER_H_
#define UI_GL_PRESENTER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/gfx/delegated_ink_metadata.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_export.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/gfx/native_pixmap.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "ui/gfx/mac/io_surface.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

namespace gfx {
namespace mojom {
class DelegatedInkPointRenderer;
}  // namespace mojom
class ColorSpace;
class GpuFence;
struct OverlayPlaneData;
}  // namespace gfx

namespace ui {
struct CARendererLayerParams;
}  // namespace ui

namespace gl {
struct DCLayerOverlayParams;

// OverlayImage is a platform specific type for overlay plane image data.
#if BUILDFLAG(IS_OZONE)
using OverlayImage = scoped_refptr<gfx::NativePixmap>;
#elif BUILDFLAG(IS_APPLE)
using OverlayImage = gfx::ScopedIOSurface;
#elif BUILDFLAG(IS_ANDROID)
using OverlayImage =
    std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>;
#else
struct OverlayImage {};
#endif

// Provides an abstraction around system api for presentation of the overlay
// planes and control presentations parameters (e.g frame rate). Temporarily is
// in ui/gl. Once its GL deps are resolved, it will be moved to ui/gfx.
class GL_EXPORT Presenter : public base::RefCounted<Presenter> {
 public:
  // Called not earlier than when the buffers from corresponding Present() call
  // are visible on screen. Note, that on some platforms there is no exact
  // signal of when the frame is on screen or presenter has to poll some system
  // flag, so this callback can come back reasonably later and should be used
  // only for metrics purpose.
  using PresentationCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback& feedback)>;
  // Called when the system compositor or display controller latched new buffers
  // and they are going to be displayed at next scanout.
  using SwapCompletionCallback =
      base::OnceCallback<void(gfx::SwapCompletionResult)>;

  Presenter();

  virtual bool SupportsOverridePlatformSize() const;
  virtual bool SupportsViewporter() const;
  virtual bool SupportsPlaneGpuFences() const;

  virtual void SetVSyncDisplayID(int64_t display_id) {}

  // Resizes the presenter, returning success.
  virtual bool Resize(const gfx::Size& size,
                      float scale_factor,
                      const gfx::ColorSpace& color_space,
                      bool has_alpha);

  // Schedule an overlay plane to be shown on the next Present() call. Should be
  // called for each overlay plane that is present in a frame before
  // the corresponding Present() call. Note, that this method doesn't put
  // anything on screen by itself, it's just adds the overlay to the current
  // frame, presentation of all planes is handled atomically by Present().
  // |image| to be presented by the overlay. |gpu_fence| is a fence for display
  // controller to wait before present. |overlay_plane_data| specifies overlay
  // data such as opacity, z_order, size, etc.
  virtual bool ScheduleOverlayPlane(
      OverlayImage image,
      std::unique_ptr<gfx::GpuFence> gpu_fence,
      const gfx::OverlayPlaneData& overlay_plane_data);

  // Schedule a CALayer to be shown at next Present(). Semantics is similar to
  // ScheduleOverlayPlane() above. All arguments correspond to their CALayer
  // properties.
  virtual bool ScheduleCALayer(const ui::CARendererLayerParams& params);

  // Schedule a DCLayer to be shown at next Present(). Semantics is similar to
  // ScheduleOverlayPlane() above. All arguments correspond to their DCLayer
  // properties.
  virtual void ScheduleDCLayer(std::unique_ptr<DCLayerOverlayParams> params);

  // Presents current frame asynchronously. `completion_callback` will be called
  // once all necessary steps were taken to display the frame.
  // `presentation_callback` will be called once frame was displayed and
  // presentation feedback was collected.
  virtual void Present(SwapCompletionCallback completion_callback,
                       PresentationCallback presentation_callback,
                       gfx::FrameData data) = 0;

#if BUILDFLAG(IS_APPLE)
  virtual void SetMaxPendingSwaps(int max_pending_swaps) {}
#endif

  // Sets preferred frame rate
  virtual void SetFrameRate(float frame_rate) {}

  // Android specific. Sets vsync_id of the corresponding Choreographer frame.
  virtual void SetChoreographerVsyncIdForNextFrame(
      std::optional<int64_t> choreographer_vsync_id) {}

  // Android specific. Request to not clean-up surface control tree, relying on
  // Android to do so after app switching animation is done.
  virtual void PreserveChildSurfaceControls() {}

#if BUILDFLAG(IS_WIN)
  virtual bool SupportsDelegatedInk() = 0;
  virtual void SetDelegatedInkTrailStartPoint(
      std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {}
  virtual void InitDelegatedInkPointRendererReceiver(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
          pending_receiver) {}
  virtual HWND GetWindow() const = 0;
#endif

  // Tells the presenter to rely on implicit sync when presenting buffers.
  virtual void SetRelyOnImplicitSync() {}

  // Tells the presenter to send
  // gfx::SwapResult::SWAP_NON_SIMPLE_OVERLAYS_FAILED if a non-simple overlay
  // submission fails (see gfx::OverlayType).
  virtual void SetNotifyNonSimpleOverlayFailure() {}

 protected:
  friend class base::RefCounted<Presenter>;
  virtual ~Presenter();
};

}  // namespace gl

#endif  // UI_GL_PRESENTER_H_
