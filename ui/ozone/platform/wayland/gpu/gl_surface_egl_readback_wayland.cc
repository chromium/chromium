// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/gl_surface_egl_readback_wayland.h"

#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/checked_math.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"

namespace ui {

namespace {

constexpr size_t kMaxBuffers = 2;

constexpr size_t kBytesPerPixelBGRA = 4;

}  // namespace

GLSurfaceEglReadbackWayland::PixelBuffer::PixelBuffer(
    base::WritableSharedMemoryMapping shm_mapping,
    uint32_t buffer_id)
    : shm_mapping_(std::move(shm_mapping)), buffer_id_(buffer_id) {}

GLSurfaceEglReadbackWayland::PixelBuffer::~PixelBuffer() = default;

GLSurfaceEglReadbackWayland::GLSurfaceEglReadbackWayland(
    gl::GLDisplayEGL* display,
    gfx::AcceleratedWidget widget,
    WaylandBufferManagerGpu* buffer_manager)
    : GLSurfaceEglReadback(display),
      widget_(widget),
      buffer_manager_(buffer_manager) {
  buffer_manager_->RegisterSurface(widget_, this);
}

void GLSurfaceEglReadbackWayland::Destroy() {
  DestroyBuffers();
  buffer_manager_->UnregisterSurface(widget_);

  PbufferGLSurfaceEGL::Destroy();
}

bool GLSurfaceEglReadbackWayland::Resize(const gfx::Size& size,
                                         float scale_factor,
                                         const gfx::ColorSpace& color_space,
                                         bool has_alpha) {
  DestroyBuffers();
  surface_scale_factor_ = scale_factor;
  pending_frames_ = 0;

  if (!PbufferGLSurfaceEGL::Resize(size, scale_factor, color_space, has_alpha))
    return false;

  for (size_t i = 0; i < kMaxBuffers; ++i) {
    base::CheckedNumeric<size_t> checked_length(size.width());
    checked_length *= size.height();
    checked_length *= kBytesPerPixelBGRA;
    if (!checked_length.IsValid())
      return false;

    base::UnsafeSharedMemoryRegion shm_region =
        base::UnsafeSharedMemoryRegion::Create(checked_length.ValueOrDie());
    if (!shm_region.IsValid())
      return false;

    auto shm_mapping = shm_region.Map();
    if (!shm_mapping.IsValid())
      return false;

    base::subtle::PlatformSharedMemoryRegion platform_shm =
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            std::move(shm_region));
    base::subtle::ScopedFDPair fd_pair = platform_shm.PassPlatformHandle();

    auto buffer_id = buffer_manager_->AllocateBufferID();
    available_buffers_.push_back(
        std::make_unique<PixelBuffer>(std::move(shm_mapping), buffer_id));

    buffer_manager_->CreateShmBasedBuffer(
        std::move(fd_pair.fd), checked_length.ValueOrDie(), size, buffer_id);
  }

  return true;
}

bool GLSurfaceEglReadbackWayland::SupportsAsyncSwap() {
  return true;
}

gfx::SwapResult GLSurfaceEglReadbackWayland::SwapBuffers(
    PresentationCallback callback,
    gfx::FrameData data) {
  NOTREACHED_IN_MIGRATION();
  return gfx::SwapResult::SWAP_FAILED;
}

void GLSurfaceEglReadbackWayland::SwapBuffersAsync(
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback,
    gfx::FrameData data) {
  DCHECK(pending_frames_ < kMaxBuffers);

  // Increase pending frames number.
  ++pending_frames_;

  completion_callbacks_.push_back(std::move(completion_callback));
  presentation_callbacks_.push_back(std::move(presentation_callback));

  DCHECK(!available_buffers_.empty());
  in_flight_pixel_buffers_.push_back(std::move(available_buffers_.front()));
  auto* next_buffer = in_flight_pixel_buffers_.back().get();
  available_buffers_.erase(available_buffers_.begin());

  ReadPixels(next_buffer->shm_mapping_);

  const auto bounds = gfx::Rect(GetSize());
  constexpr bool enable_blend_for_shadow = true;
  buffer_manager_->CommitBuffer(widget_, next_buffer->buffer_id_,
                                /*frame_id*/ next_buffer->buffer_id_, data,
                                bounds, enable_blend_for_shadow,
                                gfx::RoundedCornersF(), surface_scale_factor_,
                                bounds);
}

gfx::SurfaceOrigin GLSurfaceEglReadbackWayland::GetOrigin() const {
  // GLSurfaceEglReadbackWayland's y-axis is flipped compare to GL - (0,0) is at
  // top left corner.
  return gfx::SurfaceOrigin::kTopLeft;
}

GLSurfaceEglReadbackWayland::~GLSurfaceEglReadbackWayland() {
  Destroy();
}

void GLSurfaceEglReadbackWayland::OnSubmission(
    uint32_t frame_id,
    const gfx::SwapResult& swap_result,
    gfx::GpuFenceHandle release_fence) {
  DCHECK(release_fence.is_null());
  --pending_frames_;

  if (in_flight_pixel_buffers_.front()) {
    if (displayed_buffer_)
      available_buffers_.push_back(std::move(displayed_buffer_));
    displayed_buffer_ = std::move(in_flight_pixel_buffers_.front());
    DCHECK_EQ(displayed_buffer_->buffer_id_, frame_id);
  }

  in_flight_pixel_buffers_.pop_front();

  DCHECK(!completion_callbacks_.empty());
  std::move(completion_callbacks_.front())
      .Run(gfx::SwapCompletionResult(swap_result));
  completion_callbacks_.erase(completion_callbacks_.begin());
}

void GLSurfaceEglReadbackWayland::OnPresentation(
    uint32_t frame_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(!presentation_callbacks_.empty());
  std::move(presentation_callbacks_.front()).Run(feedback);
  presentation_callbacks_.erase(presentation_callbacks_.begin());
}

void GLSurfaceEglReadbackWayland::DestroyBuffers() {
  for (const auto& pixel_buffer : available_buffers_)
    buffer_manager_->DestroyBuffer(pixel_buffer->buffer_id_);
  for (const auto& pixel_buffer : in_flight_pixel_buffers_)
    buffer_manager_->DestroyBuffer(pixel_buffer->buffer_id_);

  if (displayed_buffer_)
    buffer_manager_->DestroyBuffer(displayed_buffer_->buffer_id_);

  available_buffers_.clear();
  in_flight_pixel_buffers_.clear();
  displayed_buffer_.reset();
}

}  // namespace ui
