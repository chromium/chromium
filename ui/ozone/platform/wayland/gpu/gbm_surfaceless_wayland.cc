// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"

#include <sync/sync.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_display.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/mojom/wayland_overlay_config.mojom.h"

namespace ui {

namespace {

// A test run showed only 9 inflight solid color buffers at the same time. Thus,
// allow to store max 12 buffers (including some margin) of solid color buffers
// and remove the rest.
static constexpr size_t kMaxSolidColorBuffers = 12;

static constexpr gfx::Size kSolidColorBufferSize{4, 4};

void WaitForGpuFences(std::vector<std::unique_ptr<gfx::GpuFence>> fences) {
  for (auto& fence : fences)
    fence->Wait();
}

}  // namespace

GbmSurfacelessWayland::SolidColorBufferHolder::SolidColorBufferHolder() =
    default;
GbmSurfacelessWayland::SolidColorBufferHolder::~SolidColorBufferHolder() =
    default;

BufferId
GbmSurfacelessWayland::SolidColorBufferHolder::GetOrCreateSolidColorBuffer(
    SkColor4f color,
    WaylandBufferManagerGpu* buffer_manager) {
  BufferId next_buffer_id = 0;

  // First try for an existing buffer.
  auto it = base::ranges::find(available_solid_color_buffers_, color,
                               &SolidColorBuffer::color);
  if (it != available_solid_color_buffers_.end()) {
    // This is a prefect color match so use this directly.
    next_buffer_id = it->buffer_id;
    inflight_solid_color_buffers_.emplace_back(std::move(*it));
    available_solid_color_buffers_.erase(it);
  } else {
    // Worst case allocate a new buffer. This definitely will occur on
    // startup.
    next_buffer_id = buffer_manager->AllocateBufferID();
    // Create wl_buffer on the browser side.
    if (buffer_manager->supports_non_backed_solid_color_buffers()) {
      buffer_manager->CreateSolidColorBuffer(color, kSolidColorBufferSize,
                                             next_buffer_id);
    } else {
      CHECK(buffer_manager->supports_single_pixel_buffer());
      buffer_manager->CreateSinglePixelBuffer(color, next_buffer_id);
    }
    // Allocate a backing structure that will be used to figure out if such
    // buffer has already existed.
    inflight_solid_color_buffers_.emplace_back(
        SolidColorBuffer(color, next_buffer_id));
  }
  DCHECK_GT(next_buffer_id, 0u);
  return next_buffer_id;
}

void GbmSurfacelessWayland::SolidColorBufferHolder::OnSubmission(
    BufferId buffer_id,
    WaylandBufferManagerGpu* buffer_manager) {
  // Solid color buffers do not require on submission as skia doesn't track
  // them. Instead, they are tracked by GbmSurfacelessWayland. In the future,
  // when SharedImageFactory allows to create non-backed shared images, this
  // should be removed from here.
  auto it = base::ranges::find(inflight_solid_color_buffers_, buffer_id,
                               &SolidColorBuffer::buffer_id);
  if (it != inflight_solid_color_buffers_.end()) {
    available_solid_color_buffers_.emplace_back(std::move(*it));
    inflight_solid_color_buffers_.erase(it);
    // Keep track of the number of created buffers and erase the least used
    // ones until the maximum number of available solid color buffer.
    while (available_solid_color_buffers_.size() > kMaxSolidColorBuffers) {
      buffer_manager->DestroyBuffer(
          available_solid_color_buffers_.begin()->buffer_id);
      available_solid_color_buffers_.erase(
          available_solid_color_buffers_.begin());
    }
  }
}

void GbmSurfacelessWayland::SolidColorBufferHolder::EraseBuffers(
    WaylandBufferManagerGpu* buffer_manager) {
  for (const auto& buffer : available_solid_color_buffers_)
    buffer_manager->DestroyBuffer(buffer.buffer_id);
  available_solid_color_buffers_.clear();
}

GbmSurfacelessWayland::GbmSurfacelessWayland(
    gl::GLDisplayEGL* display,
    WaylandBufferManagerGpu* buffer_manager,
    gfx::AcceleratedWidget widget)
    : buffer_manager_(buffer_manager),
      widget_(widget),
      solid_color_buffers_holder_(std::make_unique<SolidColorBufferHolder>()),
      display_(display),
      weak_factory_(this) {
  buffer_manager_->RegisterSurface(widget_, this);
  unsubmitted_frames_.push_back(
      std::make_unique<PendingFrame>(next_frame_id()));
}

void GbmSurfacelessWayland::QueueWaylandOverlayConfig(
    wl::WaylandOverlayConfig config) {
  auto* frame = unsubmitted_frames_.back().get();
  DCHECK(frame);
  TRACE_EVENT("wayland", "GbmSurfacelessWayland::QueueWaylandOverlayConfig",
              "frame_id", frame->frame_id, "buffer_id", config.buffer_id);
  frame->configs.emplace_back(std::move(config));
}

bool GbmSurfacelessWayland::ScheduleOverlayPlane(
    gl::OverlayImage image,
    std::unique_ptr<gfx::GpuFence> gpu_fence,
    const gfx::OverlayPlaneData& overlay_plane_data) {
  auto* frame = unsubmitted_frames_.back().get();
  // There are multiple scheduling submissions for the same frame. If the
  // previous schedule failed, there is no reason to continue.
  if (!frame->schedule_planes_succeeded)
    return false;

  // Solid color overlays are non-backed. Thus, queue them directly.
  // TODO(msisov): reconsider this once Linux Wayland compositors also support
  // creation of non-backed solid color wl_buffers.
  if (!image) {
    // Only solid color overlays can be non-backed.
    if (!overlay_plane_data.is_solid_color) {
      LOG(WARNING) << "Only solid color overlay planes are allowed to be "
                      "scheduled without backing.";
      frame->schedule_planes_succeeded = false;
      return false;
    }
    DCHECK(!gpu_fence);

    BufferId buf_id = solid_color_buffers_holder_->GetOrCreateSolidColorBuffer(
        overlay_plane_data.color.value(), buffer_manager_);
    // Invalid buffer id.
    if (buf_id == 0) {
      frame->schedule_planes_succeeded = false;
      return false;
    }
    frame->in_flight_color_buffers.push_back(buf_id);
    QueueWaylandOverlayConfig(
        {overlay_plane_data, nullptr, buf_id, surface_scale_factor()});
  } else {
    std::vector<gfx::GpuFence> acquire_fences;
    if (gpu_fence &&
        (buffer_manager_->supports_acquire_fence() || use_egl_fence_sync_)) {
      acquire_fences.push_back(std::move(*gpu_fence));
    }

    frame->schedule_planes_succeeded = image->ScheduleOverlayPlane(
        widget_, overlay_plane_data, std::move(acquire_fences), {});
  }
  return frame->schedule_planes_succeeded;
}
void GbmSurfacelessWayland::Present(SwapCompletionCallback completion_callback,
                                    PresentationCallback presentation_callback,
                                    gfx::FrameData data) {
  TRACE_EVENT0("wayland", "GbmSurfacelessWayland::Present");
  // If last swap failed, don't try to schedule new ones.
  if (!last_swap_buffers_result_) {
    std::move(completion_callback)
        .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_FAILED));
    // Notify the caller, the buffer is never presented on a screen.
    std::move(presentation_callback).Run(gfx::PresentationFeedback::Failure());
    return;
  }

  if (!no_gl_flush_for_tests_ && !buffer_manager_->supports_acquire_fence()) {
    glFlush();
  }

  PendingFrame* frame = unsubmitted_frames_.back().get();
  frame->completion_callback = std::move(completion_callback);
  frame->presentation_callback = std::move(presentation_callback);
  frame->data = data;

  unsubmitted_frames_.push_back(
      std::make_unique<PendingFrame>(next_frame_id()));
  unsubmitted_frames_.back()->configs.reserve(frame->configs.size());
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
  for (auto& config : frame->configs) {
    if (!config.access_fence_handle.is_null()) {
      fences.push_back(std::make_unique<gfx::GpuFence>(
          std::move(config.access_fence_handle)));
      config.access_fence_handle = gfx::GpuFenceHandle();
    }
  }

  fence_wait_task = base::BindOnce(&WaitForGpuFences, std::move(fences));

  base::OnceClosure fence_retired_callback = base::BindOnce(
      &GbmSurfacelessWayland::FenceRetired, weak_factory_.GetWeakPtr(), frame);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      std::move(fence_wait_task), std::move(fence_retired_callback));
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

