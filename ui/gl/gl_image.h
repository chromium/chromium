// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_H_
#define UI_GL_GL_IMAGE_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gl/gl_export.h"

namespace gpu {
class D3DImageBacking;
class D3DImageBackingFactoryTest;
class GLTextureImageBacking;
class GLTexturePassthroughD3DImageRepresentation;
FORWARD_DECLARE_TEST(CALayerTreeTest, HDRTrigger);
FORWARD_DECLARE_TEST(D3DImageBackingFactoryTestSwapChain,
                     CreateAndPresentSwapChain);
FORWARD_DECLARE_TEST(GpuOESEGLImageTest, EGLImageToTexture);
}  // namespace gpu

namespace gpu::gles2 {
class GLES2DecoderImpl;
class GLES2DecoderPassthroughImpl;
class PassthroughAbstractTextureImpl;
class Texture;
class ValidatingAbstractTextureImpl;
}  // namespace gpu::gles2

namespace media {
class GLImageEGLStream;
class GLImagePbuffer;
class DXVAVideoDecodeAccelerator;
class VaapiPictureNativePixmapAngle;
class VaapiPictureNativePixmapEgl;
class VaapiPictureNativePixmapOzone;
class V4L2SliceVideoDecodeAccelerator;
class VTVideoDecodeAccelerator;
}

namespace ui {
class NativePixmapGLBinding;
class SurfacelessGlRenderer;
class SurfacelessSkiaGlRenderer;
}  // namespace ui

namespace viz {
class ImageContextImpl;
}  // namespace viz

namespace gl {

class GLImageD3D;

// Encapsulates an image that can be bound and/or copied to a texture, hiding
// platform specific management.
class GL_EXPORT GLImage : public base::RefCounted<GLImage> {
 public:
  GLImage(const GLImage&) = delete;
  GLImage& operator=(const GLImage&) = delete;

 protected:
  // NOTE: We are in the process of eliminating client usage of GLImage. As part
  // of this effort, we have moved its public interface to be protected with
  // friend'ing of existing users. DO NOT ADD MORE client usage - instead, reach
  // out to shared-image-team@ with your use case.
  // See crbug.com/1382031.
  GLImage() = default;

  virtual ~GLImage() = default;

  // Get the size of the image.
  virtual gfx::Size GetSize();

  // Bind image to texture currently bound to |target|. Returns true on success.
  // It is valid for an implementation to always return false.
  virtual bool BindTexImage(unsigned target);

 public:
  // Allow usage of these methods from text sites that are inconvenient to
  // friend.
  gfx::Size GetSizeForTesting() { return GetSize(); }
  bool BindTexImageForTesting(unsigned target) { return BindTexImage(target); }

 protected:
  // An identifier for subclasses. Necessary for safe downcasting.
  enum class Type { NONE, EGL_STREAM, D3D, PBUFFER };
  virtual Type GetType() const;

 private:
  // Safe downcasts. All functions return nullptr if |image| does not exist or
  // does not have the specified type.
  static GLImageD3D* ToGLImageD3D(GLImage* image);
  static media::GLImageEGLStream* ToGLImageEGLStream(GLImage* image);
  static media::GLImagePbuffer* ToGLImagePbuffer(GLImage* image);

  friend class gpu::D3DImageBacking;
  friend class gpu::D3DImageBackingFactoryTest;
  friend class gpu::GLTextureImageBacking;
  friend class gpu::GLTexturePassthroughD3DImageRepresentation;
  friend class gpu::gles2::GLES2DecoderImpl;
  friend class gpu::gles2::GLES2DecoderPassthroughImpl;
  friend class gpu::gles2::PassthroughAbstractTextureImpl;
  friend class gpu::gles2::Texture;
  friend class gpu::gles2::ValidatingAbstractTextureImpl;
  friend class media::DXVAVideoDecodeAccelerator;
  friend class media::VaapiPictureNativePixmapAngle;
  friend class media::VaapiPictureNativePixmapEgl;
  friend class media::VaapiPictureNativePixmapOzone;
  friend class media::V4L2SliceVideoDecodeAccelerator;
  friend class media::VTVideoDecodeAccelerator;
  friend class ui::NativePixmapGLBinding;
  friend class ui::SurfacelessGlRenderer;
  friend class ui::SurfacelessSkiaGlRenderer;
  friend class viz::ImageContextImpl;
  FRIEND_TEST_ALL_PREFIXES(gpu::GpuOESEGLImageTest, EGLImageToTexture);
  FRIEND_TEST_ALL_PREFIXES(gpu::CALayerTreeTest, HDRTrigger);
  FRIEND_TEST_ALL_PREFIXES(gpu::D3DImageBackingFactoryTestSwapChain,
                           CreateAndPresentSwapChain);

  friend class base::RefCounted<GLImage>;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_H_
