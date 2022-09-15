// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_FENCE_WIN_H_
#define UI_GL_GL_FENCE_WIN_H_

#include "ui/gl/gl_fence.h"

#include <d3d11_3.h>
#include <wrl/client.h>

namespace gl {

class GL_EXPORT GLFenceWin : public GLFence {
 public:
  static std::unique_ptr<GLFenceWin> CreateForGpuFence();
  static std::unique_ptr<GLFenceWin> CreateForGpuFence(ID3D11Device*);
  static std::unique_ptr<GLFenceWin> CreateFromGpuFence(
      const gfx::GpuFence& gpu_fence);
  static std::unique_ptr<GLFenceWin> CreateFromGpuFence(
      ID3D11Device*,
      const gfx::GpuFence& gpu_fence);
  static bool IsSupported();

  GLFenceWin(Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence,
             gfx::GpuFenceHandle gpu_fence_handle);
  ~GLFenceWin() override;

  bool HasCompleted() override;
  void ClientWait() override;
  void ServerWait() override;
  std::unique_ptr<gfx::GpuFence> GetGpuFence() override;

 private:
  Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence_;
  gfx::GpuFenceHandle gpu_fence_handle_;
};

}  // namespace gl

#endif  // UI_GL_GL_FENCE_WIN_H_
