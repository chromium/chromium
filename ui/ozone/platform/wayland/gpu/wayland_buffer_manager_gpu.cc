// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"

#include <surface-augmenter-client-protocol.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/process/process.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/version.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/platform/wayland/common/wayland_overlay_config.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_gpu.h"
#include "ui/ozone/public/overlay_plane.h"

#if defined(WAYLAND_GBM)
#include "ui/gfx/linux/gbm_wrapper.h"  // nogncheck
#include "ui/ozone/platform/wayland/gpu/drm_render_node_handle.h"
#endif

namespace ui {

WaylandBufferManagerGpu::WaylandBufferManagerGpu()
    : WaylandBufferManagerGpu(base::FilePath()) {}

WaylandBufferManagerGpu::WaylandBufferManagerGpu(
    const base::FilePath& drm_node_path) {
#if defined(WAYLAND_GBM)
  // The path_finder and the handle do syscalls, which are permitted before
  // the sandbox entry. After the gpu enters the sandbox, they fail. Thus,
  // we get node path from the platform instance and get a handle for that here.
  OpenAndStoreDrmRenderNodeFd(drm_node_path);
#endif

  // The WaylandBufferManagerGpu takes the task runner where it was created.
  // However, it might be null in tests and be available later after
  // initialization is done. Though, when this code runs outside tests, a race
  // between setting a task runner via ::Initialize and ::RegisterSurface may
  // happen, and a surface will never be registered. Thus, the following two
  // cases are possible:
  // 1) The WaylandBufferManagerGpu runs normally outside tests.
  // SingleThreadTaskRunner::CurrentDefaultHandle is set and it is passed during
  // construction and never changes.
  // 2) The WaylandBufferManagerGpu runs in unit tests and when it's created,
  // the task runner is not available and must be set later when ::Initialize is
  // called. In this case, there is no race between ::Initialize and
  // ::RegisterSurface and it's fine to defer setting the task runner.
  //
  // TODO(msisov): think about making unit tests initialize Ozone after task
  // runner is set that would allow to always set the task runner.
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    gpu_thread_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  } else {
    // In tests, the further calls might happen on a different sequence.
    // Otherwise, SingleThreadTaskRunner::CurrentDefaultHandle should have
    // already been set.
    DETACH_FROM_SEQUENCE(gpu_sequence_checker_);
  }
}

WaylandBufferManagerGpu::~WaylandBufferManagerGpu() = default;

void WaylandBufferManagerGpu::Initialize(
    mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host,
    const base::flat_map<::gfx::BufferFormat, std::vector<uint64_t>>&
        buffer_formats_with_modifiers,
    bool supports_dma_buf,
    bool supports_viewporter,
    bool supports_acquire_fence,
    bool supports_overlays,
    uint32_t supported_surface_augmentor_version,
    bool supports_single_pixel_buffer,
    const base::Version& server_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  // See the comment in the constructor.
  if (!gpu_thread_runner_)
    gpu_thread_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();

  supported_buffer_formats_with_modifiers_ = buffer_formats_with_modifiers;
  supports_viewporter_ = supports_viewporter;
  supports_acquire_fence_ = supports_acquire_fence;
  supports_dmabuf_ = supports_dma_buf;
  supports_overlays_ = supports_overlays;

  supports_non_backed_solid_color_buffers_ =
      supported_surface_augmentor_version >=
      SURFACE_AUGMENTER_CREATE_SOLID_COLOR_BUFFER_SINCE_VERSION;
  supports_subpixel_accurate_position_ =
      supported_surface_augmentor_version >=
      SURFACE_AUGMENTER_GET_AUGMENTED_SUBSURFACE_SINCE_VERSION;
  supports_surface_background_color_ =
      supported_surface_augmentor_version >=
      AUGMENTED_SURFACE_SET_BACKGROUND_COLOR_SINCE_VERSION;
  supports_clip_rect_ = supported_surface_augmentor_version >=
                        AUGMENTED_SUB_SURFACE_SET_CLIP_RECT_SINCE_VERSION;
  supports_affine_transform_ =
      supported_surface_augmentor_version >=
      AUGMENTED_SUB_SURFACE_SET_TRANSFORM_SINCE_VERSION;

  // HitTestMask fix landed in https://crrev.com/c/5252908. This is required to
  // support DnD behavior when the target window has out-of-window frames.
  supports_out_of_window_clip_rect_ =
      server_version.IsValid() &&
      server_version >= base::Version("123.0.6274.0");

  // Exo transformation fix landed in https://crrev.com/c/4961473
  has_transformation_fix_ = server_version.IsValid() &&
                            server_version >= base::Version("121.0.6113.0");

  supports_single_pixel_buffer_ = supports_single_pixel_buffer;

  // Allow to rebind the interface if it hasn't been destroyed yet. Used, for
  // example, by tests which use buffer manager to emulate frames presentation.
  if (remote_host_.is_bound() || associated_receiver_.is_bound()) {
    OnHostDisconnected();
  }
  BindHostInterface(std::move(remote_host));

  ProcessPendingTasks();
}

