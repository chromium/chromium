// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"

#include <memory>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gl/gl_bindings.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/public/mojom/wayland/wayland_overlay_config.mojom.h"

namespace ui {

namespace {

void WaitForGpuFences(std::vector<std::unique_ptr<gfx::GpuFence>> fences) {
  for (auto& fence : fences)
    fence->Wait();
}

}  // namespace

GbmSurfacelessWayland::GbmSurfacelessWayland(
    WaylandBufferManagerGpu* buffer_manager,
    gfx::AcceleratedWidget widget)
    : SurfacelessEGL(gfx::Size()),
      buffer_manager_(buffer_manager),
      widget_(widget),
      has_implicit_external_sync_(
          HasEGLExtension("EGL_ARM_implicit_external_sync")),
      weak_factory_(this) {
  buffer_manager_->RegisterSurface(widget_, this);
  unsubmitted_frames_.push_back(std::make_unique<PendingFrame>());
}

void GbmSurfacelessWayland::QueueOverlayPlane(OverlayPlane plane,
                                              BufferId buffer_id) {
  auto result =
      unsubmitted_frames_.back()->planes.emplace(buffer_id, std::move(plane));
  DCHECK(result.second);
}

bool GbmSurfacelessWayland::ScheduleOverlayPlane(
    int z_order,
    gfx::OverlayTransform transform,
    gl::GLImage* image,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  unsubmitted_frames_.back()->overlays.emplace_back(
      z_order, transform, image, bounds_rect, crop_rect, enable_blend,
      std::move(gpu_fence));
  return true;
}

bool GbmSurfacelessWayland::IsOffscreen() {
  return false;
}

bool GbmSurfacelessWayland::SupportsAsyncSwap() {
  return true;
}

bool GbmSurfacelessWayland::SupportsPostSubBuffer() {
  return true;
}

gfx::SwapResult GbmSurfacelessWayland::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    PresentationCallback callback) {
  // The actual sub buffer handling is handled at higher layers.
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

void GbmSurfacelessWayland::SwapBuffersAsync(
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback) {
  TRACE_EVENT0("wayland", "GbmSurfacelessWayland::SwapBuffersAsync");
  // If last swap failed, don't try to schedule new ones.
  if (!last_swap_buffers_result_) {
    std::move(completion_callback)
        .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_FAILED));
    // Notify the caller, the buffer is never presented on a screen.
    std::move(presentation_callback).Run(gfx::PresentationFeedback::Failure());
    return;
  }

  // TODO(fangzhoug): remove glFlush since eglImageFlushExternalEXT called on
  // the image should be enough (https://crbug.com/720045).
  if (!no_gl_flush_for_tests_)
    glFlush();
  unsubmitted_frames_.back()->Flush();

  PendingFrame* frame = unsubmitted_frames_.back().get();
  frame->completion_callback = std::move(completion_callback);
  frame->presentation_callback = std::move(presentation_callback);
  frame->ScheduleOverlayPlanes(widget_);

  unsubmitted_frames_.push_back(std::make_unique<PendingFrame>());

  // If Wayland server supports linux_explicit_synchronization_protocol, fences
  // should be shipped with buffers. Otherwise, we will wait for fences.
  if (buffer_manager_->supports_acquire_fence() || !use_egl_fence_sync_ ||
      !frame->schedule_planes_succeeded) {
    frame->ready = true;
    MaybeSubmitFrames();
    return;
  }

  base::OnceClosure fence_wait_task;
  std::vector<std::unique_ptr<gfx::GpuFence>> fences;
  for (auto& plane : frame->planes) {
    if (plane.second.gpu_fence)
      fences.push_back(std::move(plane.second.gpu_fence));
  }

  fence_wait_task = base::BindOnce(&WaitForGpuFences, std::move(fences));

  base::OnceClosure fence_retired_callback = base::BindOnce(
      &GbmSurfacelessWayland::FenceRetired, weak_factory_.GetWeakPtr(), frame);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      std::move(fence_wait_task), std::move(fence_retired_callback));
}

void GbmSurfacelessWayland::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback) {
  PendingFrame* frame = unsubmitted_frames_.back().get();
  frame->damage_region_ = gfx::Rect(x, y, width, height);

  SwapBuffersAsync(std::move(completion_callback),
                   std::move(presentation_callback));
}

