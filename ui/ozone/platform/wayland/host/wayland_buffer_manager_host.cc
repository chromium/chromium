// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"

#include <presentation-time-client-protocol.h>
#include <memory>

#include "base/i18n/number_formatting.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_drm.h"
#include "ui/ozone/platform/wayland/host/wayland_shm.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_linux_dmabuf.h"

namespace ui {

namespace {

// Use |kInvalidBufferId| to commit surface state without updating wl_buffer.
constexpr uint32_t kInvalidBufferId = 0u;

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

base::TimeTicks GetPresentationFeedbackTimeStamp(uint32_t tv_sec_hi,
                                                 uint32_t tv_sec_lo,
                                                 uint32_t tv_nsec) {
  const int64_t seconds = (static_cast<int64_t>(tv_sec_hi) << 32) + tv_sec_lo;
  const int64_t microseconds = seconds * base::Time::kMicrosecondsPerSecond +
                               tv_nsec / base::Time::kNanosecondsPerMicrosecond;
  return base::TimeTicks() + base::TimeDelta::FromMicroseconds(microseconds);
}

std::string NumberToString(uint32_t number) {
  return base::UTF16ToUTF8(base::FormatNumber(number));
}

}  // namespace

class WaylandBufferManagerHost::Surface {
 public:
  Surface(WaylandSurface* wayland_surface,
          WaylandConnection* connection,
          WaylandBufferManagerHost* buffer_manager)
      : wayland_surface_(wayland_surface),
        connection_(connection),
        buffer_manager_(buffer_manager) {}
  ~Surface() = default;

  bool CommitBuffer(uint32_t buffer_id,
                    const gfx::Rect& damage_region,
                    bool wait_for_frame_callback) {
    // The window has already been destroyed.
    if (!wayland_surface_)
      return true;

    // This is a buffer-less commit, do not lookup buffers.
    if (buffer_id == kInvalidBufferId) {
      pending_commits_.push_back({nullptr, wait_for_frame_callback});
      MaybeProcessPendingBuffer();
      return true;
    }

    WaylandBuffer* buffer = GetBuffer(buffer_id);
    if (!buffer) {
      // Get the anonymous_wl_buffer aka the buffer that has not been attached
      // to any of the surfaces previously.
      auto anonymous_wayland_buffer =
          buffer_manager_->PassAnonymousWlBuffer(buffer_id);
      if (!anonymous_wayland_buffer)
        return false;

      buffer = anonymous_wayland_buffer.get();
      buffers_.emplace(buffer_id, std::move(anonymous_wayland_buffer));
      if (buffer->wl_buffer)
        SetupBufferReleaseListener(buffer);
    }

    buffer->damage_region = damage_region;

    // If the wl_buffer has been attached, but the wl_buffer still is null, it
    // means the Wayland server failed to create the buffer and we have to fail
    // here.
    //
    // TODO(msisov): should we ask to recreate buffers instead of failing?
    if (buffer->attached && !buffer->wl_buffer)
      return false;

    pending_commits_.push_back({buffer, wait_for_frame_callback});
    MaybeProcessPendingBuffer();
    return true;
  }

  size_t DestroyBuffer(uint32_t buffer_id) {
    auto* buffer = GetBuffer(buffer_id);

    // Treat destroying a buffer as a release, and make sure to call any
    // OnSubmission callbacks that would be sent as a result of that.
    if (buffer) {
      buffer->released = true;
      MaybeProcessSubmittedBuffers();
      for (auto it = pending_commits_.begin(); it != pending_commits_.end();
           ++it) {
        if (it->buffer == buffer)
          pending_commits_.erase(it++);
      }
    }

    return buffers_.erase(buffer_id);
  }

  void AttachWlBuffer(uint32_t buffer_id, wl::Object<wl_buffer> new_buffer) {
    WaylandBuffer* buffer = GetBuffer(buffer_id);
    // It can happen that the buffer was destroyed by the client while the
    // Wayland compositor was processing the request to create a wl_buffer.
    if (!buffer)
      return;

    DCHECK(!buffer->wl_buffer);
    buffer->wl_buffer = std::move(new_buffer);
    buffer->attached = true;

    if (buffer->wl_buffer)
      SetupBufferReleaseListener(buffer);

    MaybeProcessPendingBuffer();
  }

  void ClearState() {
    buffers_.clear();
    wl_frame_callback_.reset();
    feedback_queue_ = PresentationFeedbackQueue();

    ResetSurfaceContents();

    submitted_buffers_.clear();
    pending_commits_.clear();

    connection_->ScheduleFlush();
  }

  void ResetSurfaceContents() {
    if (!wayland_surface_)
      return;

    wayland_surface_->AttachBuffer(nullptr);
    wayland_surface_->Commit();

    // ResetSurfaceContents happens upon WaylandWindow::Hide call, which
    // destroys xdg_surface, xdg_popup, etc. They are going to be reinitialized
    // once WaylandWindow::Show is called. Thus, they will have to be configured
    // once again before buffers can be attached.
    configured_ = false;

    connection_->ScheduleFlush();
  }

