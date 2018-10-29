// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl_surface_control.h"

#include "base/threading/thread_task_runner_handle.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"

namespace gl {
namespace {

constexpr char kSurfaceName[] = "ChromeSurface";

}  // namespace

GLSurfaceEGLSurfaceControl::GLSurfaceEGLSurfaceControl(ANativeWindow* window) {
  surface_composer_ = SurfaceComposer::Create(window);
}

GLSurfaceEGLSurfaceControl::~GLSurfaceEGLSurfaceControl() = default;

bool GLSurfaceEGLSurfaceControl::Initialize(GLSurfaceFormat format) {
  format_ = format;
  return true;
}

void GLSurfaceEGLSurfaceControl::Destroy() {
  pending_transaction_.reset();
  surface_list_.clear();
  surface_composer_.reset();
}

bool GLSurfaceEGLSurfaceControl::Resize(const gfx::Size& size,
                                        float scale_factor,
                                        ColorSpace color_space,
                                        bool has_alpha) {
  // Resizing requires resizing the SurfaceView in the browser.
  return true;
}

bool GLSurfaceEGLSurfaceControl::IsOffscreen() {
  return false;
}

gfx::SwapResult GLSurfaceEGLSurfaceControl::SwapBuffers(
    const PresentationCallback& callback) {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

void GLSurfaceEGLSurfaceControl::SwapBuffersAsync(
    const SwapCompletionCallback& completion_callback,
    const PresentationCallback& presentation_callback) {
  CommitPendingTransaction(completion_callback, presentation_callback);
}

gfx::SwapResult GLSurfaceEGLSurfaceControl::CommitOverlayPlanes(
    const PresentationCallback& callback) {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

void GLSurfaceEGLSurfaceControl::CommitOverlayPlanesAsync(
    const SwapCompletionCallback& completion_callback,
    const PresentationCallback& presentation_callback) {
  CommitPendingTransaction(completion_callback, presentation_callback);
}

void GLSurfaceEGLSurfaceControl::CommitPendingTransaction(
    const SwapCompletionCallback& completion_callback,
    const PresentationCallback& present_callback) {
  DCHECK(pending_transaction_);

  // Release resources for the current frame once the next frame is acked.
  ResourceRefs resources_to_release;
  resources_to_release.swap(current_frame_resources_);
  current_frame_resources_.clear();

  // Track resources to be owned by the framework after this transaction.
  current_frame_resources_.swap(pending_frame_resources_);
  pending_frame_resources_.clear();

  pending_transaction_->Apply();
  pending_transaction_.reset();

  DCHECK_GE(surface_list_.size(), pending_surfaces_count_);
  surface_list_.resize(pending_surfaces_count_);
  pending_surfaces_count_ = 0u;

  // TODO(khushalsagar): Send the legit timestamp when hooking up transaction
  // acks.
  constexpr int64_t kRefreshIntervalInMicroseconds =
      base::Time::kMicrosecondsPerSecond / 60;
  gfx::PresentationFeedback feedback(
      base::TimeTicks::Now(),
      base::TimeDelta::FromMicroseconds(kRefreshIntervalInMicroseconds),
      0 /* flags */);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(OnTransactionAck, feedback, present_callback,
                     completion_callback, std::move(resources_to_release)));
}

gfx::Size GLSurfaceEGLSurfaceControl::GetSize() {
  return gfx::Size(0, 0);
}

bool GLSurfaceEGLSurfaceControl::OnMakeCurrent(GLContext* context) {
  return true;
}

bool GLSurfaceEGLSurfaceControl::ScheduleOverlayPlane(
    int z_order,
    gfx::OverlayTransform transform,
    GLImage* image,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  if (!pending_transaction_)
    pending_transaction_.emplace();

  bool uninitialized = false;
  if (pending_surfaces_count_ == surface_list_.size()) {
    uninitialized = true;
    surface_list_.emplace_back(surface_composer_.get());
  }
  pending_surfaces_count_++;
  auto& surface_state = surface_list_.at(pending_surfaces_count_ - 1);

  if (uninitialized || surface_state.z_order != z_order) {
    surface_state.z_order = z_order;
    pending_transaction_->SetZOrder(surface_state.surface, z_order);
  }

  if (uninitialized || surface_state.transform != transform) {
    surface_state.transform = transform;
    // TODO(khushalsagar): Forward the transform once the NDK API is in place.
  }

  AHardwareBuffer* hardware_buffer = nullptr;
  base::ScopedFD fence_fd;
  auto scoped_hardware_buffer = image->GetAHardwareBuffer();
  if (scoped_hardware_buffer) {
    hardware_buffer = scoped_hardware_buffer->buffer();
    fence_fd = scoped_hardware_buffer->TakeFence();
    pending_frame_resources_.push_back(std::move(scoped_hardware_buffer));
  }

  if (uninitialized || surface_state.hardware_buffer != hardware_buffer) {
    surface_state.hardware_buffer = hardware_buffer;

    if (!fence_fd.is_valid() && gpu_fence && surface_state.hardware_buffer) {
      auto fence_handle =
          gfx::CloneHandleForIPC(gpu_fence->GetGpuFenceHandle());
      DCHECK(!fence_handle.is_null());
      fence_fd = base::ScopedFD(fence_handle.native_fd.fd);
    }

    pending_transaction_->SetBuffer(surface_state.surface,
                                    surface_state.hardware_buffer,
                                    std::move(fence_fd));
  }

  if (uninitialized || surface_state.bounds_rect != bounds_rect) {
    surface_state.bounds_rect = bounds_rect;
    pending_transaction_->SetPosition(surface_state.surface, bounds_rect.x(),
                                      bounds_rect.y());
    pending_transaction_->SetSize(surface_state.surface, bounds_rect.width(),
                                  bounds_rect.height());
  }

  // TODO(khushalsagar): Currently the framework refuses to the draw the buffer
  // if the crop rect doesn't exactly match the buffer size. Update when fixed.
  /*gfx::Rect enclosed_crop_rect = gfx::ToEnclosedRect(crop_rect);
  if (uninitialized || surface_state.crop_rect != enclosed_crop_rect) {
    surface_state.crop_rect = enclosed_crop_rect;
    pending_transaction_->SetCropRect(
        surface_state.surface, enclosed_crop_rect.x(), enclosed_crop_rect.y(),
        enclosed_crop_rect.right(), enclosed_crop_rect.bottom());
  }*/

  bool opaque = !enable_blend;
  if (uninitialized || surface_state.opaque != opaque) {
    surface_state.opaque = opaque;
    pending_transaction_->SetOpaque(surface_state.surface, opaque);
  }

  return true;
}

bool GLSurfaceEGLSurfaceControl::IsSurfaceless() const {
  return true;
}

void* GLSurfaceEGLSurfaceControl::GetHandle() {
  return nullptr;
}

bool GLSurfaceEGLSurfaceControl::SupportsAsyncSwap() {
  return true;
}

bool GLSurfaceEGLSurfaceControl::SupportsPlaneGpuFences() const {
  return true;
}

bool GLSurfaceEGLSurfaceControl::SupportsPresentationCallback() {
  return true;
}

bool GLSurfaceEGLSurfaceControl::SupportsSwapBuffersWithBounds() {
  // TODO(khushalsagar): Add support for partial swap.
  return false;
}

bool GLSurfaceEGLSurfaceControl::SupportsCommitOverlayPlanes() {
  return true;
}

// static
void GLSurfaceEGLSurfaceControl::OnTransactionAck(
    const gfx::PresentationFeedback& feedback,
    const PresentationCallback& present_callback,
    const SwapCompletionCallback& completion_callback,
    ResourceRefs resources) {
  completion_callback.Run(gfx::SwapResult::SWAP_ACK, nullptr);
  present_callback.Run(feedback);
  resources.clear();
}

GLSurfaceEGLSurfaceControl::SurfaceState::SurfaceState(
    SurfaceComposer* composer)
    : surface(composer,
              SurfaceComposer::SurfaceContentType::kAHardwareBuffer,
              kSurfaceName) {}

GLSurfaceEGLSurfaceControl::SurfaceState::SurfaceState() = default;
GLSurfaceEGLSurfaceControl::SurfaceState::SurfaceState(SurfaceState&& other) =
    default;
GLSurfaceEGLSurfaceControl::SurfaceState&
GLSurfaceEGLSurfaceControl::SurfaceState::operator=(SurfaceState&& other) =
    default;

GLSurfaceEGLSurfaceControl::SurfaceState::~SurfaceState() = default;

}  // namespace gl
