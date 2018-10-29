// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/wayland/wayland_surface_factory.h"

namespace ui {

namespace {

void WaitForFence(EGLDisplay display, EGLSyncKHR fence) {
  eglClientWaitSyncKHR(display, fence, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR,
                       EGL_FOREVER_KHR);
  eglDestroySyncKHR(display, fence);
}

}  // namespace

GbmSurfacelessWayland::GbmSurfacelessWayland(
    WaylandSurfaceFactory* surface_factory,
    gfx::AcceleratedWidget widget)
    : SurfacelessEGL(gfx::Size()),
      surface_factory_(surface_factory),
      widget_(widget),
      has_implicit_external_sync_(
          HasEGLExtension("EGL_ARM_implicit_external_sync")),
      weak_factory_(this) {
  surface_factory_->RegisterSurface(widget_, this);
  unsubmitted_frames_.push_back(std::make_unique<PendingFrame>());
}

void GbmSurfacelessWayland::QueueOverlayPlane(OverlayPlane plane) {
  planes_.push_back(std::move(plane));
}

bool GbmSurfacelessWayland::ScheduleOverlayPlane(
    int z_order,
    gfx::OverlayTransform transform,
    gl::GLImage* image,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  unsubmitted_frames_.back()->overlays.push_back(
      gl::GLSurfaceOverlay(z_order, transform, image, bounds_rect, crop_rect,
                           enable_blend, std::move(gpu_fence)));
  return true;
}

bool GbmSurfacelessWayland::IsOffscreen() {
  return false;
}

bool GbmSurfacelessWayland::SupportsPresentationCallback() {
  return true;
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
    const PresentationCallback& callback) {
  // The actual sub buffer handling is handled at higher layers.
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

void GbmSurfacelessWayland::SwapBuffersAsync(
    const SwapCompletionCallback& completion_callback,
    const PresentationCallback& presentation_callback) {
  TRACE_EVENT0("wayland", "GbmSurfacelessWayland::SwapBuffersAsync");
  // If last swap failed, don't try to schedule new ones.
  if (!last_swap_buffers_result_) {
    completion_callback.Run(gfx::SwapResult::SWAP_FAILED, nullptr);
    // Notify the caller, the buffer is never presented on a screen.
    presentation_callback.Run(gfx::PresentationFeedback::Failure());
    return;
  }

  // TODO(dcastagna): remove glFlush since eglImageFlushExternalEXT called on
  // the image should be enough (https://crbug.com/720045).
  glFlush();
  unsubmitted_frames_.back()->Flush();

  PendingFrame* frame = unsubmitted_frames_.back().get();
  frame->completion_callback = completion_callback;
  frame->presentation_callback = presentation_callback;
  unsubmitted_frames_.push_back(std::make_unique<PendingFrame>());

  // TODO: the following should be replaced by a per surface flush as it gets
  // implemented in GL drivers.
  EGLSyncKHR fence = InsertFence(has_implicit_external_sync_);
  if (!fence) {
    completion_callback.Run(gfx::SwapResult::SWAP_FAILED, nullptr);
    // Notify the caller, the buffer is never presented on a screen.
    presentation_callback.Run(gfx::PresentationFeedback::Failure());
    return;
  }

  base::OnceClosure fence_wait_task =
      base::BindOnce(&WaitForFence, GetDisplay(), fence);

  base::OnceClosure fence_retired_callback = base::BindOnce(
      &GbmSurfacelessWayland::FenceRetired, weak_factory_.GetWeakPtr(), frame);

  base::PostTaskWithTraitsAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      std::move(fence_wait_task), std::move(fence_retired_callback));
}

void GbmSurfacelessWayland::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    const SwapCompletionCallback& completion_callback,
    const PresentationCallback& presentation_callback) {
  PendingFrame* frame = unsubmitted_frames_.back().get();
  frame->damage_region_ = gfx::Rect(x, y, width, height);

  SwapBuffersAsync(completion_callback, presentation_callback);
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

GbmSurfacelessWayland::~GbmSurfacelessWayland() {
  surface_factory_->UnregisterSurface(widget_);
}

GbmSurfacelessWayland::PendingFrame::PendingFrame() {}

GbmSurfacelessWayland::PendingFrame::~PendingFrame() {}

bool GbmSurfacelessWayland::PendingFrame::ScheduleOverlayPlanes(
    gfx::AcceleratedWidget widget) {
  for (auto& overlay : overlays)
    if (!overlay.ScheduleOverlayPlane(widget))
      return false;
  return true;
}

void GbmSurfacelessWayland::PendingFrame::Flush() {
  for (const auto& overlay : overlays)
    overlay.Flush();
}

void GbmSurfacelessWayland::SubmitFrame() {
  DCHECK(!unsubmitted_frames_.empty());

  if (unsubmitted_frames_.front()->ready && !submitted_frame_) {
    submitted_frame_ = std::move(unsubmitted_frames_.front());
    unsubmitted_frames_.erase(unsubmitted_frames_.begin());

    bool schedule_planes_succeeded =
        submitted_frame_->ScheduleOverlayPlanes(widget_);

    if (!schedule_planes_succeeded) {
      OnSubmission(gfx::SwapResult::SWAP_FAILED, nullptr);
      OnPresentation(gfx::PresentationFeedback::Failure());
      return;
    }

    auto callback =
        base::BindOnce(&GbmSurfacelessWayland::OnScheduleBufferSwapDone,
                       weak_factory_.GetWeakPtr());
    uint32_t buffer_id = planes_.back().pixmap->GetUniqueId();
    surface_factory_->ScheduleBufferSwap(widget_, buffer_id,
                                         submitted_frame_->damage_region_,
                                         std::move(callback));
  }
}

EGLSyncKHR GbmSurfacelessWayland::InsertFence(bool implicit) {
  const EGLint attrib_list[] = {EGL_SYNC_CONDITION_KHR,
                                EGL_SYNC_PRIOR_COMMANDS_IMPLICIT_EXTERNAL_ARM,
                                EGL_NONE};
  return eglCreateSyncKHR(GetDisplay(), EGL_SYNC_FENCE_KHR,
                          implicit ? attrib_list : NULL);
}

void GbmSurfacelessWayland::FenceRetired(PendingFrame* frame) {
  frame->ready = true;
  SubmitFrame();
}

void GbmSurfacelessWayland::OnScheduleBufferSwapDone(
    gfx::SwapResult result,
    const gfx::PresentationFeedback& feedback) {
  OnSubmission(result, nullptr);
  OnPresentation(feedback);
  planes_.clear();
}

void GbmSurfacelessWayland::OnSubmission(
    gfx::SwapResult result,
    std::unique_ptr<gfx::GpuFence> out_fence) {
  submitted_frame_->swap_result = result;
}

void GbmSurfacelessWayland::OnPresentation(
    const gfx::PresentationFeedback& feedback) {
  // Explicitly destroy overlays to free resources (e.g., fences) early.
  submitted_frame_->overlays.clear();

  gfx::SwapResult result = submitted_frame_->swap_result;
  std::move(submitted_frame_->completion_callback).Run(result, nullptr);
  std::move(submitted_frame_->presentation_callback).Run(feedback);
  submitted_frame_.reset();

  if (result == gfx::SwapResult::SWAP_FAILED) {
    last_swap_buffers_result_ = false;
    return;
  }

  SubmitFrame();
}

}  // namespace ui
