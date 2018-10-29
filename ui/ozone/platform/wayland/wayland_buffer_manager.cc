// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_buffer_manager.h"

#include <drm_fourcc.h>
#include <linux-dmabuf-unstable-v1-client-protocol.h>
#include <presentation-time-client-protocol.h>

#include "base/trace_event/trace_event.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

namespace ui {

namespace {

uint32_t GetPresentationKindFlags(uint32_t flags) {
  uint32_t presentation_flags = 0;
  if (flags & WP_PRESENTATION_FEEDBACK_KIND_VSYNC)
    presentation_flags |= gfx::PresentationFeedback::kVSync;
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

}  // namespace

WaylandBufferManager::Buffer::Buffer() = default;
WaylandBufferManager::Buffer::Buffer(uint32_t id,
                                     zwp_linux_buffer_params_v1* zwp_params,
                                     const gfx::Size& buffer_size)
    : buffer_id(id), size(buffer_size), params(zwp_params) {}
WaylandBufferManager::Buffer::~Buffer() = default;

WaylandBufferManager::WaylandBufferManager(
    zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
    WaylandConnection* connection)
    : zwp_linux_dmabuf_(zwp_linux_dmabuf), connection_(connection) {
  static const zwp_linux_dmabuf_v1_listener dmabuf_listener = {
      &WaylandBufferManager::Format, &WaylandBufferManager::Modifiers,
  };
  zwp_linux_dmabuf_v1_add_listener(zwp_linux_dmabuf_.get(), &dmabuf_listener,
                                   this);

  // A roundtrip after binding guarantees that the client has received all
  // supported formats.
  wl_display_roundtrip(connection->display());
}

WaylandBufferManager::~WaylandBufferManager() {
  DCHECK(buffers_.empty());
}

bool WaylandBufferManager::CreateBuffer(base::File file,
                                        uint32_t width,
                                        uint32_t height,
                                        const std::vector<uint32_t>& strides,
                                        const std::vector<uint32_t>& offsets,
                                        uint32_t format,
                                        const std::vector<uint64_t>& modifiers,
                                        uint32_t planes_count,
                                        uint32_t buffer_id) {
  TRACE_EVENT2("Wayland", "WaylandBufferManager::CreateZwpLinuxDmabuf",
               "Format", format, "Buffer id", buffer_id);

  static const struct zwp_linux_buffer_params_v1_listener params_listener = {
      WaylandBufferManager::CreateSucceeded,
      WaylandBufferManager::CreateFailed};
  if (!ValidateDataFromGpu(file, width, height, strides, offsets, format,
                           modifiers, planes_count, buffer_id)) {
    // base::File::Close() has an assertion that checks if blocking operations
    // are allowed. Thus, manually close the fd here.
    base::ScopedFD fd(file.TakePlatformFile());
    fd.reset();
    return false;
  }

  // Store |params| connected to |buffer_id| to track buffer creation and
  // identify, which buffer a client wants to use.
  DCHECK(zwp_linux_dmabuf_);
  struct zwp_linux_buffer_params_v1* params =
      zwp_linux_dmabuf_v1_create_params(zwp_linux_dmabuf_.get());

  std::unique_ptr<Buffer> buffer =
      std::make_unique<Buffer>(buffer_id, params, gfx::Size(width, height));
  buffers_.insert(std::pair<uint32_t, std::unique_ptr<Buffer>>(
      buffer_id, std::move(buffer)));

  uint32_t fd = file.TakePlatformFile();
  for (size_t i = 0; i < planes_count; i++) {
    zwp_linux_buffer_params_v1_add(params, fd, i /* plane id */, offsets[i],
                                   strides[i], modifiers[i] >> 32,
                                   modifiers[i] & UINT32_MAX);
  }
  zwp_linux_buffer_params_v1_add_listener(params, &params_listener, this);
  zwp_linux_buffer_params_v1_create(params, width, height, format, 0);

  connection_->ScheduleFlush();
  return true;
}

// TODO(msisov): handle buffer swap failure or success.
bool WaylandBufferManager::ScheduleBufferSwap(gfx::AcceleratedWidget widget,
                                              uint32_t buffer_id,
                                              const gfx::Rect& damage_region,
                                              wl::BufferSwapCallback callback) {
  TRACE_EVENT1("Wayland", "WaylandBufferManager::ScheduleSwapBuffer",
               "Buffer id", buffer_id);

  if (!ValidateDataFromGpu(widget, buffer_id))
    return false;

  auto it = buffers_.find(buffer_id);
  if (it == buffers_.end()) {
    error_message_ =
        "Buffer with " + std::to_string(buffer_id) + " id does not exist";
    return false;
  }

  Buffer* buffer = it->second.get();
  DCHECK(buffer);

  // Assign a widget to this buffer, which is used to find a corresponding
  // WaylandWindow.
  buffer->widget = widget;
  buffer->buffer_swap_callback = std::move(callback);
  buffer->damage_region = damage_region;

  if (buffer->wl_buffer) {
    // A wl_buffer might not exist by this time. Silently return.
    // TODO: check this.
    return SwapBuffer(buffer);
  }
  return true;
}

bool WaylandBufferManager::DestroyBuffer(uint32_t buffer_id) {
  TRACE_EVENT1("Wayland", "WaylandBufferManager::DestroyZwpLinuxDmabuf",
               "Buffer id", buffer_id);

  auto it = buffers_.find(buffer_id);
  if (it == buffers_.end()) {
    error_message_ = "Trying to destroy non-existing buffer";
    return false;
  }
  // It can happen that a buffer is destroyed before a frame callback comes.
  // Thus, just mark this as a successful swap, which is ok to do.
  Buffer* buffer = it->second.get();
  if (!buffer->buffer_swap_callback.is_null()) {
    std::move(buffer->buffer_swap_callback)
        .Run(gfx::SwapResult::SWAP_ACK,
             gfx::PresentationFeedback(base::TimeTicks::Now(),
                                       base::TimeDelta(), 0));
  }
  buffers_.erase(it);

  connection_->ScheduleFlush();
  return true;
}

void WaylandBufferManager::ClearState() {
  buffers_.clear();
}

// TODO(msisov): handle buffer swap failure or success.
bool WaylandBufferManager::SwapBuffer(Buffer* buffer) {
  TRACE_EVENT1("Wayland", "WaylandBufferManager::SwapBuffer", "Buffer id",
               buffer->buffer_id);

  WaylandWindow* window = connection_->GetWindow(buffer->widget);
  if (!window) {
    error_message_ = "A WaylandWindow with current widget does not exist";
    return false;
  }

  gfx::Rect damage_region = buffer->damage_region;
  // If the size of the damage region is empty, wl_surface_damage must be
  // supplied with the actual size of the buffer, which is going to be
  // committed.
  if (damage_region.size().IsEmpty())
    damage_region.set_size(buffer->size);

  wl_surface_damage_buffer(window->surface(), damage_region.x(),
                           damage_region.y(), damage_region.width(),
                           damage_region.height());
  wl_surface_attach(window->surface(), buffer->wl_buffer.get(), 0, 0);

  static const wl_callback_listener frame_listener = {
      WaylandBufferManager::FrameCallbackDone};
  DCHECK(!buffer->wl_frame_callback);
  buffer->wl_frame_callback.reset(wl_surface_frame(window->surface()));
  wl_callback_add_listener(buffer->wl_frame_callback.get(), &frame_listener,
                           this);

  // Set up presentation feedback.
  static const wp_presentation_feedback_listener feedback_listener = {
      WaylandBufferManager::FeedbackSyncOutput,
      WaylandBufferManager::FeedbackPresented,
      WaylandBufferManager::FeedbackDiscarded};
  if (connection_->presentation()) {
    DCHECK(!buffer->wp_presentation_feedback);
    buffer->wp_presentation_feedback.reset(wp_presentation_feedback(
        connection_->presentation(), window->surface()));
    wp_presentation_feedback_add_listener(
        buffer->wp_presentation_feedback.get(), &feedback_listener, this);
  }

  wl_surface_commit(window->surface());

  connection_->ScheduleFlush();
  return true;
}

bool WaylandBufferManager::ValidateDataFromGpu(
    const base::File& file,
    uint32_t width,
    uint32_t height,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    uint32_t format,
    const std::vector<uint64_t>& modifiers,
    uint32_t planes_count,
    uint32_t buffer_id) {
  std::string reason;
  if (!file.IsValid())
    reason = "Buffer fd is invalid";

  if (width == 0 || height == 0)
    reason = "Buffer size is invalid";

  if (planes_count < 1)
    reason = "Planes count cannot be less than 1";

  if (planes_count != strides.size() || planes_count != offsets.size() ||
      planes_count != modifiers.size()) {
    reason = "Number of strides(" + std::to_string(strides.size()) +
             ")/offsets(" + std::to_string(offsets.size()) + ")/modifiers(" +
             std::to_string(modifiers.size()) +
             ") does not correspond to the number of planes(" +
             std::to_string(planes_count) + ")";
  }

  for (auto stride : strides) {
    if (stride == 0)
      reason = "Strides are invalid";
  }

  if (!IsValidBufferFormat(format))
    reason = "Buffer format is invalid";

  if (buffer_id < 1)
    reason = "Invalid buffer id: " + std::to_string(buffer_id);

  auto it = buffers_.find(buffer_id);
  if (it != buffers_.end()) {
    reason = "A buffer with " + std::to_string(buffer_id) +
             " id has already existed";
  }

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }
  return true;
}

bool WaylandBufferManager::ValidateDataFromGpu(
    const gfx::AcceleratedWidget& widget,
    uint32_t buffer_id) {
  std::string reason;

  if (widget == gfx::kNullAcceleratedWidget)
    reason = "Invalid accelerated widget";

  if (buffer_id < 1)
    reason = "Invalid buffer id: " + std::to_string(buffer_id);

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }

