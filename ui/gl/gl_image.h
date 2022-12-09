// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_H_
#define UI_GL_GL_IMAGE_H_

#include <stdint.h>

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gl/gl_export.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}  // namespace trace_event
}  // namespace base

namespace gl {
class GLImage;
}

namespace gpu {
class DawnEGLImageRepresentation;
class D3DImageBacking;
class D3DImageBackingFactoryTest;
class GpuMemoryBufferFactoryAndroidHardwareBuffer;
class GpuMemoryBufferFactoryDXGI;
class GpuMemoryBufferFactoryIOSurface;
class IOSurfaceImageBackingFactory;
class IOSurfaceImageBackingFactoryNewTestBase;
class OverlayD3DImageRepresentation;
class TestOverlayImageRepresentation;
void SetColorSpaceOnGLImage(gl::GLImage* gl_image,
                            const gfx::ColorSpace& color_space);
FORWARD_DECLARE_TEST(CALayerTreeTest, HDRTrigger);
FORWARD_DECLARE_TEST(CompoundImageBackingTest, NoUploadOnOverlayMemoryAccess);
FORWARD_DECLARE_TEST(D3DImageBackingFactoryTestSwapChain,
                     CreateAndPresentSwapChain);
FORWARD_DECLARE_TEST(D3DImageBackingFactoryTest, CreateFromSharedMemory);
}  // namespace gpu

namespace gpu::gles2 {
class GLES2DecoderImpl;
class GLES2DecoderPassthroughImpl;
class Texture;
}

namespace media {
class GLImagePbuffer;
class DXVAVideoDecodeAccelerator;
class VTVideoDecodeAccelerator;
}

namespace ui {
class NativePixmapEGLBinding;
class SurfacelessGlRenderer;
class SurfacelessSkiaGlRenderer;
}  // namespace ui

namespace viz {
class ImageContextImpl;
class SkiaOutputDeviceDComp;
}  // namespace viz

namespace gl {

class DCompPresenterTest;
class DirectCompositionSurfaceTest;
class GLImageD3D;
class GLImageDXGI;
class GLImageIOSurface;
class GLImageMemory;
class SwapChainPresenter;

// Encapsulates an image that can be bound and/or copied to a texture, hiding
// platform specific management.
class GL_EXPORT GLImage : public base::RefCounted<GLImage> {
 public:
  GLImage(const GLImage&) = delete;
  GLImage& operator=(const GLImage&) = delete;

  // Get the size of the image.
  virtual gfx::Size GetSize();

  // Get the GL internal format, format, type of the image.
  // They are aligned with glTexImage{2|3}D's parameters |internalformat|,
  // |format|, and |type|.
  // The returned enums are based on ES2 contexts and are mostly ES3
  // compatible, except for GL_HALF_FLOAT_OES.
  virtual unsigned GetInternalFormat();
  virtual unsigned GetDataFormat();
  virtual unsigned GetDataType();

  enum BindOrCopy { BIND, COPY };
  // Returns whether this image is meant to be bound or copied to textures. The
  // suggested method is not guaranteed to succeed, but the alternative will
  // definitely fail.
  virtual BindOrCopy ShouldBindOrCopy();

  // Bind image to texture currently bound to |target|. Returns true on success.
  // It is valid for an implementation to always return false.
  virtual bool BindTexImage(unsigned target);

  // Release image from texture currently bound to |target|.
  virtual void ReleaseTexImage(unsigned target);

 protected:
  // NOTE: We are in the process of eliminating client usage of GLImage. As part
  // of this effort, we are incrementally moving its public interface to be
  // protected with friend'ing of existing users. DO NOT ADD MORE client usage -
  // instead, reach out to shared-image-team@ with your use case.
  // See crbug.com/1382031.
  GLImage() = default;

  virtual ~GLImage() = default;

  // Define texture currently bound to |target| by copying image into it.
  // Returns true on success. It is valid for an implementation to always
  // return false.
  virtual bool CopyTexImage(unsigned target);