void WaylandBufferManagerGpu::OnSubmission(
    gfx::AcceleratedWidget widget,
    uint32_t frame_id,
    gfx::SwapResult swap_result,
    gfx::GpuFenceHandle release_fence,
    const std::vector<wl::WaylandPresentationInfo>& presentation_infos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  DCHECK_LE(commit_thread_runners_.count(widget), 1u);
  // Return back to the same thread where the commit request came from.
  auto it = commit_thread_runners_.find(widget);
  if (it == commit_thread_runners_.end())
    return;

  if (it->second->BelongsToCurrentThread()) {
    HandleSubmissionOnOriginThread(widget, frame_id, swap_result,
                                   std::move(release_fence),
                                   presentation_infos);
  } else {
    it->second->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandBufferManagerGpu::HandleSubmissionOnOriginThread,
                       base::Unretained(this), widget, frame_id, swap_result,
                       std::move(release_fence), presentation_infos));
  }
}

void WaylandBufferManagerGpu::OnPresentation(
    gfx::AcceleratedWidget widget,
    const std::vector<wl::WaylandPresentationInfo>& presentation_infos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  DCHECK_LE(commit_thread_runners_.count(widget), 1u);
  // Return back to the same thread where the commit request came from.
  auto it = commit_thread_runners_.find(widget);
  if (it == commit_thread_runners_.end())
    return;

  if (it->second->BelongsToCurrentThread()) {
    HandlePresentationOnOriginThread(widget, presentation_infos);
  } else {
    it->second->PostTask(
        FROM_HERE,
        base::BindOnce(
            &WaylandBufferManagerGpu::HandlePresentationOnOriginThread,
            base::Unretained(this), widget, presentation_infos));
  }
}

void WaylandBufferManagerGpu::RegisterSurface(gfx::AcceleratedWidget widget,
                                              WaylandSurfaceGpu* surface) {
  DCHECK(gpu_thread_runner_);
  gpu_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WaylandBufferManagerGpu::SaveTaskRunnerForWidgetOnIOThread,
          base::Unretained(this), widget,
          base::SingleThreadTaskRunner::GetCurrentDefault()));

  base::AutoLock scoped_lock(lock_);
  widget_to_surface_map_.emplace(widget, surface);
}

void WaylandBufferManagerGpu::UnregisterSurface(gfx::AcceleratedWidget widget) {
  DCHECK(gpu_thread_runner_);
  gpu_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WaylandBufferManagerGpu::ForgetTaskRunnerForWidgetOnIOThread,
          base::Unretained(this), widget));

  base::AutoLock scoped_lock(lock_);
  widget_to_surface_map_.erase(widget);
}

WaylandSurfaceGpu* WaylandBufferManagerGpu::GetSurface(
    gfx::AcceleratedWidget widget) {
  base::AutoLock scoped_lock(lock_);

  auto it = widget_to_surface_map_.find(widget);
  if (it != widget_to_surface_map_.end())
    return it->second;
  return nullptr;
}

void WaylandBufferManagerGpu::CreateDmabufBasedBuffer(
    base::ScopedFD dmabuf_fd,
    gfx::Size size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t current_format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK(gpu_thread_runner_);
  if (!gpu_thread_runner_->BelongsToCurrentThread()) {
    // Do the mojo call on the GpuMainThread.
    gpu_thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandBufferManagerGpu::CreateDmabufBasedBuffer,
                       base::Unretained(this), std::move(dmabuf_fd),
                       std::move(size), std::move(strides), std::move(offsets),
                       std::move(modifiers), current_format, planes_count,
                       buffer_id));
    return;
  }

  base::OnceClosure task = base::BindOnce(
      &WaylandBufferManagerGpu::CreateDmabufBasedBufferTask,
      base::Unretained(this), std::move(dmabuf_fd), size, strides, offsets,
      modifiers, current_format, planes_count, buffer_id);
  RunOrQueueTask(std::move(task));
}

