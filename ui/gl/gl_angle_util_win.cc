// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_angle_util_win.h"

#include <objbase.h>

#include "base/trace_event/trace_event.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {
namespace {
void* QueryDeviceObjectFromANGLE(EGLDisplay egl_display, int object_type) {
  TRACE_EVENT0("gpu", "QueryDeviceObjectFromANGLE");
  if (egl_display == EGL_NO_DISPLAY) {
    DVLOG(1) << "Failed to retrieve EGLDisplay";
    return nullptr;
  }

  if (!g_driver_egl.client_ext.b_EGL_EXT_device_query) {
    DVLOG(1) << "EGL_EXT_device_query not supported";
    return nullptr;
  }

  intptr_t egl_device = 0;
  if (!eglQueryDisplayAttribEXT(egl_display, EGL_DEVICE_EXT, &egl_device)) {
    DVLOG(1) << "eglQueryDisplayAttribEXT failed";
    return nullptr;
  }

  if (!egl_device) {
    DVLOG(1) << "Failed to retrieve EGLDeviceEXT";
    return nullptr;
  }

  intptr_t device = 0;
  if (!eglQueryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(egl_device),
                               object_type, &device)) {
    DVLOG(1) << "eglQueryDeviceAttribEXT failed";
    return nullptr;
  }

  return reinterpret_cast<void*>(device);
}
}  // namespace

Microsoft::WRL::ComPtr<ID3D11Device> QueryD3D11DeviceObjectFromANGLE() {
  auto* display = GLSurfaceEGL::GetGLDisplayEGL();
  if (!display || (display->GetDisplayType() != ANGLE_D3D11 &&
                   display->GetDisplayType() != ANGLE_D3D11_NULL &&
                   display->GetDisplayType() != ANGLE_D3D11on12)) {
    return nullptr;
  }
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
  d3d11_device = reinterpret_cast<ID3D11Device*>(QueryDeviceObjectFromANGLE(
      display->GetDisplay(), EGL_D3D11_DEVICE_ANGLE));
  return d3d11_device;
}

Microsoft::WRL::ComPtr<IDirect3DDevice9> QueryD3D9DeviceObjectFromANGLE() {
  auto* display = GLSurfaceEGL::GetGLDisplayEGL();
  if (!display || display->GetDisplayType() != ANGLE_D3D9) {
    return nullptr;
  }
  Microsoft::WRL::ComPtr<IDirect3DDevice9> d3d9_device;
  d3d9_device = reinterpret_cast<IDirect3DDevice9*>(
      QueryDeviceObjectFromANGLE(display->GetDisplay(), EGL_D3D9_DEVICE_ANGLE));
  return d3d9_device;
}

}  // namespace gl