  // Copy |rect| of image to |offset| in texture currently bound to |target|.
  // Returns true on success. It is valid for an implementation to always
  // return false.
  virtual bool CopyTexSubImage(unsigned target,
                               const gfx::Point& offset,
                               const gfx::Rect& rect);

  // Set the color space when image is used as an overlay. The color space may
  // also be useful for images backed by YUV buffers: if the GL driver can
  // sample the YUV buffer as RGB, we need to tell it the encoding (BT.601,
  // BT.709, or BT.2020) and range (limited or null), and |color_space| conveys
  // this.
  virtual void SetColorSpace(const gfx::ColorSpace& color_space);

  // Dumps information about the memory backing the GLImage to a dump named
  // |dump_name|.
  virtual void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                            uint64_t process_tracing_id,
                            const std::string& dump_name);

  // An identifier for subclasses. Necessary for safe downcasting.
  enum class Type {
    NONE,
    MEMORY,
    IOSURFACE,
    DXGI_IMAGE,
    D3D,
    DCOMP_SURFACE,
    PBUFFER
  };
  virtual Type GetType() const;

  // Returns the NativePixmap backing the GLImage. If not backed by a
  // NativePixmap, returns null.
  virtual scoped_refptr<gfx::NativePixmap> GetNativePixmap();

  virtual void* GetEGLImage() const;

  gfx::ColorSpace color_space_;

 private:
  // Safe downcasts. All functions return nullptr if |image| does not exist or
  // does not have the specified type.
  static GLImageD3D* ToGLImageD3D(GLImage* image);
  static GLImageMemory* ToGLImageMemory(GLImage* image);
  static GLImageIOSurface* ToGLImageIOSurface(GLImage* image);
  static GLImageDXGI* ToGLImageDXGI(GLImage* image);
  static media::GLImagePbuffer* ToGLImagePbuffer(GLImage* image);

  friend class DCompPresenterTest;
  friend class DirectCompositionSurfaceTest;
  friend class SwapChainPresenter;
  friend class gpu::DawnEGLImageRepresentation;
  friend class gpu::D3DImageBacking;
  friend class gpu::D3DImageBackingFactoryTest;
  friend class gpu::GpuMemoryBufferFactoryAndroidHardwareBuffer;
  friend class gpu::GpuMemoryBufferFactoryDXGI;
  friend class gpu::GpuMemoryBufferFactoryIOSurface;
  friend class gpu::IOSurfaceImageBackingFactory;
  friend class gpu::IOSurfaceImageBackingFactoryNewTestBase;
  friend class gpu::OverlayD3DImageRepresentation;
  friend class gpu::TestOverlayImageRepresentation;
  friend class gpu::gles2::GLES2DecoderImpl;
  friend class gpu::gles2::GLES2DecoderPassthroughImpl;
  friend class gpu::gles2::Texture;
  friend class media::DXVAVideoDecodeAccelerator;
  friend class media::VTVideoDecodeAccelerator;
  friend class ui::NativePixmapEGLBinding;
  friend class ui::SurfacelessGlRenderer;
  friend class ui::SurfacelessSkiaGlRenderer;
  friend class viz::ImageContextImpl;
  friend class viz::SkiaOutputDeviceDComp;
  friend void gpu::SetColorSpaceOnGLImage(GLImage* gl_image,
                                          const gfx::ColorSpace& color_space);
  FRIEND_TEST_ALL_PREFIXES(gpu::CALayerTreeTest, HDRTrigger);
  FRIEND_TEST_ALL_PREFIXES(gpu::CompoundImageBackingTest,
                           NoUploadOnOverlayMemoryAccess);
  FRIEND_TEST_ALL_PREFIXES(gpu::D3DImageBackingFactoryTestSwapChain,
                           CreateAndPresentSwapChain);
  FRIEND_TEST_ALL_PREFIXES(gpu::D3DImageBackingFactoryTest,
                           CreateFromSharedMemory);

  friend class base::RefCounted<GLImage>;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_H_
