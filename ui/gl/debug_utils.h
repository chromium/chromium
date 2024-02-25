// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DEBUG_UTILS_H_
#define UI_GL_DEBUG_UTILS_H_

#include "base/win/windows_types.h"
#include "ui/gl/gl_export.h"

struct ID3D11DeviceChild;
struct IDXGIObject;

namespace gl {

// Set the debug name of a D3D11 resource for use with ETW debugging tools.
// D3D11 retains the string passed to this function.
HRESULT GL_EXPORT SetDebugName(ID3D11DeviceChild* d3d11_device_child,
                               const char* debug_string);
HRESULT GL_EXPORT SetDebugName(IDXGIObject* dxgi_object,
                               const char* debug_string);

}  // namespace gl

#endif  // UI_GL_DEBUG_UTILS_H_