  bool BufferExists(uint32_t buffer_id) const {
    auto* buffer = GetBuffer(buffer_id);
    return !!buffer;
  }

  bool HasBuffers() const { return !buffers_.empty(); }

  void OnSurfaceRemoved() { wayland_surface_ = nullptr; }
  bool HasSurface() const { return !!wayland_surface_; }

  void OnSurfaceConfigured() {
    if (configured_)
      return;

    configured_ = true;
    MaybeProcessPendingBuffer();
  }

 private:
  struct FeedbackInfo {
    // The wayland object identifying this feedback.
    wl::Object<struct wp_presentation_feedback> wp_presentation_feedback;
    // The buffer that this presentation feedback is for.
    uint32_t buffer_id;
    // The actual presentation feedback. May be missing if the callback from the
    // Wayland server has not arrived yet.
    base::Optional<gfx::PresentationFeedback> feedback;
    // True iff OnSubmission has been called.
    bool submission_completed;
  };

  using PresentationFeedbackQueue = std::vector<FeedbackInfo>;

  // Holds information about a submitted buffer.
  struct SubmissionInfo {
    // ID of the submitted buffer. Buffers may be destroyed after they have been
    // submitted but before we send OnSubmission for them, e.g. if the same
    // buffer is submitted twice in a row. Keep the ID so we send OnSubmission
    // even if a buffer is destroyed.
    uint32_t buffer_id;
    // Whether this buffer has had OnSubmission sent for it.
    bool acked;
  };

  // Represents a pending surface commit.
  struct PendingCommit {
    // If null, means this commit will not attach buffer.
    WaylandBuffer* buffer = nullptr;
    // Whether this commit must wait for a wl_frame_callback and setup another
    // wl_frame_callback.
    bool wait_for_callback = false;
  };

  bool CommitBufferInternal(WaylandBuffer* buffer, bool wait_for_callback) {
    DCHECK(buffer && wayland_surface_);

    // If the same buffer has been submitted again right after the client
    // received OnSubmission for that buffer, just damage the buffer and
    // commit the surface again.
    if (submitted_buffers_.empty() ||
        submitted_buffers_.back().buffer_id != buffer->buffer_id) {
      // Once the BufferRelease is called, the buffer will be released.
      DCHECK(buffer->released);
      buffer->released = false;
      AttachBuffer(buffer);
    }

    // If the client submits the same buffer twice, we need to store it twice,
    // because the client will expect two acks for it.
    submitted_buffers_.push_back(
        SubmissionInfo{buffer->buffer_id, /*acked=*/false});

    DamageBuffer(buffer);

    if (wait_for_callback)
      SetupFrameCallback();
    SetupPresentationFeedback(buffer->buffer_id);

    CommitSurface();
    connection_->ScheduleFlush();
    MaybeProcessSubmittedBuffers();

    return true;
  }

  void DamageBuffer(WaylandBuffer* buffer) {
    DCHECK(wayland_surface_);

    gfx::Rect pending_damage_region = buffer->damage_region;
    // If the size of the damage region is empty, wl_surface_damage must be
    // supplied with the actual size of the buffer, which is going to be
    // committed.
    if (pending_damage_region.size().IsEmpty())
      pending_damage_region.set_size(buffer->size);
    DCHECK(!pending_damage_region.size().IsEmpty());

    wayland_surface_->UpdateBufferDamageRegion(pending_damage_region,
                                               buffer->size);
  }

  void AttachBuffer(WaylandBuffer* buffer) {
    DCHECK(wayland_surface_ && configured_);
    wayland_surface_->AttachBuffer(buffer->wl_buffer.get());
  }

  void CommitSurface() {
    DCHECK(wayland_surface_);
    wayland_surface_->Commit();
  }

  void SetupFrameCallback() {
    DCHECK(wayland_surface_);
    static const wl_callback_listener frame_listener = {
        &Surface::FrameCallbackDone};

    DCHECK(!wl_frame_callback_);
    wl_frame_callback_.reset(wl_surface_frame(wayland_surface_->surface()));
    wl_callback_add_listener(wl_frame_callback_.get(), &frame_listener, this);
  }

  void SetupPresentationFeedback(uint32_t buffer_id) {
    DCHECK(wayland_surface_);
    // Set up presentation feedback.
    if (!connection_->presentation())
      return;

    static const wp_presentation_feedback_listener feedback_listener = {
        &Surface::FeedbackSyncOutput, &Surface::FeedbackPresented,
        &Surface::FeedbackDiscarded};

    feedback_queue_.push_back(
        {wl::Object<struct wp_presentation_feedback>(wp_presentation_feedback(
             connection_->presentation(), wayland_surface_->surface())),
         buffer_id, /*feedback=*/base::nullopt,
         /*submission_completed=*/false});
    wp_presentation_feedback_add_listener(
        feedback_queue_.back().wp_presentation_feedback.get(),
        &feedback_listener, this);
  }