bool GbmSurfacelessWayland::SupportsViewporter() const {
  return buffer_manager_->supports_viewporter();
}

bool GbmSurfacelessWayland::Resize(const gfx::Size& size,
                                   float scale_factor,
                                   const gfx::ColorSpace& color_space,
                                   bool has_alpha) {
  surface_scale_factor_ = scale_factor;

  // Remove all the buffers.
  solid_color_buffers_holder_->EraseBuffers(buffer_manager_);

  return true;
}

GbmSurfacelessWayland::~GbmSurfacelessWayland() {
  buffer_manager_->UnregisterSurface(widget_);
}

GbmSurfacelessWayland::PendingFrame::PendingFrame(uint32_t frame_id)
    : frame_id(frame_id) {}

GbmSurfacelessWayland::PendingFrame::~PendingFrame() = default;

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

    buffer_manager_->CommitOverlays(widget_, submitted_frame->frame_id,
                                    submitted_frame->data,
                                    std::move(submitted_frame->configs));
    submitted_frames_.push_back(std::move(submitted_frame));
  }
}

void GbmSurfacelessWayland::FenceRetired(PendingFrame* frame) {
  frame->ready = true;
  MaybeSubmitFrames();
}

void GbmSurfacelessWayland::SetNoGLFlushForTests() {
  no_gl_flush_for_tests_ = true;
}