  return true;
}

void WaylandBufferManager::CreateSucceededInternal(
    struct zwp_linux_buffer_params_v1* params,
    struct wl_buffer* new_buffer) {
  // Find which buffer id |params| belong to and store wl_buffer
  // with that id.
  Buffer* buffer = nullptr;
  for (auto& item : buffers_) {
    if (item.second.get()->params == params) {
      buffer = item.second.get();
      break;
    }
  }

  // It can happen that buffer was destroyed by a client while the Wayland
  // compositor was processing a request to create a wl_buffer.
  if (!buffer)
    return;

  buffer->wl_buffer.reset(new_buffer);
  buffer->params = nullptr;
  zwp_linux_buffer_params_v1_destroy(params);

  if (buffer->widget != gfx::kNullAcceleratedWidget)
    SwapBuffer(buffer);
}

void WaylandBufferManager::OnBufferSwapped(Buffer* buffer) {
  DCHECK(!buffer->buffer_swap_callback.is_null());
  std::move(buffer->buffer_swap_callback)
      .Run(buffer->swap_result, std::move(buffer->feedback));
}

void WaylandBufferManager::AddSupportedFourCCFormat(uint32_t fourcc_format) {
  // Return on not the supported fourcc format.
  if (!IsValidBufferFormat(fourcc_format))
    return;

  // It can happen that ::Format or ::Modifiers call can have already added such
  // a format. Thus, we can ignore that format.
  gfx::BufferFormat format = GetBufferFormatFromFourCCFormat(fourcc_format);
  auto it = std::find(supported_buffer_formats_.begin(),
                      supported_buffer_formats_.end(), format);
  if (it != supported_buffer_formats_.end())
    return;
  supported_buffer_formats_.push_back(format);
}