void WaylandBufferManagerGpu::CreateShmBasedBuffer(base::ScopedFD shm_fd,
                                                   size_t length,
                                                   gfx::Size size,
                                                   uint32_t buffer_id) {
  DCHECK(gpu_thread_runner_);
  // Do the mojo call on the GpuMainThread.
  if (!gpu_thread_runner_->BelongsToCurrentThread()) {
    gpu_thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandBufferManagerGpu::CreateShmBasedBuffer,
                       base::Unretained(this), std::move(shm_fd), length,
                       std::move(size), buffer_id));
    return;
  }

  base::OnceClosure task = base::BindOnce(
      &WaylandBufferManagerGpu::CreateShmBasedBufferTask,
      base::Unretained(this), std::move(shm_fd), length, size, buffer_id);
  RunOrQueueTask(std::move(task));
}

void WaylandBufferManagerGpu::CreateSolidColorBuffer(SkColor4f color,
                                                     const gfx::Size& size,
                                                     uint32_t buf_id) {
  DCHECK(gpu_thread_runner_);
  if (!gpu_thread_runner_->BelongsToCurrentThread()) {
    // Do the mojo call on the GpuMainThread.
    gpu_thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandBufferManagerGpu::CreateSolidColorBuffer,
                       base::Unretained(this), color, size, buf_id));
    return;
  }

  base::OnceClosure task =
      base::BindOnce(&WaylandBufferManagerGpu::CreateSolidColorBufferTask,
                     base::Unretained(this), color, size, buf_id);
  RunOrQueueTask(std::move(task));
}

void WaylandBufferManagerGpu::CreateSinglePixelBuffer(SkColor4f color,
                                                      uint32_t buf_id) {
  DCHECK(gpu_thread_runner_);
  if (!gpu_thread_runner_->BelongsToCurrentThread()) {
    // Do the mojo call on the GpuMainThread.
    gpu_thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandBufferManagerGpu::CreateSinglePixelBuffer,
                       base::Unretained(this), color, buf_id));
    return;
  }

  base::OnceClosure task =
      base::BindOnce(&WaylandBufferManagerGpu::CreateSinglePixelBufferTask,
                     base::Unretained(this), color, buf_id);
  RunOrQueueTask(std::move(task));
}

void WaylandBufferManagerGpu::CommitBuffer(gfx::AcceleratedWidget widget,
                                           uint32_t frame_id,
                                           uint32_t buffer_id,
                                           gfx::FrameData data,
                                           const gfx::Rect& bounds_rect,
                                           bool enable_blend,
                                           const gfx::RoundedCornersF& corners,
                                           float surface_scale_factor,
                                           const gfx::Rect& damage_region) {
  // This surface only commits one buffer per frame, use INT32_MIN to attach
  // the buffer to root_surface of wayland window.
  std::vector<wl::WaylandOverlayConfig> overlay_configs;
  overlay_configs.emplace_back(
      gfx::OverlayPlaneData(
          INT32_MIN, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
          gfx::RectF(bounds_rect), gfx::RectF(1.f, 1.f) /* no crop */,
          enable_blend, damage_region, 1.0f /*opacity*/,
          gfx::OverlayPriorityHint::kNone,
          gfx::RRectF(gfx::RectF(bounds_rect), corners), gfx::ColorSpace(),
          std::nullopt),
      nullptr, buffer_id, surface_scale_factor);
  CommitOverlays(widget, frame_id, data, std::move(overlay_configs));
}

void WaylandBufferManagerGpu::CommitOverlays(
    gfx::AcceleratedWidget widget,
    uint32_t frame_id,
    gfx::FrameData data,
    std::vector<wl::WaylandOverlayConfig> overlays) {
  DCHECK(gpu_thread_runner_);
  if (!gpu_thread_runner_->BelongsToCurrentThread()) {
    // Do the mojo call on the GpuMainThread.
    gpu_thread_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WaylandBufferManagerGpu::CommitOverlays,
                                  base::Unretained(this), widget, frame_id,
                                  data, std::move(overlays)));
    return;
  }

  base::OnceClosure task = base::BindOnce(
      &WaylandBufferManagerGpu::CommitOverlaysTask, base::Unretained(this),
      widget, frame_id, data, std::move(overlays));
  RunOrQueueTask(std::move(task));
}

