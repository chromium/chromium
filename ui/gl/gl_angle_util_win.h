// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_ANGLE_UTIL_WIN_H_
#define UI_GL_GL_ANGLE_UTIL_WIN_H_

#include <d3d11.h>
#include <d3d9.h>
#include <wrl/client.h>

#include "ui/gl/gl_export.h"

namespace gl {

GL_EXPORT Microsoft::WRL::ComPtr<ID3D11Device>
QueryD3D11DeviceObjectFromANGLE();
GL_EXPORT Microsoft::WRL::ComPtr<IDirect3DDevice9>
QueryD3D9DeviceObjectFromANGLE();

}  // namespace gl

#endif  // UI_GL_GL_ANGLE_UTIL_WIN_H_
