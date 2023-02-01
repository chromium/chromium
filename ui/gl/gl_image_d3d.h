// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_D3D_H_
#define UI_GL_GL_IMAGE_D3D_H_

#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/memory/raw_ptr.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

namespace gl {

class GL_EXPORT GLImageD3D : public GLImage {
 public:
  // Creates a GLImage backed by a D3D11 |texture| with given |size| and GL
  // unsized |internal_format|, optionally associated with |swap_chain|.  The
  // |internal_format| is passed to ANGLE and used for GL
  // operations.  |internal_format| may be different from the internal format
  // associated with the DXGI_FORMAT of the texture (e.g. RGB instead of
  // BGRA_EXT for DXGI_FORMAT_B8G8R8A8_UNORM). |array_slice| is used
  // when the |texture| is a D3D11 texture array, and |plane_index| is used for
  // specifying the plane to bind to for multi-planar YUV textures.
  // See EGL_ANGLE_d3d_texture_client_buffer spec for format restrictions.
  GLImageD3D(const gfx::Size& size,
             unsigned internal_format,
             Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
             size_t array_slice = 0,
             size_t plane_index = 0,
             Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain = nullptr);

  GLImageD3D(const GLImageD3D&) = delete;
  GLImageD3D& operator=(const GLImageD3D&) = delete;

  bool Initialize();

  void* GetEGLImage() const;

  // Binds image to texture currently bound to |target|. Returns true on
  // success.
  bool BindTexImage(unsigned target);

  // GLImage implementation
  Type GetType() const override;
  gfx::Size GetSize() override;

  const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture() const {
    return texture_;
  }
  const Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain() const {
    return swap_chain_;
  }
  size_t array_slice() const { return array_slice_; }
  size_t plane_index() const { return plane_index_; }

 protected:
  const gfx::Size size_;
  const unsigned internal_format_;  // GLenum

  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
  const size_t array_slice_;
  const size_t plane_index_;

  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;

 private:
  ~GLImageD3D() override;

  raw_ptr<void> egl_image_ = nullptr;  // EGLImageKHR
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_D3D_H_
