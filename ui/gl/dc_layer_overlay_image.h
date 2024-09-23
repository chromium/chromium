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
  // Hardware decoder NV12 video texture - might be a texture array.
  kNV12Texture,
  // Software decoder NV12 video Y and UV plane pixmaps.
  kNV12Pixmap,
  // Either an IDCompositionSurface or an IDXGISwapChain1
  kDCompVisualContent,
  // gl::DCOMPSurfaceProxy used for MediaFoundation video renderer.
  kDCompSurfaceProxy,
};

// Holds DComp content needed to update the DComp layer tree
class GL_EXPORT DCLayerOverlayImage {
 public:
  DCLayerOverlayImage(const gfx::Size& size,
                      Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12_texture,
                      size_t array_slice = 0u);
  DCLayerOverlayImage(const gfx::Size& size,
                      const uint8_t* nv12_pixmap,
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

  ID3D11Texture2D* nv12_texture() const { return nv12_texture_.Get(); }
  size_t texture_array_slice() const { return texture_array_slice_; }

  const uint8_t* nv12_pixmap() const { return nv12_pixmap_; }
  size_t pixmap_stride() const { return pixmap_stride_; }

  IUnknown* dcomp_visual_content() const { return dcomp_visual_content_.Get(); }
  uint64_t dcomp_surface_serial() const { return dcomp_surface_serial_; }

  gl::DCOMPSurfaceProxy* dcomp_surface_proxy() const {
    return dcomp_surface_proxy_.get();
  }

  bool operator==(const DCLayerOverlayImage& other) const {
    return std::tie(type_, size_, nv12_texture_, texture_array_slice_,
                    nv12_pixmap_, pixmap_stride_, dcomp_visual_content_,
                    dcomp_surface_serial_, dcomp_surface_proxy_) ==
           std::tie(other.type_, other.size_, other.nv12_texture_,
                    other.texture_array_slice_, other.nv12_pixmap_,
                    other.pixmap_stride_, other.dcomp_visual_content_,
                    other.dcomp_surface_serial_, other.dcomp_surface_proxy_);
  }

 private:
  // Type of overlay image.
  DCLayerOverlayType type_;
  // Size of overlay image.
  gfx::Size size_;
  // Hardware decoder NV12 video texture - can be a texture array.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12_texture_;
  // Array slice/index if |texture_| is a texture array.
  size_t texture_array_slice_ = 0;
  // Software decoder NV12 frame pixmap.
  raw_ptr<const uint8_t, DanglingUntriaged> nv12_pixmap_ = nullptr;
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