void GbmSurfacelessWayland::OnSubmission(uint32_t frame_id,
                                         const gfx::SwapResult& swap_result,
                                         gfx::GpuFenceHandle release_fence) {
  // If the frame_id is stale, the gpu process just recovered from a crash so
  // this frame_id can be ignored.
  if (submitted_frames_.empty() ||
      submitted_frames_.front()->frame_id != frame_id) {
    return;
  }

  auto submitted_frame = std::move(submitted_frames_.front());

  TRACE_EVENT("wayland", "GbmSurfacelessWayland::OnSubmission", "frame_id",
              submitted_frame->frame_id);

  submitted_frames_.erase(submitted_frames_.begin());
  for (auto& buf : submitted_frame->in_flight_color_buffers) {
    // Let the holder mark this buffer as free to reuse.
    solid_color_buffers_holder_->OnSubmission(buf, buffer_manager_);
  }
  submitted_frame->in_flight_color_buffers.clear();

  // Check if the fence has retired.
  if (!release_fence.is_null()) {
    base::TimeTicks ticks;
    auto status =
        gfx::GpuFence::GetStatusChangeTime(release_fence.Peek(), &ticks);
    if (status == gfx::GpuFence::kSignaled)
      release_fence = {};
  }

  std::move(submitted_frame->completion_callback)
      .Run(gfx::SwapCompletionResult(swap_result, std::move(release_fence)));

  pending_presentation_frames_.push_back(std::move(submitted_frame));

  if (swap_result != gfx::SwapResult::SWAP_ACK) {
    last_swap_buffers_result_ = false;
    return;
  }

  MaybeSubmitFrames();
}

void GbmSurfacelessWayland::OnPresentation(
    uint32_t frame_id,
    const gfx::PresentationFeedback& feedback) {
  if (pending_presentation_frames_.empty() ||
      pending_presentation_frames_.front()->frame_id != frame_id) {
    return;
  }

  std::move(pending_presentation_frames_.front()->presentation_callback)
      .Run(feedback);
  pending_presentation_frames_.erase(pending_presentation_frames_.begin());
}

EGLDisplay GbmSurfacelessWayland::GetEGLDisplay() {
  return display_->GetDisplay();
}

}  // namespace ui
