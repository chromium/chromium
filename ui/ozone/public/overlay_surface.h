// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OVERLAY_SURFACE_H_
#define UI_OZONE_PUBLIC_OVERLAY_SURFACE_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"

namespace ui {

struct OverlayPlane;

// An overlay surface is similar to a surface, but natively uses overlays and
// does not internally allocate any buffers.
class COMPONENT_EXPORT(OZONE_BASE) OverlaySurface {
 public:
  OverlaySurface();
  virtual ~OverlaySurface();

  // Called with the swap result once the frame is submitted.
  //
  // If the swap result is gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS, there has
  // been a configuration change that necessitates re-allocation of buffers by
  // the client (and re-testing of overlay configurations).
  using SubmissionCallback = base::OnceCallback<void(gfx::SwapResult)>;

  // Called when the frame has been presented.
  //
  // Buffers are presented in the same order as they are submitted.
  using PresentationCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback& feedback)>;

  // Called when the buffers backing a frame can be reused for rendering.
  //
  // Buffers are released in the same order as they are submitted. The buffers
  // backing a frame will not generally be released until after a replacement
  // frame has been submitted.
  // TODO(spang): Find out if there's any benefit to using out fences here.
  using ReleaseCallback = base::OnceClosure;

  // Submits a new frame consisting of |overlay_planes|.
  //
  // The configuration of |overlay_planes| must have been validated prior to use
  // (see CheckOverlaySupport).
  //
  // Only one frame should be submitted at a time. Once the submission callback
  // is made, it is safe to call SubmitFrame() again. Rendered but unsubmitted
  // frames can be queued by the client if desired.
  //
  // The surface owns all buffers backing |overlay_planes| starting at the call
  // to SubmitFrame() and ending at the call to |release_callback|. Buffers must
  // not be re-used for rendering while owned by the surface.
  //
  // Each plane in |overlay_planes| can optionally carry a fence that signals
  // when writes to its backing buffers have completed. When fences are
  // provided, frames can be submitted before rendering completes and each
  // buffer will not be read from until its fence has been signaled.
  virtual void SubmitFrame(std::vector<OverlayPlane> overlay_planes,
                           SubmissionCallback submission_callback,
                           PresentationCallback presentation_callback,
                           ReleaseCallback release_callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(OverlaySurface);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OVERLAY_SURFACE_H_
