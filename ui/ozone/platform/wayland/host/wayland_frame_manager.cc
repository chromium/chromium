// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_frame_manager.h"

#include <presentation-time-client-protocol.h>
#include <sync/sync.h>

#include "base/containers/adapters.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_handle.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

namespace {

constexpr uint32_t kMaxNumberOfFrames = 20u;

uint32_t GetPresentationKindFlags(uint32_t flags) {
  // Wayland spec has different meaning of VSync. In Chromium, VSync means to
  // update the begin frame vsync timing based on presentation feedback.
  uint32_t presentation_flags = gfx::PresentationFeedback::kVSync;

  if (flags & WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK)
    presentation_flags |= gfx::PresentationFeedback::kHWClock;
  if (flags & WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION)
    presentation_flags |= gfx::PresentationFeedback::kHWCompletion;
  if (flags & WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY)
    presentation_flags |= gfx::PresentationFeedback::kZeroCopy;

  return presentation_flags;
}

}  // namespace

WaylandFrame::WaylandFrame(
    uint32_t frame_id,
    WaylandSurface* root_surface,
    wl::WaylandOverlayConfig root_config,
    base::circular_deque<
        std::pair<WaylandSubsurface*, wl::WaylandOverlayConfig>>
        subsurfaces_to_overlays)
    : frame_id(frame_id),
      root_surface(root_surface),
      root_config(std::move(root_config)),
      subsurfaces_to_overlays(std::move(subsurfaces_to_overlays)),
      submission_acked(false),
      presentation_acked(false) {}

WaylandFrame::WaylandFrame(
    WaylandSurface* root_surface,
    wl::WaylandOverlayConfig root_config,
    base::circular_deque<
        std::pair<WaylandSubsurface*, wl::WaylandOverlayConfig>>
        subsurfaces_to_overlays)
    : root_surface(root_surface),
      root_config(std::move(root_config)),
      subsurfaces_to_overlays(std::move(subsurfaces_to_overlays)),
      submission_acked(true),
      presentation_acked(true) {}

WaylandFrame::~WaylandFrame() = default;

WaylandFrameManager::WaylandFrameManager(WaylandWindow* window,
                                         WaylandConnection* connection)
    : window_(window), connection_(connection), weak_factory_(this) {}

WaylandFrameManager::~WaylandFrameManager() {
  ClearStates(true /* closing */);
}

void WaylandFrameManager::RecordFrame(std::unique_ptr<WaylandFrame> frame) {
  DCHECK_LE(pending_frames_.size(), 6u);

  // Request for buffer handle creation at record time.
  for (auto& subsurface_to_overlay : frame->subsurfaces_to_overlays) {
    if (subsurface_to_overlay.second.buffer_id) {
      auto* handle = connection_->buffer_manager_host()->EnsureBufferHandle(
          subsurface_to_overlay.first->wayland_surface(),
          subsurface_to_overlay.second.buffer_id);
      if (!handle)
        return;
    }
  }
  if (frame->root_config.buffer_id) {
    auto* handle = connection_->buffer_manager_host()->EnsureBufferHandle(
        frame->root_surface, frame->root_config.buffer_id);
    if (!handle)
      return;
  }

  pending_frames_.push_back(std::move(frame));
  MaybeProcessPendingFrame();
}

