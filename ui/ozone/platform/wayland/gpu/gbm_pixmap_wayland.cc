// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/gbm_pixmap_wayland.h"

#include <drm_fourcc.h>
#include <gbm.h>
#include <xf86drmMode.h>

#include <memory>

#include "base/files/platform_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_device.h"
#include "ui/gfx/linux/gbm_util.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/public/overlay_plane.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

GbmPixmapWayland::GbmPixmapWayland(WaylandBufferManagerGpu* buffer_manager)
    : buffer_manager_(buffer_manager),
      buffer_id_(buffer_manager->AllocateBufferID()) {}

GbmPixmapWayland::~GbmPixmapWayland() {
  if (gbm_bo_)
    buffer_manager_->DestroyBuffer(widget_, buffer_id_);
}

bool GbmPixmapWayland::InitializeBuffer(gfx::Size size,
                                        gfx::BufferFormat format,
                                        gfx::BufferUsage usage) {
  TRACE_EVENT0("wayland", "GbmPixmapWayland::InitializeBuffer");

  if (!buffer_manager_->gbm_device())
    return false;

  const uint32_t fourcc_format = GetFourCCFormatFromBufferFormat(format);
  auto gbm_usage = ui::BufferUsageToGbmFlags(usage);
  std::vector<uint64_t> modifiers;
  if (!(gbm_usage & GBM_BO_USE_LINEAR))
    modifiers = buffer_manager_->GetModifiersForBufferFormat(format);

  gbm_bo_ = buffer_manager_->gbm_device()->CreateBufferWithModifiers(
      fourcc_format, size, gbm_usage, modifiers);
  if (!gbm_bo_) {
    LOG(ERROR) << "Cannot create bo with format= "
               << gfx::BufferFormatToString(format) << " and usage "
               << gfx::BufferUsageToString(usage);
    return false;
  }

  CreateDmabufBasedBuffer();
  return true;
}

void GbmPixmapWayland::SetAcceleratedWiget(gfx::AcceleratedWidget widget) {
  DCHECK(widget != gfx::kNullAcceleratedWidget);
  DCHECK(widget_ == gfx::kNullAcceleratedWidget);
  widget_ = widget;
}

bool GbmPixmapWayland::AreDmaBufFdsValid() const {
  return gbm_bo_->AreFdsValid();
}

int GbmPixmapWayland::GetDmaBufFd(size_t plane) const {
  return gbm_bo_->GetPlaneFd(plane);
}

uint32_t GbmPixmapWayland::GetDmaBufPitch(size_t plane) const {
  return gbm_bo_->GetPlaneStride(plane);
}

size_t GbmPixmapWayland::GetDmaBufOffset(size_t plane) const {
  return gbm_bo_->GetPlaneOffset(plane);
}

size_t GbmPixmapWayland::GetDmaBufPlaneSize(size_t plane) const {
  return gbm_bo_->GetPlaneSize(plane);
}

size_t GbmPixmapWayland::GetNumberOfPlanes() const {
  return gbm_bo_->GetNumPlanes();
}

uint64_t GbmPixmapWayland::GetBufferFormatModifier() const {
  return gbm_bo_->GetFormatModifier();
}

gfx::BufferFormat GbmPixmapWayland::GetBufferFormat() const {
  return gbm_bo_->GetBufferFormat();
}

gfx::Size GbmPixmapWayland::GetBufferSize() const {
  return gbm_bo_->GetSize();
}

uint32_t GbmPixmapWayland::GetUniqueId() const {
  return gbm_bo_->GetHandle();
}

bool GbmPixmapWayland::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    const gfx::Rect& display_bounds,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::vector<gfx::GpuFence> acquire_fences,
    std::vector<gfx::GpuFence> release_fences) {
  // If the widget this pixmap backs has not been assigned before, do it now.
  if (widget_ == gfx::kNullAcceleratedWidget)
    SetAcceleratedWiget(widget);

  DCHECK_EQ(widget_, widget);

  auto* surface = buffer_manager_->GetSurface(widget);
  // This must never be hit.
  DCHECK(surface);
  GbmSurfacelessWayland* surfaceless =
      static_cast<GbmSurfacelessWayland*>(surface);
  DCHECK(surfaceless);

  DCHECK(acquire_fences.empty() || acquire_fences.size() == 1u);
  surfaceless->QueueOverlayPlane(
      OverlayPlane(this,
                   acquire_fences.empty() ? nullptr
                                          : std::make_unique<gfx::GpuFence>(
                                                std::move(acquire_fences[0])),
                   plane_z_order, plane_transform, display_bounds, crop_rect,
                   enable_blend),
      buffer_id_);
  return true;
}

gfx::NativePixmapHandle GbmPixmapWayland::ExportHandle() {
  gfx::NativePixmapHandle handle;
  gfx::BufferFormat format = GetBufferFormat();

  // TODO(dcastagna): Use gbm_bo_get_plane_count once all the formats we use are
  // supported by gbm.
  const size_t num_planes = gfx::NumberOfPlanesForLinearBufferFormat(format);
  std::vector<base::ScopedFD> scoped_fds(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    scoped_fds[i] = base::ScopedFD(HANDLE_EINTR(dup(GetDmaBufFd(i))));
    if (!scoped_fds[i].is_valid()) {
      PLOG(ERROR) << "dup";
      return gfx::NativePixmapHandle();
    }
  }

  for (size_t i = 0; i < num_planes; ++i) {
    handle.planes.emplace_back(GetDmaBufPitch(i), GetDmaBufOffset(i),
                               gbm_bo_->GetPlaneSize(i),
                               std::move(scoped_fds[i]));
  }
  handle.modifier = GetBufferFormatModifier();
  return handle;
}

void GbmPixmapWayland::CreateDmabufBasedBuffer() {
  uint64_t modifier = gbm_bo_->GetFormatModifier();

  std::vector<uint32_t> strides;
  std::vector<uint32_t> offsets;
  std::vector<uint64_t> modifiers;

  size_t plane_count = gbm_bo_->GetNumPlanes();
  for (size_t i = 0; i < plane_count; ++i) {
    strides.push_back(GetDmaBufPitch(i));
    offsets.push_back(GetDmaBufOffset(i));
    modifiers.push_back(modifier);
  }

  base::ScopedFD fd(HANDLE_EINTR(dup(GetDmaBufFd(0))));
  if (!fd.is_valid()) {
    PLOG(FATAL) << "dup";
    return;
  }
  // Asks Wayland to create a wl_buffer based on the |file| fd.
  buffer_manager_->CreateDmabufBasedBuffer(
      std::move(fd), GetBufferSize(), strides, offsets, modifiers,
      gbm_bo_->GetFormat(), plane_count, buffer_id_);
}

}  // namespace ui
