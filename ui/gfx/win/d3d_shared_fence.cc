// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/d3d_shared_fence.h"

#include "base/debug/alias.h"
#include "base/logging.h"

namespace gfx {

namespace {
Microsoft::WRL::ComPtr<ID3D11DeviceContext4> GetDeviceContext4(
    ID3D11Device* d3d11_device) {
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  d3d11_device->GetImmediateContext(&context);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context4;
  HRESULT hr = context.As(&context4);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get ID3D11DeviceContext4: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }
  return context4;
}

base::win::ScopedHandle DuplicateSharedHandle(HANDLE shared_handle) {
  const base::ProcessHandle process = ::GetCurrentProcess();
  HANDLE duplicated_handle = INVALID_HANDLE_VALUE;
  const BOOL result =
      ::DuplicateHandle(process, shared_handle, process, &duplicated_handle, 0,
                        FALSE, DUPLICATE_SAME_ACCESS);
  if (!result) {
    const DWORD last_error = ::GetLastError();
    base::debug::Alias(&last_error);
    CHECK(false);
  }
  return base::win::ScopedHandle(duplicated_handle);
}
}  // namespace

// static
scoped_refptr<D3DSharedFence> D3DSharedFence::CreateForD3D11(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_signal_device) {
  Microsoft::WRL::ComPtr<ID3D11Device5> d3d11_device5;
  HRESULT hr = d3d11_signal_device.As(&d3d11_device5);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get ID3D11Device5: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence;
  hr = d3d11_device5->CreateFence(0, D3D11_FENCE_FLAG_SHARED,
                                  IID_PPV_ARGS(&d3d11_fence));
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateFence failed with error "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return CreateFromD3D11Fence(std::move(d3d11_signal_device),
                              std::move(d3d11_fence),
                              /*fence_value=*/0);
}

// static
scoped_refptr<D3DSharedFence> D3DSharedFence::CreateFromD3D11Fence(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_signal_device,
    Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence,
    uint64_t fence_value) {
  HANDLE shared_handle = nullptr;
  HRESULT hr = d3d11_fence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr,
                                               &shared_handle);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Unable to create shared handle for D3D11Fence: "
                << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }
  auto fence = base::WrapRefCounted(new D3DSharedFence(
      base::win::ScopedHandle(shared_handle), DXGIHandleToken()));
  fence->d3d11_signal_device_ = std::move(d3d11_signal_device);
  fence->d3d11_signal_fence_ = std::move(d3d11_fence);
  fence->fence_value_ = fence_value;
  return fence;
}

// static
bool D3DSharedFence::IsSupported(ID3D11Device* d3d11_device) {
  DCHECK(d3d11_device);
  Microsoft::WRL::ComPtr<ID3D11Device5> d3d11_device5;
  HRESULT hr = d3d11_device->QueryInterface(IID_PPV_ARGS(&d3d11_device5));
  if (FAILED(hr)) {
    DVLOG(1) << "Failed to get ID3D11Device5: "
             << logging::SystemErrorCodeToString(hr);
    return false;
  }
  return true;
}

// static
scoped_refptr<D3DSharedFence> D3DSharedFence::CreateFromScopedHandle(
    base::win::ScopedHandle fence_handle,
    const DXGIHandleToken& fence_token) {
  return base::WrapRefCounted(
      new D3DSharedFence(std::move(fence_handle), fence_token));
}

scoped_refptr<D3DSharedFence> D3DSharedFence::CreateFromUnownedHandle(
    HANDLE shared_handle) {
  return base::WrapRefCounted(new D3DSharedFence(
      DuplicateSharedHandle(shared_handle), DXGIHandleToken()));
}

D3DSharedFence::D3DSharedFence(base::win::ScopedHandle shared_handle,
                               const DXGIHandleToken& dxgi_token)
    : shared_handle_(std::move(shared_handle)),
      dxgi_token_(dxgi_token),
      d3d11_wait_fence_map_(kMaxD3D11FenceMapSize) {}

D3DSharedFence::~D3DSharedFence() = default;

HANDLE D3DSharedFence::GetSharedHandle() const {
  return shared_handle_.get();
}

base::win::ScopedHandle D3DSharedFence::CloneSharedHandle() {
  return DuplicateSharedHandle(shared_handle_.get());
}

uint64_t D3DSharedFence::GetFenceValue() const {
  return fence_value_;
}

const DXGIHandleToken& D3DSharedFence::GetDXGIHandleToken() const {
  return dxgi_token_;
}

Microsoft::WRL::ComPtr<ID3D11Device> D3DSharedFence::GetD3D11Device() const {
  return d3d11_signal_device_;
}

bool D3DSharedFence::IsSameFenceAsHandle(HANDLE shared_handle) const {
  return CompareObjectHandles(shared_handle_.Get(), shared_handle);
}

void D3DSharedFence::Update(uint64_t fence_value) {
  if (fence_value > fence_value_) {
    fence_value_ = fence_value;
  }
}

bool D3DSharedFence::WaitD3D11(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_wait_device) {
  // Skip wait if passed in device is the same as signaling device.
  if (d3d11_wait_device == d3d11_signal_device_) {
    return true;
  }

  auto it = d3d11_wait_fence_map_.Get(d3d11_wait_device);
  if (it == d3d11_wait_fence_map_.end()) {
    Microsoft::WRL::ComPtr<ID3D11Device5> d3d11_device5;
    HRESULT hr = d3d11_wait_device.As(&d3d11_device5);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get ID3D11Device5: "
                  << logging::SystemErrorCodeToString(hr);
      return false;
    }
    Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence;
    hr = d3d11_device5->OpenSharedFence(shared_handle_.get(),
                                        IID_PPV_ARGS(&d3d11_fence));
    if (FAILED(hr)) {
      DLOG(ERROR) << "OpenSharedFence failed: "
                  << logging::SystemErrorCodeToString(hr);
      return false;
    }
    it = d3d11_wait_fence_map_.Put(d3d11_wait_device, std::move(d3d11_fence));
  }

  const Microsoft::WRL::ComPtr<ID3D11Fence>& fence = it->second;
  // Skip wait if we're already past the wait value.
  if (fence->GetCompletedValue() >= fence_value_) {
    return true;
  }

  Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context4 =
      GetDeviceContext4(d3d11_wait_device.Get());
  if (!context4) {
    return false;
  }

  HRESULT hr = context4->Wait(fence.Get(), fence_value_);
  if (FAILED(hr)) {
    DLOG(ERROR) << "D3D11 fence wait failed: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }
  return true;
}

bool D3DSharedFence::IncrementAndSignalD3D11() {
  CHECK(d3d11_signal_device_)
      << "D3D11 fence is expected to be signaled externally";
  DCHECK(d3d11_signal_fence_);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context4 =
      GetDeviceContext4(d3d11_signal_device_.Get());
  if (!context4) {
    return false;
  }

  HRESULT hr = context4->Signal(d3d11_signal_fence_.Get(), fence_value_ + 1);
  if (FAILED(hr)) {
    DLOG(ERROR) << "D3D11 fence signal failed: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }
  fence_value_++;
  return true;
}

}  // namespace gfx