  void SetupBufferReleaseListener(WaylandBuffer* buffer) {
    static struct wl_buffer_listener buffer_listener = {
        &Surface::BufferRelease,
    };
    wl_buffer_add_listener(buffer->wl_buffer.get(), &buffer_listener, this);
  }

  WaylandBuffer* GetBuffer(uint32_t buffer_id) const {
    auto it = buffers_.find(buffer_id);
    return it != buffers_.end() ? it->second.get() : nullptr;
  }

  void OnFrameCallback(struct wl_callback* callback) {
    DCHECK(wl_frame_callback_.get() == callback);
    wl_frame_callback_.reset();

    MaybeProcessPendingBuffer();
  }

  // wl_callback_listener
  static void FrameCallbackDone(void* data,
                                struct wl_callback* callback,
                                uint32_t time) {
    Surface* self = static_cast<Surface*>(data);
    DCHECK(self);
    self->OnFrameCallback(callback);
  }

  void OnRelease(struct wl_buffer* wl_buffer) {
    DCHECK(wl_buffer);

    // Releases may not necessarily come in order, so search the submitted
    // buffers.
    WaylandBuffer* buffer = nullptr;
    for (const auto& b : submitted_buffers_) {
      auto* submitted_buffer = GetBuffer(b.buffer_id);
      if (submitted_buffer && wl_buffer == submitted_buffer->wl_buffer.get()) {
        buffer = submitted_buffer;
        break;
      }
    }
    DCHECK(buffer);
    DCHECK(!buffer->released);
    buffer->released = true;

    // A release means we may be able to send OnSubmission for previously
    // submitted buffers.
    MaybeProcessSubmittedBuffers();
  }

  // wl_buffer_listener
  static void BufferRelease(void* data, struct wl_buffer* wl_buffer) {
    Surface* self = static_cast<Surface*>(data);
    DCHECK(self);
    self->OnRelease(wl_buffer);
  }

  void MaybeProcessSubmittedBuffers() {
    if (!wayland_surface_)
      return;

    // We force an OnSubmission call for the very first buffer submitted,
    // otherwise buffers are not acked in a quiescent state. We keep track of
    // whether it has already been acked. A buffer may have already been acked
    // if it is the first buffer submitted and it is destroyed before being
    // explicitly released. In that case, don't send an OnSubmission.
    if (submitted_buffers_.size() == 1u && !submitted_buffers_[0].acked)
      ProcessOldestSubmittedBuffer();

    // Buffers may be released out of order, but we need to provide the
    // guarantee that OnSubmission will be called in order of buffer submission.
    while (submitted_buffers_.size() >= 2) {
      auto* buffer0 = GetBuffer(submitted_buffers_[0].buffer_id);
      // Treat a buffer as released if it has been explicitly released or
      // destroyed.
      bool buffer0_released = !buffer0 || buffer0->released;
      // We can send OnSubmission for the 2nd oldest buffer if the oldest buffer
      // is released, or it's the same buffer.
      if (!buffer0_released &&
          submitted_buffers_[0].buffer_id != submitted_buffers_[1].buffer_id)
        break;

      DCHECK(submitted_buffers_[0].acked);
      DCHECK(!submitted_buffers_[1].acked);
      submitted_buffers_.erase(submitted_buffers_.begin());
      ProcessOldestSubmittedBuffer();
    }
  }

  void ProcessOldestSubmittedBuffer() {
    DCHECK(wayland_surface_);
    DCHECK(!submitted_buffers_.empty());

    submitted_buffers_.front().acked = true;
    auto buffer_id = submitted_buffers_.front().buffer_id;

    // We can now complete the latest submission. We had to wait for this
    // release because SwapCompletionCallback indicates to the client that the
    // previous buffer is available for reuse.
    buffer_manager_->OnSubmission(wayland_surface_->GetWidget(), buffer_id,
                                  gfx::SwapResult::SWAP_ACK);

    // If presentation feedback is not supported, use a fake feedback. This
    // literally means there are no presentation feedback callbacks created.
    if (!connection_->presentation()) {
      DCHECK(feedback_queue_.empty());
      buffer_manager_->OnPresentation(
          wayland_surface_->GetWidget(), buffer_id,
          gfx::PresentationFeedback(base::TimeTicks::Now(), base::TimeDelta(),
                                    GetPresentationKindFlags(0)));
    } else {
      for (auto& info : feedback_queue_) {
        if (info.buffer_id == buffer_id && !info.submission_completed) {
          info.submission_completed = true;
          ProcessPresentationFeedbacks();
          return;
        }
      }
      NOTREACHED() << "Did not find matching feedback for buffer_id="
                   << buffer_id;
    }
  }

