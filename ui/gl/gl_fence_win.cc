// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_fence_win.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "ui/gl/gl_angle_util_win.h"

#include <d3d11_4.h>

namespace gl {

bool GLFenceWin::IsSupported() {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device)
    return false;

  // Support for ID3D11Device5 implies support for ID3D11DeviceContext4.
  // No point in letting you create fences if you can't signal or wait on them.
  Microsoft::WRL::ComPtr<ID3D11Device5> d3d11_device5;
  HRESULT hr = d3d11_device.As(&d3d11_device5);
  if (FAILED(hr)) {
    return false;
  }

  return true;
}

std::unique_ptr<GLFenceWin> GLFenceWin::CreateForGpuFence() {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device) {
    DLOG(ERROR) << "Unable to retrieve ID3D11Device from ANGLE";
    return nullptr;
  }

  return CreateForGpuFence(d3d11_device.Get());
}

std::unique_ptr<GLFenceWin> GLFenceWin::CreateForGpuFence(
    ID3D11Device* d3d11_device) {
  Microsoft::WRL::ComPtr<ID3D11Device5> d3d11_device5;
  HRESULT hr = d3d11_device->QueryInterface(IID_PPV_ARGS(&d3d11_device5));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to retrieve ID3D11Device5 interface "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence;
  hr = d3d11_device5->CreateFence(0, D3D11_FENCE_FLAG_SHARED,
                                  IID_PPV_ARGS(&d3d11_fence));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to create ID3D11Fence "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  HANDLE shared_handle;
  hr = d3d11_fence->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &shared_handle);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to create shared handle for DXGIResource "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  // Put the shared handle into an RAII object as quickly as possible to
  // ensure we do not leak it.
  base::win::ScopedHandle scoped_handle(shared_handle);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
  d3d11_device5->GetImmediateContext(&d3d11_device_context);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext4> d3d11_device_context4;
  hr = d3d11_device_context.As(&d3d11_device_context4);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to retrieve ID3D11DeviceContext4 interface "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  hr = d3d11_device_context4->Signal(d3d11_fence.Get(), 1);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to Signal D3D11 fence "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  gfx::GpuFenceHandle gpu_fence_handle;
  gpu_fence_handle.Adopt(std::move(scoped_handle));
  return std::make_unique<GLFenceWin>(std::move(d3d11_fence),
                                      std::move(gpu_fence_handle));
}

std::unique_ptr<GLFenceWin> GLFenceWin::CreateFromGpuFence(
    const gfx::GpuFence& gpu_fence) {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  return CreateFromGpuFence(d3d11_device.Get(), gpu_fence);
}

std::unique_ptr<GLFenceWin> GLFenceWin::CreateFromGpuFence(
    ID3D11Device* d3d11_device,
    const gfx::GpuFence& gpu_fence) {
  Microsoft::WRL::ComPtr<ID3D11Device5> d3d11_device5;
  HRESULT hr = d3d11_device->QueryInterface(IID_PPV_ARGS(&d3d11_device5));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to retrieve ID3D11Device5 interface "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  gfx::GpuFenceHandle gpu_fence_handle = gpu_fence.GetGpuFenceHandle().Clone();
  Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence;
  hr = d3d11_device5->OpenSharedFence(gpu_fence_handle.Peek(),
                                      IID_PPV_ARGS(&d3d11_fence));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to open a shared fence "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return std::make_unique<GLFenceWin>(std::move(d3d11_fence),
                                      std::move(gpu_fence_handle));
}

GLFenceWin::GLFenceWin(Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence,
                       gfx::GpuFenceHandle gpu_fence_handle)
    : d3d11_fence_(std::move(d3d11_fence)),
      gpu_fence_handle_(std::move(gpu_fence_handle)) {}

GLFenceWin::~GLFenceWin() = default;

bool GLFenceWin::HasCompleted() {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void GLFenceWin::ClientWait() {
  NOTREACHED_IN_MIGRATION();
}

void GLFenceWin::ServerWait() {
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
  d3d11_fence_->GetDevice(&d3d11_device);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
  d3d11_device->GetImmediateContext(&d3d11_device_context);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext4> d3d11_device_context4;
  HRESULT hr = d3d11_device_context.As(&d3d11_device_context4);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to retrieve ID3D11DeviceContext4 interface "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  hr = d3d11_device_context4->Wait(d3d11_fence_.Get(), 1);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to Wait on D3D11 fence "
                << logging::SystemErrorCodeToString(hr);
    return;
  }
}

std::unique_ptr<gfx::GpuFence> GLFenceWin::GetGpuFence() {
  return std::make_unique<gfx::GpuFence>(gpu_fence_handle_.Clone());
}

}  // namespace gl
