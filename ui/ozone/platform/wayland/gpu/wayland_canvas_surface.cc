// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_canvas_surface.h"

#include <memory>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/checked_math.h"
#include "base/posix/eintr_wrapper.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"

namespace ui {

namespace {

// The maximum number of buffers we allow to be created.
constexpr uint32_t kMaxNumbferOfBuffers = 3;

}  // namespace

size_t CalculateStride(int width) {
  base::CheckedNumeric<size_t> stride(width);
  stride *= 4;
  return stride.ValueOrDie();
}

class WaylandCanvasSurface::SharedMemoryBuffer {
 public:
  SharedMemoryBuffer(gfx::AcceleratedWidget widget,
                     WaylandBufferManagerGpu* buffer_manager)
      : buffer_id_(buffer_manager->AllocateBufferID()),
        widget_(widget),
        buffer_manager_(buffer_manager) {
    DCHECK(buffer_manager_);
  }
  ~SharedMemoryBuffer() { buffer_manager_->DestroyBuffer(widget_, buffer_id_); }

  // Returns SkSurface, which the client can use to write to this buffer.
  sk_sp<SkSurface> sk_surface() const { return sk_surface_; }

  // Returns the id of the buffer.
  uint32_t buffer_id() const { return buffer_id_; }

  // Tells if the buffer is currently being used.
  bool used() const { return used_; }

  // Initializes the shared memory and asks Wayland to import a shared memory
  // based wl_buffer, which can be attached to a surface and have its contents
  // shown on a screen.
  bool Initialize(const gfx::Size& size) {
    base::CheckedNumeric<size_t> checked_length(size.width());
    checked_length *= size.height();
    checked_length *= 4;
    if (!checked_length.IsValid())
      return false;

    base::UnsafeSharedMemoryRegion shm_region =
        base::UnsafeSharedMemoryRegion::Create(checked_length.ValueOrDie());
    if (!shm_region.IsValid())
      return false;

    shm_mapping_ = shm_region.Map();
    if (!shm_mapping_.IsValid())
      return false;

    base::subtle::PlatformSharedMemoryRegion platform_shm =
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            std::move(shm_region));
    base::subtle::ScopedFDPair fd_pair = platform_shm.PassPlatformHandle();
    buffer_manager_->CreateShmBasedBuffer(
        std::move(fd_pair.fd), checked_length.ValueOrDie(), size, buffer_id_);

    sk_surface_ = SkSurface::MakeRasterDirect(
        SkImageInfo::MakeN32Premul(size.width(), size.height()),
        shm_mapping_.memory(), CalculateStride(size.width()));
    if (!sk_surface_)
      return false;

    dirty_region_.setRect(gfx::RectToSkIRect(gfx::Rect(size)));
    return true;
  }

  void CommitBuffer(const gfx::Rect& damage) {
    buffer_manager_->CommitBuffer(widget_, buffer_id_, damage);
  }

  void OnUse() {
    DCHECK(!used_);
    used_ = true;
  }

  void OnRelease() {
    DCHECK(used_);
    used_ = false;
  }

  void UpdateDirtyRegion(const gfx::Rect& damage, SkRegion::Op op) {
    SkIRect sk_damage = gfx::RectToSkIRect(damage);
    dirty_region_.op(sk_damage, op);
  }

  void CopyDirtyRegionFrom(SharedMemoryBuffer* buffer) {
    DCHECK_NE(this, buffer);
    const size_t stride = CalculateStride(sk_surface_->width());
    for (SkRegion::Iterator i(dirty_region_); !i.done(); i.next()) {
      uint8_t* dst_ptr =
          static_cast<uint8_t*>(shm_mapping_.memory()) +
          i.rect().x() * SkColorTypeBytesPerPixel(kN32_SkColorType) +
          i.rect().y() * stride;
      buffer->sk_surface_->readPixels(
          SkImageInfo::MakeN32Premul(i.rect().width(), i.rect().height()),
          dst_ptr, stride, i.rect().x(), i.rect().y());
    }
    dirty_region_.setEmpty();
  }

  void set_pending_damage_region(const gfx::Rect& damage) {
    pending_damage_region_ = damage;
  }

  const gfx::Rect& pending_damage_region() const {
    return pending_damage_region_;
  }

 private:
  // The id of the buffer this surface is backed.
  const uint32_t buffer_id_;

  // Whether this buffer is currently being used.
  bool used_ = false;

  // The widget this buffer is created for.
  const gfx::AcceleratedWidget widget_;

  // Non-owned pointer to the buffer manager on the gpu process/thread side.
  WaylandBufferManagerGpu* const buffer_manager_;

  // Shared memory for the buffer.
  base::WritableSharedMemoryMapping shm_mapping_;

  // The SkSurface the clients can draw to show the surface on screen.
  sk_sp<SkSurface> sk_surface_;

  // The region of the buffer that's not up-to-date.
  SkRegion dirty_region_;

  // Pending damage region if the buffer is pending to be submitted.
  gfx::Rect pending_damage_region_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemoryBuffer);
};

WaylandCanvasSurface::WaylandCanvasSurface(
    WaylandBufferManagerGpu* buffer_manager,
    gfx::AcceleratedWidget widget)
    : buffer_manager_(buffer_manager), widget_(widget) {
  buffer_manager_->RegisterSurface(widget_, this);
}

