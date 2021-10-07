// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_dcomp_surface.h"

#include "base/logging.h"
#include "ui/gl/gl_bindings.h"

namespace gl {

using Microsoft::WRL::ComPtr;

GLImageDCOMPSurface::GLImageDCOMPSurface(const gfx::Size& size,
                                         HANDLE surface_handle)
    : size_(size), surface_handle_(surface_handle) {}

// static
GLImageDCOMPSurface* GLImageDCOMPSurface::FromGLImage(GLImage* image) {
  if (!image || image->GetType() != Type::DCOMP_SURFACE)
    return nullptr;
  return static_cast<GLImageDCOMPSurface*>(image);
}

gfx::Size GLImageDCOMPSurface::GetSize() {
  return size_;
}

unsigned GLImageDCOMPSurface::GetDataType() {
  return GL_UNSIGNED_BYTE;
}

unsigned GLImageDCOMPSurface::GetInternalFormat() {
  return GL_BGRA_EXT;
}

GLImageDCOMPSurface::BindOrCopy GLImageDCOMPSurface::ShouldBindOrCopy() {
  return BIND;
}

bool GLImageDCOMPSurface::BindTexImage(unsigned target) {
  // This should not be called via compositor. It is possible other code paths
  // such as canvas2d drawImage with video might use `BindTexImage` to import
  // the video into GL.
  return false;
}

void GLImageDCOMPSurface::ReleaseTexImage(unsigned target) {}

bool GLImageDCOMPSurface::CopyTexImage(unsigned target) {
  return false;
}

bool GLImageDCOMPSurface::CopyTexSubImage(unsigned target,
                                          const gfx::Point& offset,
                                          const gfx::Rect& rect) {
  return false;
}

void GLImageDCOMPSurface::SetSize(gfx::Size size) {
  size_ = size;
}

void GLImageDCOMPSurface::SetColorSpace(const gfx::ColorSpace& color_space) {
  color_space_ = color_space;
}

void GLImageDCOMPSurface::Flush() {}

void GLImageDCOMPSurface::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t process_tracing_id,
    const std::string& dump_name) {}

GLImage::Type GLImageDCOMPSurface::GetType() const {
  return Type::DCOMP_SURFACE;
}

GLImageDCOMPSurface::~GLImageDCOMPSurface() {}

ComPtr<IDCompositionSurface> GLImageDCOMPSurface::CreateSurfaceForDevice(
    IDCompositionDevice2* device) {
  DVLOG(1) << __func__ << " this=" << this;

  ComPtr<IDCompositionSurface> dcomp_surface;
  ComPtr<IDCompositionDevice> dcomp_device_1;
  HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&dcomp_device_1));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get DCOMP device. hr=" << hr;
    return nullptr;
  }

  hr = dcomp_device_1->CreateSurfaceFromHandle(surface_handle_.Get(),
                                               &dcomp_surface);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create DCOMP surface. hr=" << hr;
    return nullptr;
  }

  return dcomp_surface;
}

void GLImageDCOMPSurface::SetParentWindow(HWND parent) {
  last_parent_ = reinterpret_cast<HWND>(parent);
}

bool GLImageDCOMPSurface::IsContextValid() const {
  return true;
}

}  // namespace gl
