// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_surfaceless.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gl/gl_bindings.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"

#if BUILDFLAG(USE_OPENGL_APITRACE)
#include "ui/gl/gl_implementation.h"
#endif

namespace ui {

namespace {

void WaitForFence(EGLDisplay display, EGLSyncKHR fence) {
  eglClientWaitSyncKHR(display, fence, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR,
                       EGL_FOREVER_KHR);
  eglDestroySyncKHR(display, fence);
}

}  // namespace

GbmSurfaceless::GbmSurfaceless(GbmSurfaceFactory* surface_factory,
                               gl::GLDisplayEGL* display,
                               std::unique_ptr<DrmWindowProxy> window,
                               gfx::AcceleratedWidget widget)
    : surface_factory_(surface_factory),
      window_(std::move(window)),
      widget_(widget),
      display_(display) {
  surface_factory_->RegisterSurface(window_->widget(), this);
  supports_plane_gpu_fences_ = window_->SupportsGpuFences();
  unsubmitted_frames_.push_back(std::make_unique<PendingFrame>());
}

void GbmSurfaceless::QueueOverlayPlane(DrmOverlayPlane plane) {
  is_on_external_drm_device_ = !plane.buffer->drm_device()->is_primary_device();
  planes_.push_back(std::move(plane));
}

bool GbmSurfaceless::ScheduleOverlayPlane(
    gl::OverlayImage image,
    std::unique_ptr<gfx::GpuFence> gpu_fence,
    const gfx::OverlayPlaneData& overlay_plane_data) {
  unsubmitted_frames_.back()->overlays.emplace_back(
      std::move(image), std::move(gpu_fence), overlay_plane_data);
  return true;
}

bool GbmSurfaceless::Resize(const gfx::Size& size,
                            float scale_factor,
                            const gfx::ColorSpace& color_space,
                            bool has_alpha) {
  return true;
}

bool GbmSurfaceless::SupportsPlaneGpuFences() const {
  return supports_plane_gpu_fences_;
}

void GbmSurfaceless::Present(SwapCompletionCallback completion_callback,
                             PresentationCallback presentation_callback,
                             gfx::FrameData data) {
  TRACE_EVENT0("drm", "GbmSurfaceless::Present");
  // If last swap failed, don't try to schedule new ones.
  if (!last_swap_buffers_result_) {
    std::move(completion_callback)
        .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_FAILED));
    // Notify the caller, the buffer is never presented on a screen.
    std::move(presentation_callback).Run(gfx::PresentationFeedback::Failure());
    return;
  }

  if (!supports_plane_gpu_fences_) {
    glFlush();
  }

#if BUILDFLAG(USE_OPENGL_APITRACE)
  gl::TerminateFrame();  // Notify end of frame at buffer swap request.
#endif

  PendingFrame* frame = unsubmitted_frames_.back().get();
  frame->completion_callback = std::move(completion_callback);
  frame->presentation_callback = std::move(presentation_callback);
  unsubmitted_frames_.push_back(std::make_unique<PendingFrame>());

  // TODO(dcastagna): Remove the following workaround once we get explicit sync
  // on all Intel boards, currently we don't have it on legacy KMS.
  // We can not rely on implicit sync on external devices (crbug.com/692508).
  // NOTE: When on internal devices, |is_on_external_drm_device_| is set to true
  // by default conservatively, and it is correctly computed after the first
  // plane is enqueued in QueueOverlayPlane, that is called from
  // GbmSurfaceless::SubmitFrame.
  // This means |is_on_external_drm_device_| could be incorrectly set to true
  // the first time we're testing it.
  if (supports_plane_gpu_fences_ ||
      (!use_egl_fence_sync_ && !is_on_external_drm_device_)) {
    frame->ready = true;
    SubmitFrame();
    return;
  }

  // TODO: the following should be replaced by a per surface flush as it gets
  // implemented in GL drivers.
  EGLSyncKHR fence = InsertFence();
  CHECK_NE(fence, EGL_NO_SYNC_KHR) << "eglCreateSyncKHR failed";

  base::OnceClosure fence_wait_task =
      base::BindOnce(&WaitForFence, GetEGLDisplay(), fence);

  base::OnceClosure fence_retired_callback = base::BindOnce(
      &GbmSurfaceless::FenceRetired, weak_factory_.GetWeakPtr(), frame);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      std::move(fence_wait_task), std::move(fence_retired_callback));
}

void GbmSurfaceless::SetRelyOnImplicitSync() {
  use_egl_fence_sync_ = false;
}

void GbmSurfaceless::SetNotifyNonSimpleOverlayFailure() {
  notify_non_simple_overlay_failure_ = true;
}

GbmSurfaceless::~GbmSurfaceless() {
  surface_factory_->UnregisterSurface(window_->widget());
}

GbmSurfaceless::PendingFrame::PendingFrame() = default;

GbmSurfaceless::PendingFrame::~PendingFrame() = default;

bool GbmSurfaceless::PendingFrame::ScheduleOverlayPlanes(
    gfx::AcceleratedWidget widget) {
  for (auto& overlay : overlays)
    if (!overlay.ScheduleOverlayPlane(widget))
      return false;
  return true;
}

