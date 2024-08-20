// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_canvas_surface.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/checked_math.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/memory.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"

namespace ui {

namespace {

// The maximum number of buffers we allow to be created.
constexpr size_t kMaxNumberOfBuffers = 3;

}  // namespace

size_t CalculateStride(int width) {
  base::CheckedNumeric<size_t> stride(width);
  stride *= 4;
  return stride.ValueOrDie();
}

class WaylandCanvasSurface::SharedMemoryBuffer {
 public:
  explicit SharedMemoryBuffer(WaylandBufferManagerGpu* buffer_manager)
      : buffer_id_(buffer_manager->AllocateBufferID()),
        buffer_manager_(buffer_manager) {
    DCHECK(buffer_manager_);
  }

  SharedMemoryBuffer(const SharedMemoryBuffer&) = delete;
  SharedMemoryBuffer& operator=(const SharedMemoryBuffer&) = delete;

  ~SharedMemoryBuffer() { buffer_manager_->DestroyBuffer(buffer_id_); }

  // Returns SkSurface, which the client can use to write to this buffer.
  sk_sp<SkSurface> sk_surface() const { return sk_surface_; }

  // Returns the id of the buffer.
  uint32_t buffer_id() const { return buffer_id_; }

  // Tells if the buffer is currently being used.
  bool used() const { return used_; }

  // Initializes the shared memory and asks Wayland to import a shared memory
  // based wl_buffer, which can be attached to a surface and have its contents
  // shown on a screen.
  void Initialize(const gfx::Size& size) {
    size_ = size;

    // The format can either be RGBA_8888 or RGBX_8888 but either way it's 4
    // bytes per pixel.
    size_t size_in_bytes = viz::ResourceSizes::CheckedSizeInBytes<size_t>(
        size, viz::SinglePlaneFormat::kRGBA_8888);

    base::UnsafeSharedMemoryRegion shm_region =
        base::UnsafeSharedMemoryRegion::Create(size_in_bytes);
    if (!shm_region.IsValid()) {
      base::TerminateBecauseOutOfMemory(size_in_bytes);
    }

    shm_mapping_ = shm_region.Map();
    if (!shm_mapping_.IsValid()) {
      base::TerminateBecauseOutOfMemory(size_in_bytes);
    }

    base::subtle::PlatformSharedMemoryRegion platform_shm =
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            std::move(shm_region));
    base::subtle::ScopedFDPair fd_pair = platform_shm.PassPlatformHandle();
    buffer_manager_->CreateShmBasedBuffer(std::move(fd_pair.fd), size_in_bytes,
                                          size, buffer_id_);

    SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
    sk_surface_ = SkSurfaces::WrapPixels(
        SkImageInfo::MakeN32Premul(size.width(), size.height()),
        shm_mapping_.memory(), CalculateStride(size.width()), &props);
    DCHECK(sk_surface_);

    dirty_region_.setRect(gfx::RectToSkIRect(gfx::Rect(size)));
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
    auto dst_span = base::span(shm_mapping_);
    for (SkRegion::Iterator i(dirty_region_); !i.done(); i.next()) {
      auto offset = i.rect().x() * SkColorTypeBytesPerPixel(kN32_SkColorType) +
                    i.rect().y() * stride;
      auto dst_subspan = dst_span.subspan(
          offset, i.rect().width() * i.rect().height() *
                      SkColorTypeBytesPerPixel(kN32_SkColorType));

      UNSAFE_TODO(buffer->sk_surface_->readPixels(
          SkImageInfo::MakeN32Premul(i.rect().width(), i.rect().height()),
          dst_subspan.data(), stride, i.rect().x(), i.rect().y()));
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
  // The size of the buffer.
  gfx::Size size_;

  // The id of the buffer this surface is backed.
  const uint32_t buffer_id_;

  // Whether this buffer is currently being used.
  bool used_ = false;

  // Non-owned pointer to the buffer manager on the gpu process/thread side.
  const raw_ptr<WaylandBufferManagerGpu> buffer_manager_;

  // Shared memory for the buffer.
  base::WritableSharedMemoryMapping shm_mapping_;

  // The SkSurface the clients can draw to show the surface on screen.
  sk_sp<SkSurface> sk_surface_;

  // The region of the buffer that's not up-to-date.
  SkRegion dirty_region_;

  // Pending damage region if the buffer is pending to be submitted.
  gfx::Rect pending_damage_region_;
};

class WaylandCanvasSurface::VSyncProvider : public gfx::VSyncProvider {
 public:
  explicit VSyncProvider(base::WeakPtr<WaylandCanvasSurface> surface)
      : surface_(surface) {}