void WaylandBufferManagerGpu::DestroyBuffer(uint32_t buffer_id) {
  DCHECK(gpu_thread_runner_);
  if (!gpu_thread_runner_->BelongsToCurrentThread()) {
    // Do the mojo call on the GpuMainThread.
    gpu_thread_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WaylandBufferManagerGpu::DestroyBuffer,
                                  base::Unretained(this), buffer_id));
    return;
  }

  base::OnceClosure task =
      base::BindOnce(&WaylandBufferManagerGpu::DestroyBufferTask,
                     base::Unretained(this), buffer_id);
  RunOrQueueTask(std::move(task));
}

#if defined(WAYLAND_GBM)
GbmDevice* WaylandBufferManagerGpu::GetGbmDevice() {
  // Wayland won't support wl_drm or zwp_linux_dmabuf without this extension.
  if (!supports_dmabuf_ || (!gl::GLSurfaceEGL::GetGLDisplayEGL()
                                 ->ext->b_EGL_EXT_image_dma_buf_import &&
                            !use_fake_gbm_device_for_test_)) {
    supports_dmabuf_ = false;
    return nullptr;
  }

  if (gbm_device_ || use_fake_gbm_device_for_test_)
    return gbm_device_.get();

  if (!drm_render_node_fd_.is_valid()) {
    supports_dmabuf_ = false;
    return nullptr;
  }

  gbm_device_ = CreateGbmDevice(drm_render_node_fd_.get());
  if (!gbm_device_) {
    supports_dmabuf_ = false;
    LOG(WARNING) << "Failed to initialize gbm device.";
    return nullptr;
  }
  return gbm_device_.get();
}
#endif  // defined(WAYLAND_GBM)

void WaylandBufferManagerGpu::AddBindingWaylandBufferManagerGpu(
    mojo::PendingReceiver<ozone::mojom::WaylandBufferManagerGpu> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

const std::vector<uint64_t>
WaylandBufferManagerGpu::GetModifiersForBufferFormat(
    gfx::BufferFormat buffer_format) const {
  auto it = supported_buffer_formats_with_modifiers_.find(buffer_format);
  if (it != supported_buffer_formats_with_modifiers_.end()) {
    if (drm_modifiers_filter_) {
      return drm_modifiers_filter_->Filter(buffer_format, it->second);
    }
    return it->second;
  }
  return {};
}

uint32_t WaylandBufferManagerGpu::AllocateBufferID() {
  return ++next_buffer_id_ ? next_buffer_id_ : ++next_buffer_id_;
}

bool WaylandBufferManagerGpu::SupportsFormat(
    gfx::BufferFormat buffer_format) const {
  return supported_buffer_formats_with_modifiers_.contains(buffer_format);
}

void WaylandBufferManagerGpu::BindHostInterface(
    mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  DCHECK(!remote_host_.is_bound() && !associated_receiver_.is_bound());
  remote_host_.Bind(std::move(remote_host));
  // WaylandBufferManagerHost may bind host again after an error. See
  // WaylandBufferManagerHost::BindInterface for more details.
  remote_host_.set_disconnect_handler(base::BindOnce(
      &WaylandBufferManagerGpu::OnHostDisconnected, base::Unretained(this)));

  // Setup associated interface.
  mojo::PendingAssociatedRemote<ozone::mojom::WaylandBufferManagerGpu>
      client_remote;
  associated_receiver_.Bind(client_remote.InitWithNewEndpointAndPassReceiver());
  DCHECK(remote_host_);
  remote_host_->SetWaylandBufferManagerGpu(std::move(client_remote));
}

void WaylandBufferManagerGpu::SaveTaskRunnerForWidgetOnIOThread(
    gfx::AcceleratedWidget widget,
    scoped_refptr<base::SingleThreadTaskRunner> origin_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  commit_thread_runners_.emplace(widget, origin_runner);
}

void WaylandBufferManagerGpu::ForgetTaskRunnerForWidgetOnIOThread(
    gfx::AcceleratedWidget widget) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  commit_thread_runners_.erase(widget);
}

