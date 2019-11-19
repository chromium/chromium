// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_DXGI_H_
#define UI_GL_GL_IMAGE_DXGI_H_

#include <DXGI1_2.h>
#include <d3d11.h>
#include <wrl/client.h>

#include "base/win/scoped_handle.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

typedef void* EGLStreamKHR;
typedef void* EGLConfig;
typedef void* EGLSurface;

namespace gl {
class GL_EXPORT GLImageDXGI : public GLImage {
 public:
  GLImageDXGI(const gfx::Size& size, EGLStreamKHR stream);

  // Safe downcast. Returns nullptr on failure.
  static GLImageDXGI* FromGLImage(GLImage* image);

  // GLImage implementation.
  BindOrCopy ShouldBindOrCopy() override;
  bool BindTexImage(unsigned target) override;
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  void Flush() override;
  unsigned GetInternalFormat() override;
  unsigned GetDataType() override;
  gfx::Size GetSize() override;
  Type GetType() const override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;
  void ReleaseTexImage(unsigned target) override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override;

  const gfx::ColorSpace& color_space() const { return color_space_; }
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex() { return keyed_mutex_; }
  size_t level() const { return level_; }
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture() { return texture_; }

  bool InitializeHandle(base::win::ScopedHandle handle,
                        uint32_t level,
                        gfx::BufferFormat format);
  void SetTexture(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,
                  size_t level);

 protected:
  ~GLImageDXGI() override;

  gfx::BufferFormat buffer_format_ = gfx::BufferFormat::BGRA_8888;
  base::win::ScopedHandle handle_;
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex_;
  size_t level_ = 0;
  gfx::Size size_;
  EGLSurface surface_ = nullptr;
  EGLStreamKHR stream_ = nullptr;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
};

// This copies to a new texture on bind.
class GL_EXPORT CopyingGLImageDXGI : public GLImageDXGI {
 public:
  CopyingGLImageDXGI(const Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device,
                     const gfx::Size& size,
                     EGLStreamKHR stream);

  bool Initialize();
  bool InitializeVideoProcessor(
      const Microsoft::WRL::ComPtr<ID3D11VideoProcessor>& video_processor,
      const Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>& enumerator);
  void UnbindFromTexture();

  // GLImage implementation.
  bool BindTexImage(unsigned target) override;

 private:
  ~CopyingGLImageDXGI() override;

  bool copied_ = false;

  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device_;
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> d3d11_processor_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> enumerator_;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> decoder_copy_texture_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> output_view_;
};
}

#endif  // UI_GL_GL_IMAGE_DXGI_H_