  ~VSyncProvider() override = default;

  void GetVSyncParameters(UpdateVSyncCallback callback) override {
    base::TimeTicks timebase;
    base::TimeDelta interval;
    if (GetVSyncParametersIfAvailable(&timebase, &interval))
      std::move(callback).Run(timebase, interval);
  }

  bool GetVSyncParametersIfAvailable(base::TimeTicks* timebase,
                                     base::TimeDelta* interval) override {
    if (!surface_ || surface_->last_timestamp_.is_null())
      return false;
    *timebase = surface_->last_timestamp_;
    *interval = surface_->last_interval_;
    return true;
  }

  bool SupportGetVSyncParametersIfAvailable() const override { return true; }

  bool IsHWClock() const override {
    if (!surface_)
      return false;
    return surface_->is_hw_clock_;
  }

 private:
  base::WeakPtr<WaylandCanvasSurface> surface_;
};

WaylandCanvasSurface::PendingFrame::PendingFrame(
    uint32_t frame_id,
    const gfx::Size& surface_size,
    SwapBuffersCallback callback,
    gfx::FrameData frame_data,
    SharedMemoryBuffer* frame_buffer)
    : frame_id(frame_id),
      surface_size(surface_size),
      swap_ack_callback(std::move(callback)),
      data(std::move(frame_data)),
      frame_buffer(frame_buffer) {
  // The frame might be invalid if there is no frame buffer. However, if there
  // is a buffer, it must be marked as used.
  DCHECK(!frame_buffer || frame_buffer->used());
}

WaylandCanvasSurface::PendingFrame::~PendingFrame() {
  if (swap_ack_callback) {
    // Post a task for this ack callback as this frame may ack the submission
    // immediately without actually sending a frame to the host side, which
    // eventually calls OnSubmission before the callback is executed. This is
    // required to avoid situations when ack'ing a frame results in
    // submitting another frame. And if that last frame is immediately acked
    // as well, the previous frame may not be completely processed as its ack
    // results in a code path that submits a new frame. That is, viz::Display
    // may then see that the ack callback has wrong submission time (it's before
    // the draw start time).
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(swap_ack_callback), surface_size));
  }
}

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
    // It must be impossible that the maximum number of buffers that can be
    // created is achieved.
    DCHECK_LE(buffers_.size(), kMaxNumberOfBuffers);
    auto buffer = CreateSharedMemoryBuffer();
    pending_buffer_ = buffer.get();
    buffers_.push_back(std::move(buffer));
  }

  DCHECK(pending_buffer_);
  pending_buffer_->OnUse();
  return pending_buffer_->sk_surface()->getCanvas();
}

void WaylandCanvasSurface::ResizeCanvas(const gfx::Size& viewport_size,
                                        float scale) {
  if (size_ == viewport_size)
    return;
  // TODO(crbug.com/41440520): We could implement more efficient resizes
  // by allocating buffers rounded up to larger sizes, and then reusing them if
  // the new size still fits (but still reallocate if the new size is much
  // smaller than the old size).
  buffers_.clear();
  previous_buffer_ = nullptr;
  pending_buffer_ = nullptr;

  // First clear submitted frame, which will execute the pending swap ack
  // callback and only then clear unsubmitted ones. This helps to preserve order
  // of swap ack callbacks.
  submitted_frame_.reset();

  // We must preserve FIFO. Thus, manually destroy pending frames.
  for (auto& frame : unsubmitted_frames_) {
    frame.reset();
  }
  unsubmitted_frames_.clear();

  size_ = viewport_size;
  viewport_scale_ = scale;
}

void WaylandCanvasSurface::PresentCanvas(const gfx::Rect& damage) {
  if (!pending_buffer_)
    return;

  pending_buffer_->set_pending_damage_region(damage);
}

bool WaylandCanvasSurface::SupportsAsyncBufferSwap() const {
  return true;
}

void WaylandCanvasSurface::OnSwapBuffers(SwapBuffersCallback swap_ack_callback,
                                         gfx::FrameData data) {
  unsubmitted_frames_.push_back(std::make_unique<PendingFrame>(
      next_frame_id(), size_, std::move(swap_ack_callback), std::move(data),
      pending_buffer_));
  pending_buffer_ = nullptr;

  MaybeProcessUnsubmittedFrames();
}