void WaylandBufferManagerGpu::HandleSubmissionOnOriginThread(
    gfx::AcceleratedWidget widget,
    uint32_t frame_id,
    gfx::SwapResult swap_result,
    gfx::GpuFenceHandle release_fence,
    const std::vector<wl::WaylandPresentationInfo>& presentation_infos) {
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  auto* surface = GetSurface(widget);
  // The surface might be destroyed by the time the swap result is provided.
  if (surface)
    surface->OnSubmission(frame_id, swap_result, std::move(release_fence));

  HandlePresentationOnOriginThread(widget, presentation_infos);
}

void WaylandBufferManagerGpu::HandlePresentationOnOriginThread(
    gfx::AcceleratedWidget widget,
    const std::vector<wl::WaylandPresentationInfo>& presentation_infos) {
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);

  auto* surface = GetSurface(widget);
  // The surface might be destroyed by the time the presentation feedback is
  // provided.
  if (!surface) {
    return;
  }

  for (const auto& presentation_info : presentation_infos) {
    surface->OnPresentation(presentation_info.frame_id,
                            presentation_info.feedback);
  }
}

#if defined(WAYLAND_GBM)
void WaylandBufferManagerGpu::OpenAndStoreDrmRenderNodeFd(
    const base::FilePath& drm_node_path) {
  DrmRenderNodeHandle handle;
  if (drm_node_path.empty() || !handle.Initialize(drm_node_path)) {
    LOG(WARNING) << "Failed to initialize drm render node handle.";
    return;
  }

  drm_render_node_fd_ = handle.PassFD();
}
#endif

void WaylandBufferManagerGpu::OnHostDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  // WaylandBufferManagerHost may bind host again after an error. See
  // WaylandBufferManagerHost::BindInterface for more details.
  remote_host_.reset();
  // When the remote host is disconnected, it also disconnects the associated
  // receiver. Thus, reset that as well.
  associated_receiver_.reset();
}

void WaylandBufferManagerGpu::RunOrQueueTask(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  if (remote_host_) {
    std::move(task).Run();
    return;
  }
  pending_tasks_.emplace_back(std::move(task));
}

void WaylandBufferManagerGpu::ProcessPendingTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  DCHECK(remote_host_);

  for (auto& task : pending_tasks_)
    std::move(task).Run();

  pending_tasks_.clear();
}

void WaylandBufferManagerGpu::CreateDmabufBasedBufferTask(
    base::ScopedFD dmabuf_fd,
    gfx::Size size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t current_format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  DCHECK(remote_host_);

  remote_host_->CreateDmabufBasedBuffer(
      mojo::PlatformHandle(std::move(dmabuf_fd)), size, strides, offsets,
      modifiers, current_format, planes_count, buffer_id);
}

void WaylandBufferManagerGpu::CreateShmBasedBufferTask(base::ScopedFD shm_fd,
                                                       size_t length,
                                                       gfx::Size size,
                                                       uint32_t buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  DCHECK(remote_host_);

  remote_host_->CreateShmBasedBuffer(mojo::PlatformHandle(std::move(shm_fd)),
                                     length, size, buffer_id);
}

void WaylandBufferManagerGpu::CreateSolidColorBufferTask(SkColor4f color,
                                                         const gfx::Size& size,
                                                         uint32_t buf_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  DCHECK(remote_host_);

  remote_host_->CreateSolidColorBuffer(size, color, buf_id);
}

void WaylandBufferManagerGpu::CreateSinglePixelBufferTask(SkColor4f color,
                                                          uint32_t buf_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  DCHECK(remote_host_);

  remote_host_->CreateSinglePixelBuffer(color, buf_id);
}

void WaylandBufferManagerGpu::CommitOverlaysTask(
    gfx::AcceleratedWidget widget,
    uint32_t frame_id,
    gfx::FrameData data,
    std::vector<wl::WaylandOverlayConfig> overlays) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  DCHECK(remote_host_);

  remote_host_->CommitOverlays(widget, frame_id, data, std::move(overlays));
}

void WaylandBufferManagerGpu::DestroyBufferTask(uint32_t buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  DCHECK(remote_host_);

  remote_host_->DestroyBuffer(buffer_id);
}

}  // namespace ui