// static
void WaylandBufferManager::Modifiers(
    void* data,
    struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
    uint32_t format,
    uint32_t modifier_hi,
    uint32_t modifier_lo) {
  WaylandBufferManager* self = static_cast<WaylandBufferManager*>(data);
  if (self)
    self->AddSupportedFourCCFormat(format);
}

// static
void WaylandBufferManager::Format(void* data,
                                  struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                                  uint32_t format) {
  WaylandBufferManager* self = static_cast<WaylandBufferManager*>(data);
  if (self)
    self->AddSupportedFourCCFormat(format);
}

// static
void WaylandBufferManager::CreateSucceeded(
    void* data,
    struct zwp_linux_buffer_params_v1* params,
    struct wl_buffer* new_buffer) {
  WaylandBufferManager* self = static_cast<WaylandBufferManager*>(data);
  DCHECK(self);
  self->CreateSucceededInternal(params, new_buffer);
}

// static
void WaylandBufferManager::CreateFailed(
    void* data,
    struct zwp_linux_buffer_params_v1* params) {
  zwp_linux_buffer_params_v1_destroy(params);
  LOG(FATAL) << "zwp_linux_buffer_params.create failed";
}

// static
void WaylandBufferManager::FrameCallbackDone(void* data,
                                             wl_callback* callback,
                                             uint32_t time) {
  WaylandBufferManager* self = static_cast<WaylandBufferManager*>(data);
  DCHECK(self);
  for (auto& item : self->buffers_) {
    Buffer* buffer = item.second.get();
    if (buffer->wl_frame_callback.get() == callback) {
      buffer->swap_result = gfx::SwapResult::SWAP_ACK;
      buffer->wl_frame_callback.reset();

      // If presentation feedback is not supported, use fake feedback and
      // trigger the callback.
      if (!self->connection_->presentation()) {
        buffer->feedback = gfx::PresentationFeedback(base::TimeTicks::Now(),
                                                     base::TimeDelta(), 0);
        self->OnBufferSwapped(buffer);
      }
      return;
    }
  }

  NOTREACHED();
}