std::unique_ptr<gfx::VSyncProvider>
WaylandCanvasSurface::CreateVSyncProvider() {
  return std::make_unique<WaylandCanvasSurface::VSyncProvider>(
      weak_factory_.GetWeakPtr());
}

bool WaylandCanvasSurface::SupportsOverridePlatformSize() const {
  return true;
}

void WaylandCanvasSurface::MaybeProcessUnsubmittedFrames() {
  // Don't submit a new frame if there's one already submitted being
  // processed.
  if (submitted_frame_ || unsubmitted_frames_.empty()) {
    return;
  }

  auto frame = std::move(unsubmitted_frames_.front());
  unsubmitted_frames_.erase(unsubmitted_frames_.begin());

  auto* frame_buffer = frame->frame_buffer.get();
  // There was no frame buffer associated with this frame. Skip that and process
  // the next frame.
  if (!frame_buffer) {
    // Reset the frame so that it executes the swap ack callback.
    frame.reset();
    MaybeProcessUnsubmittedFrames();
    return;
  }

  gfx::Rect damage = frame_buffer->pending_damage_region();

  // The buffer has been updated. Thus, the |damage| can be subtracted
  // from its dirty region.
  frame_buffer->UpdateDirtyRegion(damage, SkRegion::kDifference_Op);

  // Make sure the buffer is up-to-date by copying the outdated region from
  // the previous buffer.
  if (previous_buffer_ && previous_buffer_ != frame_buffer) {
    frame_buffer->CopyDirtyRegionFrom(previous_buffer_);
  }

  // As long as the current frame's buffer has been updated, add dirty region to
  // other buffers to make sure their regions will be updated with up-to-date
  // content.
  for (auto& buffer : buffers_) {
    if (buffer.get() != frame_buffer) {
      buffer->UpdateDirtyRegion(damage, SkRegion::kUnion_Op);
    }
  }

  constexpr bool enable_blend_for_shadow = true;
  buffer_manager_->CommitBuffer(
      widget_, frame->frame_id, frame_buffer->buffer_id(),
      std::move(frame->data), gfx::Rect(size_), enable_blend_for_shadow,
      gfx::RoundedCornersF(), viewport_scale_, damage);

  submitted_frame_ = std::move((frame));
}

void WaylandCanvasSurface::OnSubmission(uint32_t frame_id,
                                        const gfx::SwapResult& swap_result,
                                        gfx::GpuFenceHandle release_fence) {
  DCHECK(release_fence.is_null());
  // We may get an OnSubmission callback for a frame that was submitted
  // before a ResizeCanvas call, which clears all our submitted frame. Check to
  // see if we still know about this frame. If we know about this frame
  // it must have the same |frame_id| because we only submit new frames when
  // |submitted_frame_| is nullptr, and it is only set to nullptr in
  // |OnSubmission| and |ResizeCanvas|. In |ResizeCanvas|, |submitted_frame_|
  // is cleared so we will not know about |frame_id|. In addition to that, if
  // the frame_id is stale, the gpu process may have just recovered from a crash
  // so this frame_id can also be ignored.
  if (!submitted_frame_ || submitted_frame_->frame_id != frame_id) {
    return;
  }

  DCHECK_EQ(size_, submitted_frame_->surface_size);

  if (previous_buffer_)
    previous_buffer_->OnRelease();

  previous_buffer_ = submitted_frame_->frame_buffer;
  // The frame will automatically execute the swap ack callback on destruction.
  submitted_frame_.reset();

  MaybeProcessUnsubmittedFrames();
}

void WaylandCanvasSurface::OnPresentation(
    uint32_t frame_id,
    const gfx::PresentationFeedback& feedback) {
  last_timestamp_ = feedback.timestamp;
  last_interval_ = feedback.interval;
  is_hw_clock_ = feedback.flags & gfx::PresentationFeedback::Flags::kHWClock;
}

std::unique_ptr<WaylandCanvasSurface::SharedMemoryBuffer>
WaylandCanvasSurface::CreateSharedMemoryBuffer() {
  DCHECK(!size_.IsEmpty());

  auto canvas_buffer = std::make_unique<SharedMemoryBuffer>(buffer_manager_);
  canvas_buffer->Initialize(size_);
  return canvas_buffer;
}

}  // namespace ui