EGLConfig GbmSurfacelessWayland::GetConfig() {
  if (!config_) {
    EGLint config_attribs[] = {EGL_BUFFER_SIZE,
                               32,
                               EGL_ALPHA_SIZE,
                               8,
                               EGL_BLUE_SIZE,
                               8,
                               EGL_GREEN_SIZE,
                               8,
                               EGL_RED_SIZE,
                               8,
                               EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES2_BIT,
                               EGL_SURFACE_TYPE,
                               EGL_DONT_CARE,
                               EGL_NONE};
    config_ = ChooseEGLConfig(GetDisplay(), config_attribs);
  }
  return config_;
}

void GbmSurfacelessWayland::SetRelyOnImplicitSync() {
  use_egl_fence_sync_ = false;
}

bool GbmSurfacelessWayland::SupportsPlaneGpuFences() const {
  return true;
}

bool GbmSurfacelessWayland::SupportsOverridePlatformSize() const {
  return true;
}

gfx::SurfaceOrigin GbmSurfacelessWayland::GetOrigin() const {
  // GbmSurfacelessWayland's y-axis is flipped compare to GL - (0,0) is at top
  // left corner.
  return gfx::SurfaceOrigin::kTopLeft;
}

GbmSurfacelessWayland::~GbmSurfacelessWayland() {
  buffer_manager_->UnregisterSurface(widget_);
}

GbmSurfacelessWayland::PendingFrame::PendingFrame() = default;

GbmSurfacelessWayland::PendingFrame::~PendingFrame() = default;

void GbmSurfacelessWayland::PendingFrame::ScheduleOverlayPlanes(
    gfx::AcceleratedWidget widget) {
  for (auto& overlay : overlays) {
    if (!overlay.ScheduleOverlayPlane(widget))
      return;
  }
  schedule_planes_succeeded = true;
  return;
}

void GbmSurfacelessWayland::PendingFrame::Flush() {
  for (const auto& overlay : overlays)
    overlay.Flush();
}

void GbmSurfacelessWayland::MaybeSubmitFrames() {
  while (!unsubmitted_frames_.empty() && unsubmitted_frames_.front()->ready) {
    auto submitted_frame = std::move(unsubmitted_frames_.front());
    unsubmitted_frames_.erase(unsubmitted_frames_.begin());

    if (!submitted_frame->schedule_planes_succeeded) {
      last_swap_buffers_result_ = false;

      std::move(submitted_frame->completion_callback)
          .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_FAILED));
      // Notify the caller, the buffer is never presented on a screen.
      std::move(submitted_frame->presentation_callback)
          .Run(gfx::PresentationFeedback::Failure());

      submitted_frame.reset();
      return;
    }

    std::vector<ui::ozone::mojom::WaylandOverlayConfigPtr> overlay_configs;
    for (auto& plane : submitted_frame->planes) {
      overlay_configs.push_back(
          ui::ozone::mojom::WaylandOverlayConfig::From(plane.second));
      overlay_configs.back()->buffer_id = plane.first;
      if (plane.second.z_order == 0)
        overlay_configs.back()->damage_region = submitted_frame->damage_region_;
#if DCHECK_IS_ON()
      if (plane.second.z_order == INT32_MIN)
        background_buffer_id_ = plane.first;
#endif
      plane.second.gpu_fence.reset();
    }

    buffer_manager_->CommitOverlays(widget_, std::move(overlay_configs));
    submitted_frames_.push_back(std::move(submitted_frame));
  }
}

EGLSyncKHR GbmSurfacelessWayland::InsertFence(bool implicit) {
  const EGLint attrib_list[] = {EGL_SYNC_CONDITION_KHR,
                                EGL_SYNC_PRIOR_COMMANDS_IMPLICIT_EXTERNAL_ARM,
                                EGL_NONE};
  return eglCreateSyncKHR(GetDisplay(), EGL_SYNC_FENCE_KHR,
                          implicit ? attrib_list : nullptr);
}

void GbmSurfacelessWayland::FenceRetired(PendingFrame* frame) {
  frame->ready = true;
  MaybeSubmitFrames();
}

