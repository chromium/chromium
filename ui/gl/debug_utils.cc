// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/debug_utils.h"

#include <d3d11.h>
#include <dxgi.h>

namespace gl {

namespace {

// ID3D11DeviceChild and IDXGIObject implement SetPrivateData with
// the exact same parameters.
template <typename T>
HRESULT SetDebugNameInternal(T* d3d11_object, const char* debug_string) {
  return d3d11_object->SetPrivateData(WKPDID_D3DDebugObjectName,
                                      strlen(debug_string), debug_string);
}

}  // namespace

HRESULT SetDebugName(ID3D11DeviceChild* d3d11_device_child,
                     const char* debug_string) {
  return SetDebugNameInternal(d3d11_device_child, debug_string);
}

HRESULT SetDebugName(IDXGIObject* dxgi_object, const char* debug_string) {
  return SetDebugNameInternal(dxgi_object, debug_string);
}

}  // namespace gl