  void OnPresentation(struct wp_presentation_feedback* wp_presentation_feedback,
                      const gfx::PresentationFeedback& feedback,
                      bool discarded = false) {
    FeedbackInfo* feedback_info = nullptr;
    for (auto& info : feedback_queue_) {
      if (info.wp_presentation_feedback.get() == wp_presentation_feedback) {
        feedback_info = &info;
        break;
      } else if (!info.feedback.has_value() && !discarded) {
        // Feedback must come in order. However, if one of the feedbacks was
        // discarded and the previous feedbacks haven't been received yet, don't
        // mark previous feedbacks as failed as they will come later. For
        // example, imagine you are waiting for f[0], f[1] and f[2]. f[2] gets
        // discarded, previous ones mustn't be marked as failed as they will
        // come later.
        info.feedback = gfx::PresentationFeedback::Failure();
      }
    }
    DCHECK(feedback_info);
    DCHECK(!feedback_info->feedback.has_value());
    feedback_info->feedback = feedback;

    ProcessPresentationFeedbacks();
  }

  // We provide the guarantee to the client that:
  // 1. OnPresentation and OnSubmission will be called for each submitted buffer
  // 2. OnPresentation(buffer_id) will be called after OnSubmission(buffer_id)
  // 3. OnPresentation and OnSubmission will be called in the same order
  //    of buffer submission.
  // We make the following assumptions about the server:
  // 1. Presentation feedback will arrive in the same order of submission.
  // 2. Presentation feedback may never arrive if the buffer is destroyed.
  // 3. Presentation feedback may arrive at an arbitrary time after commit.
  // For these reasons, we can't associate feedback with a specific buffer,
  // as there may be more than one feedback in-flight for a single buffer.
  // This function ensures that we send OnPresentation for each buffer that
  // already has had OnSubmission called for it (condition #2).
  void ProcessPresentationFeedbacks() {
    if (!wayland_surface_)
      return;

    while (!feedback_queue_.empty()) {
      const auto& info = feedback_queue_.front();
      if (!info.submission_completed || !info.feedback.has_value())
        break;
      buffer_manager_->OnPresentation(wayland_surface_->GetWidget(),
                                      info.buffer_id, *info.feedback);
      feedback_queue_.erase(feedback_queue_.begin());
    }
    // This queue should be small - if not it's likely a bug.
    DCHECK_LE(feedback_queue_.size(), 25u);
  }

  // wp_presentation_feedback_listener
  static void FeedbackSyncOutput(
      void* data,
      struct wp_presentation_feedback* wp_presentation_feedback,
      struct wl_output* output) {}

  static void FeedbackPresented(
      void* data,
      struct wp_presentation_feedback* wp_presentation_feedback,
      uint32_t tv_sec_hi,
      uint32_t tv_sec_lo,
      uint32_t tv_nsec,
      uint32_t refresh,
      uint32_t seq_hi,
      uint32_t seq_lo,
      uint32_t flags) {
    Surface* self = static_cast<Surface*>(data);
    DCHECK(self);
    self->OnPresentation(
        wp_presentation_feedback,
        gfx::PresentationFeedback(
            GetPresentationFeedbackTimeStamp(tv_sec_hi, tv_sec_lo, tv_nsec),
            base::TimeDelta::FromNanoseconds(refresh),
            GetPresentationKindFlags(flags)));
  }

  static void FeedbackDiscarded(
      void* data,
      struct wp_presentation_feedback* wp_presentation_feedback) {
    Surface* self = static_cast<Surface*>(data);
    DCHECK(self);
    self->OnPresentation(wp_presentation_feedback,
                         gfx::PresentationFeedback::Failure(),
                         true /* discarded */);
  }

  void MaybeProcessPendingBuffer() {
    DCHECK_LE(pending_commits_.size(), 6u);
    // There is nothing to process if there is no pending buffer or the window
    // has been destroyed.
    if (pending_commits_.empty() || !wayland_surface_)
      return;

    // This request may come earlier than the Wayland compositor has imported a
    // wl_buffer. Wait until the buffer is created. The wait takes place only
    // once. Though, the case when a request to attach a buffer comes earlier
    // than the wl_buffer is created does not happen often. 1) Depending on the
    // zwp linux dmabuf protocol version, the wl_buffer can be created
    // immediately without asynchronous wait 2) the wl_buffer can have been
    // created by this time.
    //
    // Another case, which always happen is waiting until the frame callback is
    // completed. Thus, wait here when the Wayland compositor fires the frame
    // callback.
    //
    // The third case happens if the window hasn't been configured until a
    // request to attach a buffer to its surface is sent.
    auto pending_commit = std::move(pending_commits_.front());
    if ((pending_commit.buffer && !pending_commit.buffer->wl_buffer) ||
        (wl_frame_callback_ && pending_commit.wait_for_callback) ||
        !configured_) {
      return;
    }

    // A Commit without attaching buffers only needs to setup wl_frame_callback.
    if (!pending_commit.buffer) {
      pending_commits_.erase(pending_commits_.begin());
      if (pending_commit.wait_for_callback)
        SetupFrameCallback();
      CommitSurface();
      connection_->ScheduleFlush();
      MaybeProcessSubmittedBuffers();
      return;
    }

    pending_commits_.erase(pending_commits_.begin());
    CommitBufferInternal(pending_commit.buffer,
                         pending_commit.wait_for_callback);
  }