void GbmSurfaceless::SubmitFrame() {
  DCHECK(!unsubmitted_frames_.empty());

  if (unsubmitted_frames_.front()->ready && !submitted_frame_) {
    bool should_handle_non_simple_overlay_failure = false;
    for (auto& overlay : unsubmitted_frames_.front()->overlays) {
      if (overlay.z_order() == 0 && overlay.gpu_fence()) {
        submitted_frame_gpu_fence_ = std::make_unique<gfx::GpuFence>(
            overlay.gpu_fence()->GetGpuFenceHandle().Clone());
      }

      // At the moment, only fullscreen overlays are treated in a special way.
      // if other types of overlays also need special handling, then also
      // update the DrmOverlayManager, which handles that.
      if (overlay.overlay_type() == gfx::OverlayType::kFullScreen &&
          should_handle_non_simple_overlay_failure) {
        should_handle_non_simple_overlay_failure = true;
        break;
      }
    }
    submitted_frame_ = std::move(unsubmitted_frames_.front());
    unsubmitted_frames_.erase(unsubmitted_frames_.begin());

    bool schedule_planes_succeeded =
        submitted_frame_->ScheduleOverlayPlanes(widget_);

    if (!schedule_planes_succeeded) {
      OnSubmission(should_handle_non_simple_overlay_failure,
                   gfx::SwapResult::SWAP_FAILED,
                   /*release_fence=*/gfx::GpuFenceHandle());
      OnPresentation(gfx::PresentationFeedback::Failure());
      return;
    }

    window_->SchedulePageFlip(
        std::move(planes_),
        base::BindOnce(&GbmSurfaceless::OnSubmission,
                       weak_factory_.GetWeakPtr(),
                       should_handle_non_simple_overlay_failure),
        base::BindOnce(&GbmSurfaceless::OnPresentation,
                       weak_factory_.GetWeakPtr()));
    planes_.clear();
  }
}

EGLSyncKHR GbmSurfaceless::InsertFence() {
  const bool has_global_fence = display_->ext->b_EGL_ANGLE_global_fence_sync;
  const bool has_implicit_external_fence =
      display_->ext->b_EGL_ARM_implicit_external_sync;

  // Prefer EGL_ANGLE_global_fence_sync as it guarantees synchronization with
  // past submissions from all contexts, rather than the current context.
  const EGLenum syncType =
      has_global_fence ? EGL_SYNC_GLOBAL_FENCE_ANGLE : EGL_SYNC_FENCE_KHR;
  const bool use_implicit_external_sync =
      has_implicit_external_fence && !has_global_fence;
  const EGLint attrib_list[] = {EGL_SYNC_CONDITION_KHR,
                                EGL_SYNC_PRIOR_COMMANDS_IMPLICIT_EXTERNAL_ARM,
                                EGL_NONE};

  return eglCreateSyncKHR(GetEGLDisplay(), syncType,
                          use_implicit_external_sync ? attrib_list : nullptr);
}

void GbmSurfaceless::FenceRetired(PendingFrame* frame) {
  frame->ready = true;
  SubmitFrame();
}

void GbmSurfaceless::OnSubmission(bool should_handle_fullscreen_overlay_failure,
                                  gfx::SwapResult result,
                                  gfx::GpuFenceHandle release_fence) {
  // Handling fullscreen overlays' failures means usage of the
  // gfx::SwapResult::SWAP_NON_SIMPLE_OVERLAYS_FAILED. That way, viz is able to
  // recover from the error. The gpu watchdog will reset as viz will either
  // reschedule the same frame or it'll send a new one if it's already queued.
  if (should_handle_fullscreen_overlay_failure &&
      result == gfx::SwapResult::SWAP_FAILED) {
    result = gfx::SwapResult::SWAP_NON_SIMPLE_OVERLAYS_FAILED;
  }
  submitted_frame_->swap_result = result;
  if (!release_fence.is_null()) {
    std::move(submitted_frame_->completion_callback)
        .Run(gfx::SwapCompletionResult(result, std::move(release_fence)));
  }
}

void GbmSurfaceless::OnPresentation(const gfx::PresentationFeedback& feedback) {
  gfx::PresentationFeedback feedback_copy = feedback;

  if (submitted_frame_gpu_fence_ && !feedback.failed()) {
    feedback_copy.ready_timestamp =
        submitted_frame_gpu_fence_->GetMaxTimestamp();
  }
  submitted_frame_gpu_fence_.reset();
  submitted_frame_->overlays.clear();

  gfx::SwapResult result = submitted_frame_->swap_result;
  if (submitted_frame_->completion_callback)
    std::move(submitted_frame_->completion_callback)
        .Run(gfx::SwapCompletionResult(result));
  std::move(submitted_frame_->presentation_callback).Run(feedback_copy);
  submitted_frame_.reset();

  if (result == gfx::SwapResult::SWAP_FAILED) {
    last_swap_buffers_result_ = false;
    return;
  }

  SubmitFrame();
}

EGLDisplay GbmSurfaceless::GetEGLDisplay() {
  return display_->GetDisplay();
}

}  // namespace ui
