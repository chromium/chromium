// Copyright 2016 The Chromium Authors. All rights reserved.
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

void* QueryDeviceObjectFromANGLE(int object_type) {
  EGLDisplay egl_display = nullptr;
  intptr_t egl_device = 0;
  intptr_t device = 0;

  {
    TRACE_EVENT0("gpu", "QueryDeviceObjectFromANGLE. GetHardwareDisplay");
    egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  }

  if (!gl::GLSurfaceEGL::HasEGLClientExtension("EGL_EXT_device_query"))
    return nullptr;

  PFNEGLQUERYDISPLAYATTRIBEXTPROC QueryDisplayAttribEXT = nullptr;

  {
    TRACE_EVENT0("gpu", "QueryDeviceObjectFromANGLE. eglGetProcAddress");

    QueryDisplayAttribEXT = reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(
        eglGetProcAddress("eglQueryDisplayAttribEXT"));

    if (!QueryDisplayAttribEXT)
      return nullptr;
  }

  PFNEGLQUERYDEVICEATTRIBEXTPROC QueryDeviceAttribEXT = nullptr;

  {
    TRACE_EVENT0("gpu", "QueryDeviceObjectFromANGLE. eglGetProcAddress");

    QueryDeviceAttribEXT = reinterpret_cast<PFNEGLQUERYDEVICEATTRIBEXTPROC>(
        eglGetProcAddress("eglQueryDeviceAttribEXT"));
    if (!QueryDeviceAttribEXT)
      return nullptr;
  }

  {
    TRACE_EVENT0("gpu", "QueryDeviceObjectFromANGLE. QueryDisplayAttribEXT");

    if (!QueryDisplayAttribEXT(egl_display, EGL_DEVICE_EXT, &egl_device))
      return nullptr;
  }
  if (!egl_device)
    return nullptr;

  {
    TRACE_EVENT0("gpu", "QueryDeviceObjectFromANGLE. QueryDisplayAttribEXT");

    if (!QueryDeviceAttribEXT(reinterpret_cast<EGLDeviceEXT>(egl_device),
                              object_type, &device)) {
      return nullptr;
    }
  }

  return reinterpret_cast<void*>(device);
}

Microsoft::WRL::ComPtr<ID3D11Device> QueryD3D11DeviceObjectFromANGLE() {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
  d3d11_device = reinterpret_cast<ID3D11Device*>(
      QueryDeviceObjectFromANGLE(EGL_D3D11_DEVICE_ANGLE));
  return d3d11_device;
}

Microsoft::WRL::ComPtr<IDirect3DDevice9> QueryD3D9DeviceObjectFromANGLE() {
  Microsoft::WRL::ComPtr<IDirect3DDevice9> d3d9_device;
  d3d9_device = reinterpret_cast<IDirect3DDevice9*>(
      QueryDeviceObjectFromANGLE(EGL_D3D9_DEVICE_ANGLE));
  return d3d9_device;
}

Microsoft::WRL::ComPtr<IDCompositionDevice2> QueryDirectCompositionDevice(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device) {
  // Each D3D11 device will have a DirectComposition device stored in its
  // private data under this GUID.
  // {CF81D85A-8D30-4769-8509-B9D73898D870}
  static const GUID kDirectCompositionGUID = {
      0xcf81d85a,
      0x8d30,
      0x4769,
      {0x85, 0x9, 0xb9, 0xd7, 0x38, 0x98, 0xd8, 0x70}};

  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device;
  if (!d3d11_device)
    return dcomp_device;

  UINT data_size = sizeof(dcomp_device.Get());
  HRESULT hr =
      d3d11_device->GetPrivateData(kDirectCompositionGUID, &data_size,
                                   dcomp_device.ReleaseAndGetAddressOf());
  if (SUCCEEDED(hr) && dcomp_device)
    return dcomp_device;

  // Allocate a new DirectComposition device if none currently exists.
  HMODULE dcomp_module = ::GetModuleHandle(L"dcomp.dll");
  if (!dcomp_module)
    return dcomp_device;

  using PFN_DCOMPOSITION_CREATE_DEVICE2 = HRESULT(WINAPI*)(
      IUnknown * renderingDevice, REFIID iid, void** dcompositionDevice);
  PFN_DCOMPOSITION_CREATE_DEVICE2 create_device_function =
      reinterpret_cast<PFN_DCOMPOSITION_CREATE_DEVICE2>(
          ::GetProcAddress(dcomp_module, "DCompositionCreateDevice2"));
  if (!create_device_function)
    return dcomp_device;

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device.As(&dxgi_device);
  Microsoft::WRL::ComPtr<IDCompositionDesktopDevice> desktop_device;
  hr = create_device_function(dxgi_device.Get(), IID_PPV_ARGS(&desktop_device));
  if (FAILED(hr))
    return dcomp_device;

  hr = desktop_device.As(&dcomp_device);
  CHECK(SUCCEEDED(hr));
  d3d11_device->SetPrivateDataInterface(kDirectCompositionGUID,
                                        dcomp_device.Get());

  return dcomp_device;
}

}  // namespace gl
