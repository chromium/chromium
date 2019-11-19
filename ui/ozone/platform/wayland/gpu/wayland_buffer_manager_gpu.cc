// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"

#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop_current.h"
#include "base/process/process.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_gpu.h"

namespace ui {

WaylandBufferManagerGpu::WaylandBufferManagerGpu() = default;
WaylandBufferManagerGpu::~WaylandBufferManagerGpu() = default;

void WaylandBufferManagerGpu::Initialize(
    mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host,
    const base::flat_map<::gfx::BufferFormat, std::vector<uint64_t>>&
        buffer_formats_with_modifiers,
    bool supports_dma_buf) {
  DCHECK(supported_buffer_formats_with_modifiers_.empty());
  supported_buffer_formats_with_modifiers_ = buffer_formats_with_modifiers;

#if defined(WAYLAND_GBM)
  if (!supports_dma_buf)
    set_gbm_device(nullptr);
#endif

  BindHostInterface(std::move(remote_host));

  io_thread_runner_ = base::ThreadTaskRunnerHandle::Get();
}

void WaylandBufferManagerGpu::OnSubmission(gfx::AcceleratedWidget widget,
                                           uint32_t buffer_id,
                                           gfx::SwapResult swap_result) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  // Return back to the same thread where the commit request came from.
  commit_thread_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WaylandBufferManagerGpu::SubmitSwapResultOnOriginThread,
                 base::Unretained(this), widget, buffer_id, swap_result));
}

void WaylandBufferManagerGpu::OnPresentation(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  // Return back to the same thread where the commit request came from.
  commit_thread_runner_->PostTask(
      FROM_HERE,
      base::Bind(&WaylandBufferManagerGpu::SubmitPresentationtOnOriginThread,
                 base::Unretained(this), widget, buffer_id, feedback));
}

void WaylandBufferManagerGpu::RegisterSurface(gfx::AcceleratedWidget widget,
                                              WaylandSurfaceGpu* surface) {
  base::AutoLock scoped_lock(lock_);
  widget_to_surface_map_.emplace(widget, surface);
}

void WaylandBufferManagerGpu::UnregisterSurface(gfx::AcceleratedWidget widget) {
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
    gfx::AcceleratedWidget widget,
    base::ScopedFD dmabuf_fd,
    gfx::Size size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t current_format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK(io_thread_runner_);

  // Do the mojo call on the IO child thread.
  io_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerGpu::CreateDmabufBasedBufferInternal,
                     base::Unretained(this), widget, std::move(dmabuf_fd),
                     std::move(size), std::move(strides), std::move(offsets),
                     std::move(modifiers), current_format, planes_count,
                     buffer_id));
}

void WaylandBufferManagerGpu::CreateShmBasedBuffer(
    gfx::AcceleratedWidget widget,
    base::ScopedFD shm_fd,
    size_t length,
    gfx::Size size,
    uint32_t buffer_id) {
  DCHECK(io_thread_runner_);

  // Do the mojo call on the IO child thread.
  io_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerGpu::CreateShmBasedBufferInternal,
                     base::Unretained(this), widget, std::move(shm_fd), length,
                     std::move(size), buffer_id));
}

void WaylandBufferManagerGpu::CommitBuffer(gfx::AcceleratedWidget widget,
                                           uint32_t buffer_id,
                                           const gfx::Rect& damage_region) {
  DCHECK(io_thread_runner_);

  if (!commit_thread_runner_)
    commit_thread_runner_ = base::ThreadTaskRunnerHandle::Get();

  // Do the mojo call on the IO child thread.
  io_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerGpu::CommitBufferInternal,
                     base::Unretained(this), widget, buffer_id, damage_region));
}

void WaylandBufferManagerGpu::DestroyBuffer(gfx::AcceleratedWidget widget,
                                            uint32_t buffer_id) {
  DCHECK(io_thread_runner_);

  // Do the mojo call on the IO child thread.
  io_thread_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WaylandBufferManagerGpu::DestroyBufferInternal,
                                base::Unretained(this), widget, buffer_id));
}

void WaylandBufferManagerGpu::AddBindingWaylandBufferManagerGpu(
    mojo::PendingReceiver<ozone::mojom::WaylandBufferManagerGpu> receiver) {
  receiver_.Bind(std::move(receiver));
}

const std::vector<uint64_t>&
WaylandBufferManagerGpu::GetModifiersForBufferFormat(
    gfx::BufferFormat buffer_format) const {
  auto it = supported_buffer_formats_with_modifiers_.find(buffer_format);
  if (it != supported_buffer_formats_with_modifiers_.end()) {
    return it->second;
  }
  static std::vector<uint64_t> dummy;
  return dummy;
}

void WaylandBufferManagerGpu::CreateDmabufBasedBufferInternal(
    gfx::AcceleratedWidget widget,
    base::ScopedFD dmabuf_fd,
    gfx::Size size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t current_format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  DCHECK(remote_host_);
  remote_host_->CreateDmabufBasedBuffer(
      widget,
      mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(dmabuf_fd))),
      size, strides, offsets, modifiers, current_format, planes_count,
      buffer_id);
}

void WaylandBufferManagerGpu::CreateShmBasedBufferInternal(
    gfx::AcceleratedWidget widget,
    base::ScopedFD shm_fd,
    size_t length,
    gfx::Size size,
    uint32_t buffer_id) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  DCHECK(remote_host_);
  remote_host_->CreateShmBasedBuffer(
      widget, mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(shm_fd))),
      length, size, buffer_id);
}

void WaylandBufferManagerGpu::CommitBufferInternal(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::Rect& damage_region) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  DCHECK(remote_host_);

  remote_host_->CommitBuffer(widget, buffer_id, damage_region);
}

void WaylandBufferManagerGpu::DestroyBufferInternal(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  DCHECK(remote_host_);

  remote_host_->DestroyBuffer(widget, buffer_id);
}

void WaylandBufferManagerGpu::BindHostInterface(
    mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host) {
  remote_host_.Bind(std::move(remote_host));

  // Setup associated interface.
  mojo::PendingAssociatedRemote<ozone::mojom::WaylandBufferManagerGpu>
      client_remote;
  associated_receiver_.Bind(client_remote.InitWithNewEndpointAndPassReceiver());
  DCHECK(remote_host_);
  remote_host_->SetWaylandBufferManagerGpu(std::move(client_remote));
}

void WaylandBufferManagerGpu::SubmitSwapResultOnOriginThread(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    gfx::SwapResult swap_result) {
  DCHECK(commit_thread_runner_->BelongsToCurrentThread());
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  auto* surface = GetSurface(widget);
  // The surface might be destroyed by the time the swap result is provided.
  if (surface)
    surface->OnSubmission(buffer_id, swap_result);
}

void WaylandBufferManagerGpu::SubmitPresentationtOnOriginThread(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(commit_thread_runner_->BelongsToCurrentThread());
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  auto* surface = GetSurface(widget);
  // The surface might be destroyed by the time the presentation feedback is
  // provided.
  if (surface)
    surface->OnPresentation(buffer_id, feedback);
}

}  // namespace ui