void GbmSurfacelessWayland::SetNoGLFlushForTests() {
  no_gl_flush_for_tests_ = true;
}

void GbmSurfacelessWayland::OnSubmission(BufferId buffer_id,
                                         const gfx::SwapResult& swap_result) {
  // submitted_frames_ may temporarily have more than one buffer in it if
  // buffers are released out of order by the Wayland server.
  DCHECK(!submitted_frames_.empty() || background_buffer_id_ == buffer_id);

  size_t erased = 0;
  for (auto& submitted_frame : submitted_frames_) {
    if ((erased = submitted_frame->planes.erase(buffer_id)) > 0) {
      // |completion_callback| only takes 1 SwapResult. It's possible that only
      // one of the buffers in a frame gets a SWAP_FAILED or
      // SWAP_NAK_RECREATE_BUFFERS. Don't replace a failed swap_result with
      // SWAP_ACK. If both SWAP_FAILED and SWAP_NAK_RECREATE_BUFFERS happens,
      // this swap is treated as SWAP_FAILED.
      if (submitted_frame->swap_result == gfx::SwapResult::SWAP_ACK ||
          swap_result == gfx::SwapResult::SWAP_FAILED) {
        submitted_frame->swap_result = swap_result;
      }
      submitted_frame->pending_presentation_buffers.insert(buffer_id);
      break;
    }
  }

  // Following while loop covers below scenario:
  //   frame_1 submitted a buffer_1 for overlay; frame_2 submitted a buffer_2
  //   for primary plane. This can happen at the end of a single-on-top overlay.
  //   buffer_1 is not attached immediately due to unack'ed wl_frame_callback.
  //   buffer_2 is attached immediately Onsubmission() of buffer_2 runs.
  while (!submitted_frames_.empty() &&
         submitted_frames_.front()->planes.empty()) {
    auto submitted_frame = std::move(submitted_frames_.front());
    submitted_frames_.erase(submitted_frames_.begin());
    submitted_frame->overlays.clear();

    std::move(submitted_frame->completion_callback)
        .Run(gfx::SwapCompletionResult(submitted_frame->swap_result));

    pending_presentation_frames_.push_back(std::move(submitted_frame));
  }

  if (swap_result != gfx::SwapResult::SWAP_ACK) {
    last_swap_buffers_result_ = false;
    return;
  }

  MaybeSubmitFrames();
}

void GbmSurfacelessWayland::OnPresentation(
    BufferId buffer_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(!submitted_frames_.empty() || !pending_presentation_frames_.empty() ||
         background_buffer_id_ == buffer_id);

  size_t erased = 0;
  for (auto& frame : pending_presentation_frames_) {
    if ((erased = frame->pending_presentation_buffers.erase(buffer_id)) > 0) {
      frame->feedback = feedback;
      break;
    }
  }

  // Items in |submitted_frames_| will not be moved to
  // |pending_presentation_frames_| until |planes| is empty.
  // Example:
  //    A SwapBuffers that submitted 2 buffers (buffer_1 and buffer_2) will push
  //    a submitted_frame expecting 2 submission feedbacks and 2 presentation
  //    feedbacks.
  //    If IPCs comes in the order of:
  //      buffer_1:submission > buffer_2:submission > buffer_1:presentation >
  //      buffer_2:presentation
  //    We are fine without below logic. However, this can happen:
  //      buffer_1:submission > buffer_1:presentation > buffer_2:submission >
  //      buffer_2:presentation
  //    In this case, we have to find the item in |submitted_frames_| and
  //    remove from |pending_presentation_buffers| there.
  if (!erased) {
    for (auto& frame : submitted_frames_) {
      if ((erased = frame->pending_presentation_buffers.erase(buffer_id)) > 0) {
        frame->feedback = feedback;
        break;
      }
    }
  }

  while (!pending_presentation_frames_.empty() &&
         pending_presentation_frames_.front()
             ->pending_presentation_buffers.empty()) {
    auto* frame = pending_presentation_frames_.front().get();
    DCHECK(frame->planes.empty());
    std::move(frame->presentation_callback).Run(frame->feedback);
    pending_presentation_frames_.erase(pending_presentation_frames_.begin());
  }
}

}  // namespace ui