WaylandCanvasSurface::~WaylandCanvasSurface() {
  buffer_manager_->UnregisterSurface(widget_);
}

SkCanvas* WaylandCanvasSurface::GetCanvas() {
  DCHECK(!pending_buffer_)
      << "The previous pending buffer has not been presented yet";

  for (const auto& buffer : buffers_) {
    if (!buffer->used()) {
      pending_buffer_ = buffer.get();
      break;
    }
  }

  if (!pending_buffer_) {
    if (buffers_.size() >= kMaxNumbferOfBuffers) {
      // We have achieved the maximum number of buffers we can create. Wait for
      // a free buffer.
      return nullptr;
    }
    auto buffer = CreateSharedMemoryBuffer();
    if (!buffer)
      return nullptr;

    pending_buffer_ = buffer.get();
    buffers_.push_back(std::move(buffer));
  }

  DCHECK(pending_buffer_);
  pending_buffer_->OnUse();
  return pending_buffer_->sk_surface()->getCanvas();
}

void WaylandCanvasSurface::ResizeCanvas(const gfx::Size& viewport_size) {
  if (size_ == viewport_size)
    return;
  // TODO(https://crbug.com/930667): We could implement more efficient resizes
  // by allocating buffers rounded up to larger sizes, and then reusing them if
  // the new size still fits (but still reallocate if the new size is much
  // smaller than the old size).
  buffers_.clear();
  current_buffer_ = nullptr;
  previous_buffer_ = nullptr;
  pending_buffer_ = nullptr;
  unsubmitted_buffers_.clear();
  size_ = viewport_size;
}

void WaylandCanvasSurface::PresentCanvas(const gfx::Rect& damage) {
  if (!pending_buffer_)
    return;

  pending_buffer_->set_pending_damage_region(damage);
  unsubmitted_buffers_.push_back(pending_buffer_);
  pending_buffer_ = nullptr;

  ProcessUnsubmittedBuffers();
}

std::unique_ptr<gfx::VSyncProvider>
WaylandCanvasSurface::CreateVSyncProvider() {
  // TODO(https://crbug.com/930662): This can be implemented with information
  // from presentation feedback.
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

void WaylandCanvasSurface::ProcessUnsubmittedBuffers() {
  DCHECK(!unsubmitted_buffers_.empty() && unsubmitted_buffers_.front()->used());

  // Don't submit a new buffer if there's one already submitted being
  // processed.
  if (current_buffer_)
    return;

  current_buffer_ = std::move(unsubmitted_buffers_.front());
  unsubmitted_buffers_.erase(unsubmitted_buffers_.begin());

  gfx::Rect damage = current_buffer_->pending_damage_region();

  // The buffer has been updated. Thus, the |damage| can be subtracted
  // from its dirty region.
  current_buffer_->UpdateDirtyRegion(damage, SkRegion::kDifference_Op);

  // Make sure the buffer is up-to-date by copying the outdated region from
  // the previous buffer.
  if (previous_buffer_ && previous_buffer_ != current_buffer_)
    current_buffer_->CopyDirtyRegionFrom(previous_buffer_);

  // As long as the |current_buffer_| has been updated, add dirty region to
  // other buffers to make sure their regions will be updated with up-to-date
  // content.
  for (auto& buffer : buffers_) {
    if (buffer.get() != current_buffer_)
      buffer->UpdateDirtyRegion(damage, SkRegion::kUnion_Op);
  }

  current_buffer_->CommitBuffer(damage);
}

void WaylandCanvasSurface::OnSubmission(uint32_t buffer_id,
                                        const gfx::SwapResult& swap_result) {
  // We may get an OnSubmission callback for a buffer that was submitted
  // before a ResizeCanvas call, which clears all our buffers. Check to
  // see if we still know about this buffer. If we know about this buffer
  // it must be |current_buffer_| because we only submit new buffers when
  // |current_buffer_| is nullptr, and it is only set to nullptr in
  // |OnSubmission| and |ResizeCanvas|. In |ResizeCanvas|, |buffers_| is cleared
  // so we will not know about |buffer_id|.
  if (std::none_of(buffers_.begin(), buffers_.end(),
                   [buffer_id](const auto& buffer) {
                     return buffer->buffer_id() == buffer_id;
                   }))
    return;

  DCHECK(current_buffer_);
  DCHECK_EQ(current_buffer_->buffer_id(), buffer_id);

  if (previous_buffer_)
    previous_buffer_->OnRelease();

  previous_buffer_ = current_buffer_;
  current_buffer_ = nullptr;

  if (!unsubmitted_buffers_.empty())
    ProcessUnsubmittedBuffers();
}

void WaylandCanvasSurface::OnPresentation(
    uint32_t buffer_id,
    const gfx::PresentationFeedback& feedback) {
  // TODO(https://crbug.com/930662): this can be used for the vsync provider.
  NOTIMPLEMENTED_LOG_ONCE();
}

std::unique_ptr<WaylandCanvasSurface::SharedMemoryBuffer>
WaylandCanvasSurface::CreateSharedMemoryBuffer() {
  DCHECK(!size_.IsEmpty());

  auto canvas_buffer =
      std::make_unique<SharedMemoryBuffer>(widget_, buffer_manager_);
  return canvas_buffer->Initialize(size_) ? std::move(canvas_buffer) : nullptr;
}

}  // namespace ui
