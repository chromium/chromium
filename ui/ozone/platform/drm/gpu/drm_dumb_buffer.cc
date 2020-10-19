// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_dumb_buffer.h"

#include <drm_fourcc.h>
#include <xf86drmMode.h>

#include "base/logging.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"

namespace ui {

namespace {

bool DestroyDumbBuffer(const scoped_refptr<DrmDevice>& drm_device,
                       uint32_t handle,
                       DrmDumbBuffer::HandleCloser handle_closer) {
  switch (handle_closer) {
    case DrmDumbBuffer::HandleCloser::DESTROY_DUMB:
      return drm_device->DestroyDumbBuffer(handle);
    case DrmDumbBuffer::HandleCloser::GEM_CLOSE:
      return drm_device->CloseBufferHandle(handle);
  }
}

}  // namespace

DrmDumbBuffer::DrmDumbBuffer(const scoped_refptr<DrmDevice>& drm) : drm_(drm) {}

DrmDumbBuffer::~DrmDumbBuffer() {
  if (mmap_base_ && !drm_->UnmapDumbBuffer(mmap_base_, mmap_size_))
    PLOG(ERROR) << "DrmDumbBuffer: UnmapDumbBuffer: handle " << handle_;

  if (handle_ && !DestroyDumbBuffer(drm_, handle_, handle_closer_))
    PLOG(ERROR) << "DrmDumbBuffer: DestroyDumbBuffer: handle " << handle_;
}

bool DrmDumbBuffer::Initialize(const SkImageInfo& info) {
  DCHECK(!handle_);

  if (!drm_->CreateDumbBuffer(info, &handle_, &stride_)) {
    PLOG(ERROR) << "DrmDumbBuffer: CreateDumbBuffer: width " << info.width()
                << " height " << info.height();
    return false;
  }

  handle_closer_ = HandleCloser::DESTROY_DUMB;

  return MapDumbBuffer(info);
}

bool DrmDumbBuffer::InitializeFromFramebuffer(uint32_t framebuffer_id) {
  DCHECK(!handle_);

  ScopedDrmFramebufferPtr framebuffer(drm_->GetFramebuffer(framebuffer_id));
  if (!framebuffer)
    return false;

  handle_ = framebuffer->handle;
  stride_ = framebuffer->pitch;
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(framebuffer->width, framebuffer->height);

  handle_closer_ = HandleCloser::GEM_CLOSE;

  return MapDumbBuffer(info);
}

SkCanvas* DrmDumbBuffer::GetCanvas() const {
  return surface_->getCanvas();
}

uint32_t DrmDumbBuffer::GetHandle() const {
  return handle_;
}

gfx::Size DrmDumbBuffer::GetSize() const {
  return gfx::Size(surface_->width(), surface_->height());
}

bool DrmDumbBuffer::MapDumbBuffer(const SkImageInfo& info) {
  mmap_size_ = info.computeByteSize(stride_);
  if (!drm_->MapDumbBuffer(handle_, mmap_size_, &mmap_base_)) {
    PLOG(ERROR) << "DrmDumbBuffer: MapDumbBuffer: handle " << handle_;
    return false;
  }

  surface_ = SkSurface::MakeRasterDirect(info, mmap_base_, stride_);
  if (!surface_) {
    LOG(ERROR) << "DrmDumbBuffer: Failed to create SkSurface: handle "
               << handle_;
    return false;
  }

  return true;
}

}  // namespace ui