  // Widget this helper surface backs and has 1:1 relationship with the
  // WaylandWindow.

  // Non-owned. The surface this helper stores and submits buffers for.
  WaylandSurface* wayland_surface_;

  // Non-owned pointer to the connection.
  WaylandConnection* const connection_;

  // Non-owned pointer to the buffer manager.
  WaylandBufferManagerHost* const buffer_manager_;

  // A container of created buffers.
  base::flat_map<uint32_t, std::unique_ptr<WaylandBuffer>> buffers_;

  // A Wayland callback, which is triggered once wl_buffer has been committed
  // and it is right time to notify the GPU that it can start a new drawing
  // operation.
  wl::Object<wl_callback> wl_frame_callback_;

  // Queue of commits which are pending to be submitted (look the comment
  // in the CommitBuffer method).
  std::list<PendingCommit> pending_commits_;

  // Queue of buffers which have been submitted and are waiting to be
  // acked (send OnSubmission)
  std::vector<SubmissionInfo> submitted_buffers_;

  // Queue of buffers which have been acked and are waiting to have
  // OnPresentation sent.
  PresentationFeedbackQueue feedback_queue_;

  // If WaylandWindow has never been configured, do not try to attach
  // buffers to its surface. Otherwise, Wayland server will drop the connection
  // and send an error - "The surface has never been configured.".
  bool configured_ = false;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

WaylandBuffer::WaylandBuffer(const gfx::Size& size, uint32_t buffer_id)
    : size(size), buffer_id(buffer_id) {}
WaylandBuffer::~WaylandBuffer() = default;

WaylandBufferManagerHost::WaylandBufferManagerHost(
    WaylandConnection* connection)
    : connection_(connection), receiver_(this), weak_factory_(this) {
  connection_->wayland_window_manager()->AddObserver(this);
}

WaylandBufferManagerHost::~WaylandBufferManagerHost() {
  DCHECK(surfaces_.empty());
  DCHECK(anonymous_buffers_.empty());
}

void WaylandBufferManagerHost::OnWindowAdded(WaylandWindow* window) {
  DCHECK(window);
  surfaces_[window->root_surface()] =
      std::make_unique<Surface>(window->root_surface(), connection_, this);
}

void WaylandBufferManagerHost::OnWindowRemoved(WaylandWindow* window) {
  DCHECK(window);
  auto it = surfaces_.find(window->root_surface());
  DCHECK(it != surfaces_.end());
  if (it->second->HasBuffers()) {
    it->second->OnSurfaceRemoved();
    surface_graveyard_.emplace_back(std::move(it->second));
  }
  surfaces_.erase(it);
}

void WaylandBufferManagerHost::OnWindowConfigured(WaylandWindow* window) {
  DCHECK(window);
  auto it = surfaces_.find(window->root_surface());
  DCHECK(it != surfaces_.end());
  it->second->OnSurfaceConfigured();
}

void WaylandBufferManagerHost::OnSubsurfaceAdded(
    WaylandWindow* window,
    WaylandSubsurface* subsurface) {
  DCHECK(subsurface);
  surfaces_[subsurface->wayland_surface()] = std::make_unique<Surface>(
      subsurface->wayland_surface(), connection_, this);
  // WaylandSubsurface is always configured.
  surfaces_[subsurface->wayland_surface()]->OnSurfaceConfigured();
}

void WaylandBufferManagerHost::OnSubsurfaceRemoved(
    WaylandWindow* window,
    WaylandSubsurface* subsurface) {
  DCHECK(subsurface);
  auto it = surfaces_.find(subsurface->wayland_surface());
  DCHECK(it != surfaces_.end());
  if (it->second->HasBuffers()) {
    it->second->OnSurfaceRemoved();
    surface_graveyard_.emplace_back(std::move(it->second));
  }
  surfaces_.erase(it);
}

void WaylandBufferManagerHost::SetSurfaceConfigured(WaylandSurface* surface) {
  DCHECK(surface);
  auto it = surfaces_.find(surface);
  DCHECK(it != surfaces_.end());
  it->second->OnSurfaceConfigured();
}

void WaylandBufferManagerHost::SetTerminateGpuCallback(
    base::OnceCallback<void(std::string)> terminate_callback) {
  terminate_gpu_cb_ = std::move(terminate_callback);
}

mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost>
WaylandBufferManagerHost::BindInterface() {
  // Allow to rebind the interface if it hasn't been destroyed yet.
  if (receiver_.is_bound())
    OnChannelDestroyed();

  mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost>
      buffer_manager_host;
  receiver_.Bind(buffer_manager_host.InitWithNewPipeAndPassReceiver());
  return buffer_manager_host;
}

void WaylandBufferManagerHost::OnChannelDestroyed() {
  buffer_manager_gpu_associated_.reset();
  receiver_.reset();

  for (auto& surface_pair : surfaces_)
    surface_pair.second->ClearState();

  anonymous_buffers_.clear();
}

wl::BufferFormatsWithModifiersMap
WaylandBufferManagerHost::GetSupportedBufferFormats() const {
#if defined(WAYLAND_GBM)
  if (connection_->zwp_dmabuf())
    return connection_->zwp_dmabuf()->supported_buffer_formats();
  else if (connection_->drm())
    return connection_->drm()->supported_buffer_formats();
#endif
  return {};
}

bool WaylandBufferManagerHost::SupportsDmabuf() const {
  return !!connection_->zwp_dmabuf() ||
         (connection_->drm() && connection_->drm()->SupportsDrmPrime());
}

void WaylandBufferManagerHost::SetWaylandBufferManagerGpu(
    mojo::PendingAssociatedRemote<ozone::mojom::WaylandBufferManagerGpu>
        buffer_manager_gpu_associated) {
  buffer_manager_gpu_associated_.Bind(std::move(buffer_manager_gpu_associated));
}

void WaylandBufferManagerHost::CreateDmabufBasedBuffer(
    mojo::PlatformHandle dmabuf_fd,
    const gfx::Size& size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(error_message_.empty());

  TRACE_EVENT2("wayland", "WaylandBufferManagerHost::CreateDmabufBasedBuffer",
               "Format", format, "Buffer id", buffer_id);

  base::ScopedFD fd = dmabuf_fd.TakeFD();

  // Validate data and ask surface to create a buffer associated with the
  // |buffer_id|.
  if (!ValidateDataFromGpu(fd, size, strides, offsets, modifiers, format,
                           planes_count, buffer_id) ||
      !CreateBuffer(size, buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  // Create wl_buffer associated with the internal Buffer.
  auto callback =
      base::BindOnce(&WaylandBufferManagerHost::OnCreateBufferComplete,
                     weak_factory_.GetWeakPtr(), buffer_id);
  if (connection_->zwp_dmabuf()) {
    connection_->zwp_dmabuf()->CreateBuffer(std::move(fd), size, strides,
                                            offsets, modifiers, format,
                                            planes_count, std::move(callback));
  } else if (connection_->drm()) {
    connection_->drm()->CreateBuffer(std::move(fd), size, strides, offsets,
                                     modifiers, format, planes_count,
                                     std::move(callback));
  } else {
    // This method must never be called if neither zwp_linux_dmabuf or wl_drm
    // are supported.
    NOTREACHED();
  }
}

void WaylandBufferManagerHost::CreateShmBasedBuffer(mojo::PlatformHandle shm_fd,
                                                    uint64_t length,
                                                    const gfx::Size& size,
                                                    uint32_t buffer_id) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(error_message_.empty());

  TRACE_EVENT1("wayland", "WaylandBufferManagerHost::CreateShmBasedBuffer",
               "Buffer id", buffer_id);

  base::ScopedFD fd = shm_fd.TakeFD();
  // Validate data and create a buffer associated with the |buffer_id|.
  if (!ValidateDataFromGpu(fd, length, size, buffer_id) ||
      !CreateBuffer(size, buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  // Create a shm based wl_buffer and attach it to the created buffer.
  auto buffer = connection_->shm()->CreateBuffer(std::move(fd), length, size);
  OnCreateBufferComplete(buffer_id, std::move(buffer));

  connection_->ScheduleFlush();
}

bool WaylandBufferManagerHost::CommitBufferInternal(
    WaylandSurface* wayland_surface,
    uint32_t buffer_id,
    const gfx::Rect& damage_region,
    bool wait_for_frame_callback) {
  DCHECK(base::CurrentUIThread::IsSet());

  Surface* surface = GetSurface(wayland_surface);
  if (!surface || !ValidateBufferIdFromGpu(buffer_id))
    return false;

  if (!surface->CommitBuffer(buffer_id, damage_region,
                             wait_for_frame_callback)) {
    error_message_ =
        base::StrCat({"Buffer with ", NumberToString(buffer_id),
                      " id does not exist or failed to be created."});
  }

  if (!error_message_.empty())
    TerminateGpuProcess();
  return true;
}

bool WaylandBufferManagerHost::CommitWithoutBufferInternal(
    WaylandSurface* wayland_surface,
    bool wait_for_frame_callback) {
  DCHECK(base::CurrentUIThread::IsSet());

  Surface* surface = GetSurface(wayland_surface);
  if (!surface)
    return false;

  bool result = surface->CommitBuffer(kInvalidBufferId, gfx::Rect(),
                                      wait_for_frame_callback);
  DCHECK(result);

  if (!error_message_.empty())
    TerminateGpuProcess();
  return true;
}

void WaylandBufferManagerHost::CommitBuffer(gfx::AcceleratedWidget widget,
                                            uint32_t buffer_id,
                                            const gfx::Rect& damage_region) {
  DCHECK(base::CurrentUIThread::IsSet());

  TRACE_EVENT1("wayland", "WaylandBufferManagerHost::CommitBuffer", "Buffer id",
               buffer_id);

  DCHECK(error_message_.empty());

  if (widget == gfx::kNullAcceleratedWidget) {
    error_message_ = "Invalid widget.";
    TerminateGpuProcess();
  } else {
    auto* window = connection_->wayland_window_manager()->GetWindow(widget);
    if (!window)
      return;
    CommitBufferInternal(window->root_surface(), buffer_id, damage_region);
  }
}

void WaylandBufferManagerHost::CommitOverlays(
    gfx::AcceleratedWidget widget,
    std::vector<ui::ozone::mojom::WaylandOverlayConfigPtr> overlays) {
  DCHECK(base::CurrentUIThread::IsSet());

  TRACE_EVENT0("wayland", "WaylandBufferManagerHost::CommitOverlays");

  DCHECK(error_message_.empty());

  if (widget == gfx::kNullAcceleratedWidget) {
    error_message_ = "Invalid widget.";
    TerminateGpuProcess();
  }
  WaylandWindow* window =
      connection_->wayland_window_manager()->GetWindow(widget);
  // In tab dragging, window may have been destroyed when buffers reach here. We
  // omit buffer commits and OnSubmission, because the corresponding buffer
  // queue in gpu process should be destroyed soon.
  if (!window)
    return;

  window->CommitOverlays(overlays);
}

void WaylandBufferManagerHost::DestroyBuffer(gfx::AcceleratedWidget widget,
                                             uint32_t buffer_id) {
  DCHECK(base::CurrentUIThread::IsSet());

  TRACE_EVENT1("wayland", "WaylandBufferManagerHost::DestroyBuffer",
               "Buffer id", buffer_id);

  DCHECK(error_message_.empty());
  if (!ValidateBufferIdFromGpu(buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  // We allow creating buffers and attaching them to surfaces later. Thus, we
  // must pay attention to the following things during the destruction of the
  // buffers.
  // 1) the |widget| is basically a hint where we must search for buffers. If no
  // such surface exists (has already been destroyed), check if the buffer still
  // has been stored in the |anonymous_buffers_|.
  // 2) if the |widget| is null, always search a buffer with the |buffer_id| in
  // the |anonymous_buffers_|.
  // 3) if the |widget| hints at a non-existing window, it's likely that the
  // window has been destroyed. In that case, the surface containing the buffer
  // is in the graveyard.

  uint32_t destroyed_count = 0u;

  auto* window = connection_->wayland_window_manager()->GetWindow(widget);
  if (window) {
    // Case 1).
    Surface* surface = GetSurface(window->root_surface());
    if (surface) {
      destroyed_count = surface->DestroyBuffer(buffer_id);
      if (!surface->HasBuffers() && !surface->HasSurface())
        surfaces_.erase(window->root_surface());
    }
    const auto& subsurfaces = window->wayland_subsurfaces();
    for (const auto& it : subsurfaces) {
      Surface* subsurface = GetSurface((*it).wayland_surface());
      if (subsurface)
        destroyed_count += subsurface->DestroyBuffer(buffer_id);
    }
  } else {
    // Case 3)
    auto it = surface_graveyard_.begin();
    while (it != surface_graveyard_.end()) {
      destroyed_count += (*it)->DestroyBuffer(buffer_id);
      if (!(*it)->HasBuffers() && !(*it)->HasSurface()) {
        surface_graveyard_.erase(it++);
      } else {
        ++it;
      }
    }
  }

  // Ensure that we can't destroy more than 1 buffer. This can be 0 as well
  // if no buffers are destroyed.
  DCHECK_LE(destroyed_count, 1u);

  // Case 2)
  if (destroyed_count == 1u || DestroyAnonymousBuffer(buffer_id))
    return;

  error_message_ = base::StrCat(
      {"Buffer with ", NumberToString(buffer_id), " id does not exist"});
  TerminateGpuProcess();
}

void WaylandBufferManagerHost::ResetSurfaceContents(
    WaylandSurface* wayland_surface) {
  auto* surface = GetSurface(wayland_surface);
  DCHECK(surface);
  surface->ResetSurfaceContents();
}

std::unique_ptr<WaylandBuffer> WaylandBufferManagerHost::PassAnonymousWlBuffer(
    uint32_t buffer_id) {
  auto it = anonymous_buffers_.find(buffer_id);
  if (it == anonymous_buffers_.end())
    return nullptr;
  auto buffer = std::move(it->second);
  anonymous_buffers_.erase(it);
  return buffer;
}

bool WaylandBufferManagerHost::CreateBuffer(const gfx::Size& size,
                                            uint32_t buffer_id) {
  // First check if any of the surfaces has already had a buffer with the same
  // id.
  for (auto const& surface : surfaces_) {
    if (surface.second->BufferExists(buffer_id)) {
      error_message_ = base::StrCat(
          {"A buffer with id= ", NumberToString(buffer_id), " already exists"});
      return false;
    }
  }

  auto result = anonymous_buffers_.emplace(
      buffer_id, std::make_unique<WaylandBuffer>(size, buffer_id));
  if (!result.second)
    error_message_ = base::StrCat(
        {"A buffer with id= ", NumberToString(buffer_id), " already exists"});
  return result.second;
}

WaylandBufferManagerHost::Surface* WaylandBufferManagerHost::GetSurface(
    WaylandSurface* wayland_surface) const {
  auto it = surfaces_.find(wayland_surface);
  return it != surfaces_.end() ? it->second.get() : nullptr;
}

bool WaylandBufferManagerHost::ValidateDataFromGpu(
    const base::ScopedFD& fd,
    const gfx::Size& size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  if (!ValidateBufferIdFromGpu(buffer_id))
    return false;

  std::string reason;
  if (!fd.is_valid())
    reason = "Buffer fd is invalid";

  if (size.IsEmpty())
    reason = "Buffer size is invalid";

  if (planes_count < 1)
    reason = "Planes count cannot be less than 1";

  if (planes_count != strides.size() || planes_count != offsets.size() ||
      planes_count != modifiers.size()) {
    reason = base::StrCat({"Number of strides(", NumberToString(strides.size()),
                           ")/offsets(", NumberToString(offsets.size()),
                           ")/modifiers(", NumberToString(modifiers.size()),
                           ") does not correspond to the number of planes(",
                           NumberToString(planes_count), ")"});
  }

  for (auto stride : strides) {
    if (stride == 0)
      reason = "Strides are invalid";
  }

  if (!IsValidBufferFormat(format))
    reason = "Buffer format is invalid";

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }
  return true;
}

bool WaylandBufferManagerHost::ValidateBufferIdFromGpu(uint32_t buffer_id) {
  std::string reason;
  if (buffer_id < 1)
    reason = base::StrCat({"Invalid buffer id: ", NumberToString(buffer_id)});

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }

  return true;
}

bool WaylandBufferManagerHost::ValidateDataFromGpu(const base::ScopedFD& fd,
                                                   size_t length,
                                                   const gfx::Size& size,
                                                   uint32_t buffer_id) {
  if (!ValidateBufferIdFromGpu(buffer_id))
    return false;

  std::string reason;
  if (!fd.is_valid())
    reason = "Buffer fd is invalid";

  if (length == 0)
    reason = "The shm pool length cannot be less than 1";

  if (size.IsEmpty())
    reason = "Buffer size is invalid";

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }

  return true;
}

void WaylandBufferManagerHost::OnCreateBufferComplete(
    uint32_t buffer_id,
    wl::Object<struct wl_buffer> new_buffer) {
  auto it = anonymous_buffers_.find(buffer_id);
  // It might have already been destroyed or stored by any of the surfaces.
  if (it != anonymous_buffers_.end()) {
    it->second->wl_buffer = std::move(new_buffer);
  } else {
    for (auto& surface : surfaces_) {
      if (surface.second->BufferExists(buffer_id)) {
        surface.second.get()->AttachWlBuffer(buffer_id, std::move(new_buffer));
        break;
      }
    }
  }
  // There is no need for the buffer anymore. Let it go out of the scope and
  // be destroyed.
}

void WaylandBufferManagerHost::OnSubmission(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::SwapResult& swap_result) {
  DCHECK(base::CurrentUIThread::IsSet());

  DCHECK(buffer_manager_gpu_associated_);
  buffer_manager_gpu_associated_->OnSubmission(widget, buffer_id, swap_result);
}

void WaylandBufferManagerHost::OnPresentation(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(base::CurrentUIThread::IsSet());

  DCHECK(buffer_manager_gpu_associated_);
  buffer_manager_gpu_associated_->OnPresentation(widget, buffer_id, feedback);
}

void WaylandBufferManagerHost::TerminateGpuProcess() {
  DCHECK(!error_message_.empty());
  std::move(terminate_gpu_cb_).Run(std::move(error_message_));
  // The GPU process' failure results in calling ::OnChannelDestroyed.
}

bool WaylandBufferManagerHost::DestroyAnonymousBuffer(uint32_t buffer_id) {
  auto it = anonymous_buffers_.find(buffer_id);
  if (it == anonymous_buffers_.end())
    return false;

  anonymous_buffers_.erase(it);
  return true;
}

}  // namespace ui
