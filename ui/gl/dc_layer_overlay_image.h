// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DC_LAYER_OVERLAY_IMAGE_H_
#define UI_GL_DC_LAYER_OVERLAY_IMAGE_H_

#include <wrl/client.h>

#include "base/memory/raw_ptr.h"
#include "ui/gl/dcomp_surface_proxy.h"
#include "ui/gl/gl_export.h"

class ID3D11Texture2D;
class IDCompositionSurface;
class IDXGISwapChain1;
class IUnknown;

namespace gl {

enum class DCLayerOverlayType {
  // Hardware decoder NV12 or P010 video texture - might be a texture array.
  kD3D11Texture,
  // Software decoder NV12 or P010 video Y and UV plane pixmaps.
  kShMemPixmap,
  // Either an IDCompositionSurface or an IDXGISwapChain1
  kDCompVisualContent,
  // gl::DCOMPSurfaceProxy used for MediaFoundation video renderer.
  kDCompSurfaceProxy,
};

// Holds DComp content needed to update the DComp layer tree
class GL_EXPORT DCLayerOverlayImage {
 public:
  DCLayerOverlayImage(
      const gfx::Size& size,
      Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_video_texture,
      size_t array_slice = 0u);
  DCLayerOverlayImage(const gfx::Size& size,
                      const uint8_t* shm_video_pixmap,
                      size_t stride);
  DCLayerOverlayImage(const gfx::Size& size,
                      Microsoft::WRL::ComPtr<IUnknown> dcomp_visual_content,
                      uint64_t dcomp_surface_serial = 0u);
  DCLayerOverlayImage(const gfx::Size& size,
                      scoped_refptr<gl::DCOMPSurfaceProxy> dcomp_surface_proxy);
  DCLayerOverlayImage(DCLayerOverlayImage&&);
  DCLayerOverlayImage& operator=(DCLayerOverlayImage&&);
  ~DCLayerOverlayImage();

  DCLayerOverlayType type() const { return type_; }
  const gfx::Size& size() const { return size_; }

  ID3D11Texture2D* d3d11_video_texture() const {
    return d3d11_video_texture_.Get();
  }
  size_t texture_array_slice() const { return texture_array_slice_; }

  const uint8_t* shm_video_pixmap() const { return shm_video_pixmap_; }
  size_t pixmap_stride() const { return pixmap_stride_; }

  IUnknown* dcomp_visual_content() const { return dcomp_visual_content_.Get(); }
  uint64_t dcomp_surface_serial() const { return dcomp_surface_serial_; }

  gl::DCOMPSurfaceProxy* dcomp_surface_proxy() const {
    return dcomp_surface_proxy_.get();
  }

  bool operator==(const DCLayerOverlayImage& other) const {
    return std::tie(type_, size_, d3d11_video_texture_, texture_array_slice_,
                    shm_video_pixmap_, pixmap_stride_, dcomp_visual_content_,
                    dcomp_surface_serial_, dcomp_surface_proxy_) ==
           std::tie(other.type_, other.size_, other.d3d11_video_texture_,
                    other.texture_array_slice_, other.shm_video_pixmap_,
                    other.pixmap_stride_, other.dcomp_visual_content_,
                    other.dcomp_surface_serial_, other.dcomp_surface_proxy_);
  }

 private:
  // Type of overlay image.
  DCLayerOverlayType type_;
  // Size of overlay image.
  gfx::Size size_;
  // Hardware decoder NV12 or P010 video texture - can be a texture array.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_video_texture_;
  // Array slice/index if |d3d11_video_texture_| is a texture array.
  size_t texture_array_slice_ = 0;
  // Software decoder NV12 or P010 frame pixmap.
  raw_ptr<const uint8_t, DanglingUntriaged> shm_video_pixmap_ = nullptr;
  // Software video pixmap stride. Y and UV planes have the same stride in NV12.
  size_t pixmap_stride_ = 0;
  // Either an IDCompositionSurface or an IDXGISwapChain1
  Microsoft::WRL::ComPtr<IUnknown> dcomp_visual_content_;
  // This is a number that increments once for every EndDraw on a surface, and
  // is used to determine when the contents have changed so Commit() needs to
  // be called on the device.
  uint64_t dcomp_surface_serial_ = 0;
  // DCOMPSurfaceProxy used for MediaFoundation video renderer.
  scoped_refptr<gl::DCOMPSurfaceProxy> dcomp_surface_proxy_;
};

}  // namespace gl

#endif  // UI_GL_DC_LAYER_OVERLAY_IMAGE_H_