void WaylandFrameManager::MaybeProcessPendingFrame() {
  if (pending_frames_.empty())
    return;

  auto* frame = pending_frames_.front().get();
  DCHECK(frame) << "This WaylandFrame is already in playback.";
  if (!frame)
    return;

  // Ensure wl_buffer existence.
  WaylandBufferHandle* handle_pending_creation = nullptr;
  for (auto& subsurface_to_overlay : frame->subsurfaces_to_overlays) {
    if (subsurface_to_overlay.second.buffer_id) {
      auto* handle = connection_->buffer_manager_host()->EnsureBufferHandle(
          subsurface_to_overlay.first->wayland_surface(),
          subsurface_to_overlay.second.buffer_id);
      // Buffer is gone while this frame is pending, remove this config.
      if (!handle) {
        frame->buffer_lost = true;
        subsurface_to_overlay.second = wl::WaylandOverlayConfig();
      } else if (!handle->wl_buffer() && !handle_pending_creation) {
        // Found the first not-ready buffer, let handle invoke
        // MaybeProcessPendingFrame() when wl_buffer is created.
        handle_pending_creation = handle;
      }
    }
  }
  if (frame->root_config.buffer_id) {
    auto* handle = connection_->buffer_manager_host()->EnsureBufferHandle(
        frame->root_surface, frame->root_config.buffer_id);
    if (!handle) {
      frame->buffer_lost = true;
      frame->root_config = wl::WaylandOverlayConfig();
    } else if (!handle->wl_buffer() && !handle_pending_creation) {
      handle_pending_creation = handle;
    }
  }

  // There are wl_buffers missing, need to wait.
  if (handle_pending_creation) {
    handle_pending_creation->set_buffer_created_callback(
        base::BindOnce(&WaylandFrameManager::MaybeProcessPendingFrame,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  // Frame callback hasn't been acked, need to wait.
  if (!submitted_frames_.empty() &&
      submitted_frames_.back()->wl_frame_callback) {
    return;
  }
  // Window is not configured, need to wait.
  if (!window_->can_submit_frames())
    return;

  std::unique_ptr<WaylandFrame> playback = std::move(pending_frames_.front());
  PlayBackFrame(std::move(playback));

  pending_frames_.pop_front();

  // wl_frame_callback drives the continuous playback of frames, if the frame we
  // just played-back did not set up a wl_frame_callback, we should playback
  // another frame.
  if (!submitted_frames_.empty() &&
      !submitted_frames_.back()->wl_frame_callback) {
    MaybeProcessPendingFrame();
  }
}

void WaylandFrameManager::PlayBackFrame(std::unique_ptr<WaylandFrame> frame) {
  // Skip this frame if we can't playback this frame due to lost buffers.
  if (frame->buffer_lost) {
    frame->feedback = gfx::PresentationFeedback::Failure();
    submitted_frames_.push_back(std::move(frame));
    VerifyNumberOfSubmittedFrames();
    MaybeProcessSubmittedFrames();
    return;
  }

  auto* root_surface = frame->root_surface;
  auto& root_config = frame->root_config;
  bool empty_frame = !root_config.buffer_id;

  if (!empty_frame) {
    window_->UpdateVisualSize(
        gfx::ToRoundedSize(root_config.bounds_rect.size()),
        root_config.surface_scale_factor);
  }

  // Configure subsurfaces. Traverse the deque backwards s.t. we can set
  // frame_callback and presentation_feedback on the top-most possible surface.
  WaylandSubsurface* reference_above = nullptr;
  for (auto& [subsurface, config] :
       base::Reversed(frame->subsurfaces_to_overlays)) {
    DCHECK(subsurface);
    auto* surface = subsurface->wayland_surface();
    if (empty_frame || !config.buffer_id ||
        wl_fixed_from_double(config.opacity) == 0) {
      subsurface->Hide();
      // Mutter sometimes does not call buffer.release if wl_surface role is
      // destroyed, causing graphics freeze. Manually release buffer from the
      // last frame and trigger OnSubmission callbacks.
      if (!submitted_frames_.empty()) {
        auto result = submitted_frames_.back()->submitted_buffers.find(surface);
        if (result != submitted_frames_.back()->submitted_buffers.end()) {
          submitted_frames_.back()->submitted_buffers.erase(result);
          surface->AttachBuffer(nullptr);
          surface->ApplyPendingState();
          surface->Commit(false);
        }
      }
    } else {
      subsurface->ConfigureAndShowSurface(
          config.bounds_rect, root_config.bounds_rect,
          root_config.surface_scale_factor, nullptr, reference_above);
      ApplySurfaceConfigure(frame.get(), surface, config, true);
      reference_above = subsurface;
      surface->Commit(false);
    }
  }

  if (empty_frame) {
    // GPU channel has been destroyed. Do nothing for empty frames except that
    // the frame should be marked as failed if it hasn't been presented yet.
    if (!frame->presentation_acked)
      frame->feedback = gfx::PresentationFeedback::Failure();
  } else {
    // Opaque region is set during UpdateVisualSize() no need to set it again.
    ApplySurfaceConfigure(frame.get(), root_surface, root_config, false);
  }

  DCHECK(empty_frame || !connection_->presentation() ||
         frame->pending_feedback || frame->feedback.has_value());
  root_surface->Commit(true);

  frame->root_config = wl::WaylandOverlayConfig();
  frame->subsurfaces_to_overlays.clear();

  // Empty frames do not expect feedbacks so don't push to |submitted_frames_|.
  if (!empty_frame)
    submitted_frames_.push_back(std::move(frame));

  VerifyNumberOfSubmittedFrames();

  MaybeProcessSubmittedFrames();
}

void WaylandFrameManager::ApplySurfaceConfigure(
    WaylandFrame* frame,
    WaylandSurface* surface,
    wl::WaylandOverlayConfig& config,
    bool set_opaque_region) {
  DCHECK(surface);
  if (!config.buffer_id)
    return;

  static const wl_callback_listener frame_listener = {
      &WaylandFrameManager::FrameCallbackDone};
  static const wp_presentation_feedback_listener feedback_listener = {
      &WaylandFrameManager::FeedbackSyncOutput,
      &WaylandFrameManager::FeedbackPresented,
      &WaylandFrameManager::FeedbackDiscarded};

  surface->SetBufferTransform(config.transform);
  surface->SetSurfaceBufferScale(ceil(config.surface_scale_factor));
  surface->SetViewportSource(config.crop_rect);
  surface->SetViewportDestination(config.bounds_rect.size());
  surface->SetOpacity(config.opacity);
  surface->SetBlending(config.enable_blend);
  surface->SetRoundedClipBounds(config.rounded_clip_bounds);
  surface->SetOverlayPriority(config.priority_hint);
  surface->SetBackgroundColor(config.background_color);
  if (set_opaque_region) {
    std::vector<gfx::Rect> region_px = {
        gfx::Rect(gfx::ToRoundedSize(config.bounds_rect.size()))};
    surface->SetOpaqueRegion(config.enable_blend ? nullptr : &region_px);
  }

  WaylandBufferHandle* buffer_handle =
      connection_->buffer_manager_host()->GetBufferHandle(surface,
                                                          config.buffer_id);
  DCHECK(buffer_handle);
  bool will_attach = surface->AttachBuffer(buffer_handle);
  // If we don't attach a released buffer, graphics freeze will occur.
  DCHECK(will_attach || !buffer_handle->released(surface));

  surface->UpdateBufferDamageRegion(config.damage_region);
  if (!config.access_fence_handle.is_null())
    surface->SetAcquireFence(std::move(config.access_fence_handle));

  if (will_attach) {
    // Setup frame callback if wayland_surface will commit this buffer.
    // On Mutter, we don't receive frame.callback acks if we don't attach a
    // new wl_buffer, which leads to graphics freeze. So only setup
    // frame_callback when we're attaching a different buffer.
    if (!frame->wl_frame_callback) {
      frame->wl_frame_callback.reset(wl_surface_frame(surface->surface()));
      wl_callback_add_listener(frame->wl_frame_callback.get(), &frame_listener,
                               this);
    }

    if (connection_->linux_explicit_synchronization_v1() &&
        !surface->has_explicit_release_callback()) {
      surface->set_explicit_release_callback(
          base::BindRepeating(&WaylandFrameManager::OnExplicitBufferRelease,
                              weak_factory_.GetWeakPtr(), surface));
    }
    buffer_handle->set_buffer_released_callback(
        base::BindOnce(&WaylandFrameManager::OnWlBufferRelease,
                       weak_factory_.GetWeakPtr(), surface),
        surface);
  }

  if (connection_->presentation() && !frame->pending_feedback) {
    frame->pending_feedback.reset(wp_presentation_feedback(
        connection_->presentation(), surface->surface()));
    wp_presentation_feedback_add_listener(frame->pending_feedback.get(),
                                          &feedback_listener, this);
  }

  // If we have submitted this buffer in a previous frame and it is not released
  // yet, submitting the buffer again will not make wayland compositor to
  // release it twice. Remove it from the previous frame.
  for (auto& submitted_frames : submitted_frames_) {
    auto result = submitted_frames->submitted_buffers.find(surface);
    if (result != submitted_frames->submitted_buffers.end() &&
        result->second->wl_buffer() == buffer_handle->wl_buffer()) {
      submitted_frames->submitted_buffers.erase(result);
      break;
    }
  }

  frame->submitted_buffers.emplace(surface, buffer_handle);

  // Send instructions across wayland protocol, but do not commit yet, let the
  // caller decide whether the commit should flush.
  surface->ApplyPendingState();
}

// static
void WaylandFrameManager::FrameCallbackDone(void* data,
                                            struct wl_callback* callback,
                                            uint32_t time) {
  WaylandFrameManager* self = static_cast<WaylandFrameManager*>(data);
  DCHECK(self);
  self->OnFrameCallback(callback);
}

void WaylandFrameManager::OnFrameCallback(struct wl_callback* callback) {
  DCHECK(submitted_frames_.back()->wl_frame_callback.get() == callback);
  submitted_frames_.back()->wl_frame_callback.reset();
  MaybeProcessPendingFrame();
}

// static
void WaylandFrameManager::FeedbackSyncOutput(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback,
    struct wl_output* output) {}

// static
void WaylandFrameManager::FeedbackPresented(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback,
    uint32_t tv_sec_hi,
    uint32_t tv_sec_lo,
    uint32_t tv_nsec,
    uint32_t refresh,
    uint32_t seq_hi,
    uint32_t seq_lo,
    uint32_t flags) {
  WaylandFrameManager* self = static_cast<WaylandFrameManager*>(data);
  DCHECK(self);
  self->OnPresentation(
      wp_presentation_feedback,
      gfx::PresentationFeedback(self->connection_->ConvertPresentationTime(
                                    tv_sec_hi, tv_sec_lo, tv_nsec),
                                base::Nanoseconds(refresh),
                                GetPresentationKindFlags(flags)));
}

// static
void WaylandFrameManager::FeedbackDiscarded(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback) {
  WaylandFrameManager* self = static_cast<WaylandFrameManager*>(data);
  DCHECK(self);
  self->OnPresentation(wp_presentation_feedback,
                       gfx::PresentationFeedback::Failure(),
                       true /* discarded */);
}

void WaylandFrameManager::OnPresentation(
    struct wp_presentation_feedback* wp_presentation_feedback,
    const gfx::PresentationFeedback& feedback,
    bool discarded) {
  for (auto& frame : submitted_frames_) {
    if (frame->pending_feedback.get() == wp_presentation_feedback) {
      frame->feedback = feedback;
      break;
    } else if (!frame->feedback.has_value() && !discarded) {
      // Feedback must come in order. However, if one of the feedbacks was
      // discarded and the previous feedbacks haven't been received yet, don't
      // mark previous feedbacks as failed as they will come later. For
      // example, imagine you are waiting for f[0], f[1] and f[2]. f[2] gets
      // discarded, previous ones mustn't be marked as failed as they will
      // come later.
      // TODO(fangzhoug): Exo seems to deliver presentation_feedbacks out of
      // order occasionally, causing us to mark a valid feedback as failed.
      // Investigate the issue with surface sync.
      frame->feedback = gfx::PresentationFeedback::Failure();
    }
    CHECK_NE(frame.get(), submitted_frames_.back().get());
  }
  MaybeProcessSubmittedFrames();
}

void WaylandFrameManager::VerifyNumberOfSubmittedFrames() {
  static constexpr uint32_t kLastThreeFrames = 3u;

  // This queue should be small - if not it's likely a bug.
  //
  // Ideally this should be a DCHECK, but if the feedbacks are never sent (a bug
  // in the server, everything crashes).
  if (submitted_frames_.size() >= kMaxNumberOfFrames) {
    LOG(ERROR)
        << "The server has buggy presentation feedback. Discarding all "
           "presentation feedback requests in all frames except the last "
        << kLastThreeFrames << ".";
    // Leave three last frames in case if the server restores its behavior
    // (unlikely).
    for (auto it = submitted_frames_.begin();
         it < (submitted_frames_.end() - kLastThreeFrames); it++) {
      if (!(*it)->submission_acked || !(*it)->pending_feedback)
        break;
      DCHECK(!(*it)->feedback.has_value());
      (*it)->feedback = gfx::PresentationFeedback::Failure();
      (*it)->pending_feedback.reset();
    }
  }
}

void WaylandFrameManager::OnExplicitBufferRelease(WaylandSurface* surface,
                                                  struct wl_buffer* wl_buffer,
                                                  base::ScopedFD fence) {
  DCHECK(wl_buffer);

  // Releases may not necessarily come in order, so search the submitted
  // buffers.
  for (const auto& frame : submitted_frames_) {
    auto result = frame->submitted_buffers.find(surface);
    if (result != frame->submitted_buffers.end() &&
        result->second->wl_buffer() == wl_buffer) {
      // Explicitly make this buffer released when
      // linux_explicit_synchronization is used.
      result->second->OnExplicitRelease(surface);

      if (fence.is_valid()) {
        if (frame->merged_release_fence_fd.is_valid()) {
          frame->merged_release_fence_fd.reset(sync_merge(
              "", frame->merged_release_fence_fd.get(), fence.get()));
        } else {
          frame->merged_release_fence_fd = std::move(fence);
        }
        DCHECK(frame->merged_release_fence_fd.is_valid());
      }

      frame->submitted_buffers.erase(result);
      break;
    }
  }

  // A release means we may be able to send OnSubmission for previously
  // submitted buffers.
  MaybeProcessSubmittedFrames();
}

void WaylandFrameManager::OnWlBufferRelease(WaylandSurface* surface,
                                            struct wl_buffer* wl_buffer) {
  DCHECK(wl_buffer);

  // Releases may not necessarily come in order, so search the submitted
  // buffers.
  for (const auto& frame : submitted_frames_) {
    auto result = frame->submitted_buffers.find(surface);
    if (result != frame->submitted_buffers.end() &&
        result->second->wl_buffer() == wl_buffer) {
      frame->submitted_buffers.erase(result);
      break;
    }
  }

  // A release means we may be able to send OnSubmission for previously
  // submitted buffers.
  MaybeProcessSubmittedFrames();
}

void WaylandFrameManager::MaybeProcessSubmittedFrames() {
  if (submitted_frames_.empty())
    return;

  // We force an OnSubmission call for the very first buffer submitted,
  // otherwise buffers are not acked in a quiescent state. We keep track of
  // whether it has already been acked. A buffer may have already been acked
  // if it is the first buffer submitted and it is destroyed before being
  // explicitly released. In that case, don't send an OnSubmission.
  if (submitted_frames_.size() == 1u &&
      !submitted_frames_.front()->submission_acked) {
    ProcessOldSubmittedFrame(submitted_frames_.front().get(),
                             gfx::GpuFenceHandle());
  }

  // Buffers may be released out of order, but we need to provide the
  // guarantee that OnSubmission will be called in order of buffer submission.
  for (auto iter = submitted_frames_.begin();
       iter < submitted_frames_.end() - 1; ++iter) {
    // Treat a buffer as released if it has been explicitly released or
    // destroyed.
    bool all_buffers_released = (*iter)->submitted_buffers.empty();
    // We can send OnSubmission for the |iter + 1| if the buffers of |iter| are
    // all released.
    if (!all_buffers_released)
      break;

    DCHECK((*iter)->submission_acked);
    if ((*(iter + 1))->submission_acked)
      continue;

    // Call OnSubmission() for this for |iter + 1| since |iter| is fully
    // released.
    gfx::GpuFenceHandle release_fence_handle;
    if ((*iter)->merged_release_fence_fd.is_valid())
      release_fence_handle.owned_fd =
          std::move((*iter)->merged_release_fence_fd);
    ProcessOldSubmittedFrame((iter + 1)->get(),
                             std::move(release_fence_handle));
  }

  // Process for presentation feedbacks. OnPresentation() must be called after
  // OnSubmission() for a frame.
  for (auto& frame : submitted_frames_) {
    if (!frame->submission_acked || !frame->feedback.has_value())
      break;
    if (frame->presentation_acked)
      continue;
    frame->presentation_acked = true;
    connection_->buffer_manager_host()->OnPresentation(
        window_->GetWidget(), frame->frame_id, frame->feedback.value());
  }

  // Clear frames that are fully released and has already called
  // OnPresentation().
  while (submitted_frames_.size() > 1 &&
         submitted_frames_.front()->submitted_buffers.empty() &&
         submitted_frames_.front()->presentation_acked) {
    DCHECK(submitted_frames_.front()->submission_acked);
    submitted_frames_.pop_front();
  }

  DCHECK_LE(submitted_frames_.size(), kMaxNumberOfFrames);
}

void WaylandFrameManager::ProcessOldSubmittedFrame(
    WaylandFrame* frame,
    gfx::GpuFenceHandle release_fence_handle) {
  DCHECK(!submitted_frames_.empty());
  DCHECK(!frame->submission_acked);
  DCHECK(!frame->presentation_acked);
  DCHECK(frame->pending_feedback || frame->feedback.has_value() ||
         !connection_->presentation());
  frame->submission_acked = true;

  // We can now complete the latest submission. We had to wait for this
  // release because SwapCompletionCallback indicates to the client that the
  // buffers in previous frame is available for reuse.
  connection_->buffer_manager_host()->OnSubmission(
      window_->GetWidget(), frame->frame_id, gfx::SwapResult::SWAP_ACK,
      std::move(release_fence_handle));

  // If presentation feedback is not supported, use a fake feedback. This
  // literally means there are no presentation feedback callbacks created.
  if (!connection_->presentation()) {
    DCHECK(!frame->feedback.has_value() || frame->feedback->failed());
    frame->feedback = frame->feedback.value_or(
        gfx::PresentationFeedback(base::TimeTicks::Now(), base::TimeDelta(),
                                  GetPresentationKindFlags(0)));
  }
}

void WaylandFrameManager::Hide() {
  if (!submitted_frames_.empty() &&
      submitted_frames_.back()->wl_frame_callback) {
    submitted_frames_.back()->wl_frame_callback.reset();
    // Mutter sometimes does not call buffer.release if wl_surface role is
    // destroyed, causing graphics freeze. Manually release them and trigger
    // OnSubmission callbacks.
    for (auto& submitted : submitted_frames_.back()->submitted_buffers)
      submitted.second->OnExplicitRelease(submitted.first);
    submitted_frames_.back()->submitted_buffers.clear();
    submitted_frames_.back()->feedback = gfx::PresentationFeedback::Failure();
  }

  // Discard |pending_frames_| since they're not going to be used when hidden.
  for (auto& frame : pending_frames_) {
    DCHECK(frame) << "Can't perform Hide() during a frame playback.";
    frame->feedback = gfx::PresentationFeedback::Failure();
    submitted_frames_.push_back(std::move(frame));
  }
  pending_frames_.clear();

  MaybeProcessSubmittedFrames();
}

void WaylandFrameManager::ClearStates(bool closing) {
  for (auto& frame : submitted_frames_) {
    frame->wl_frame_callback.reset();
    for (auto& submitted : frame->submitted_buffers)
      submitted.second->OnExplicitRelease(submitted.first);
    frame->submission_acked = true;
    frame->submitted_buffers.clear();
    if (!frame->feedback.has_value())
      frame->feedback = gfx::PresentationFeedback::Failure();
  }

  for (auto& frame : pending_frames_) {
    DCHECK(frame)
        << "Can't perform OnChannelDestroyed() during a frame playback.";
    frame->feedback = gfx::PresentationFeedback::Failure();
    submitted_frames_.push_back(std::move(frame));
  }
  pending_frames_.clear();

  if (closing)
    return;

  MaybeProcessSubmittedFrames();

  DCHECK(submitted_frames_.empty() ||
         (submitted_frames_.size() == 1 &&
          submitted_frames_.back()->submission_acked &&
          submitted_frames_.back()->presentation_acked));
}

}  // namespace ui
