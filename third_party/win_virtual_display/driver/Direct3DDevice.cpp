// Copyright (c) Microsoft Corporation

#include "Direct3DDevice.h"

namespace display::test {
Direct3DDevice::Direct3DDevice(LUID AdapterLuid) : AdapterLuid(AdapterLuid) {}

Direct3DDevice::Direct3DDevice() {
  AdapterLuid = LUID{};
}

HRESULT Direct3DDevice::Init() {
  // The DXGI factory could be cached, but if a new render adapter appears on
  // the system, a new factory needs to be created. If caching is desired, check
  // DxgiFactory->IsCurrent() each time and recreate the factory if !IsCurrent.
  HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&(DxgiFactory)));
  if (FAILED(hr)) {
    return hr;
  }

  // Find the specified render adapter
  hr = DxgiFactory->EnumAdapterByLuid(AdapterLuid, IID_PPV_ARGS(&(Adapter)));
  if (FAILED(hr)) {
    return hr;
  }

  // Create a D3D device using the render adapter. BGRA support is required by
  // the WHQL test suite.
  hr = D3D11CreateDevice(Adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                         D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                         D3D11_SDK_VERSION, &Device, nullptr, &DeviceContext);
  if (FAILED(hr)) {
    // If creating the D3D device failed, it's possible the render GPU was lost
    // (e.g. detachable GPU) or else the system is in a transient state.
    return hr;
  }

  return S_OK;
}
}  // namespace display::test