// static
void WaylandBufferManager::FeedbackSyncOutput(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback,
    struct wl_output* output) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void WaylandBufferManager::FeedbackPresented(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback,
    uint32_t tv_sec_hi,
    uint32_t tv_sec_lo,
    uint32_t tv_nsec,
    uint32_t refresh,
    uint32_t seq_hi,
    uint32_t seq_lo,
    uint32_t flags) {
  WaylandBufferManager* self = static_cast<WaylandBufferManager*>(data);
  DCHECK(self);

  for (auto& item : self->buffers_) {
    Buffer* buffer = item.second.get();
    if (buffer->wp_presentation_feedback.get() == wp_presentation_feedback) {
      // Frame callback must come before a feedback is presented.
      DCHECK(!buffer->wl_frame_callback);

      buffer->feedback = gfx::PresentationFeedback(
          GetPresentationFeedbackTimeStamp(tv_sec_hi, tv_sec_lo, tv_nsec),
          base::TimeDelta::FromNanoseconds(refresh),
          GetPresentationKindFlags(flags));

      buffer->wp_presentation_feedback.reset();
      self->OnBufferSwapped(buffer);
      return;
    }
  }

  NOTREACHED();
}

// static
void WaylandBufferManager::FeedbackDiscarded(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback) {
  WaylandBufferManager* self = static_cast<WaylandBufferManager*>(data);
  DCHECK(self);

  for (auto& item : self->buffers_) {
    Buffer* buffer = item.second.get();
    if (buffer->wp_presentation_feedback.get() == wp_presentation_feedback) {
      // Frame callback must come before a feedback is presented.
      DCHECK(!buffer->wl_frame_callback);
      buffer->feedback = gfx::PresentationFeedback::Failure();

      buffer->wp_presentation_feedback.reset();
      self->OnBufferSwapped(buffer);
      return;
    }
  }

  NOTREACHED();
}

}  // namespace ui
