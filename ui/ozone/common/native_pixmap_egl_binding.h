// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_NATIVE_PIXMAP_EGL_BINDING_H_
#define UI_OZONE_COMMON_NATIVE_PIXMAP_EGL_BINDING_H_

#include <memory>

#include "base/threading/thread_checker.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/scoped_egl_image.h"
#include "ui/ozone/public/native_pixmap_gl_binding.h"

namespace ui {

// A binding maintained between NativePixmap and GL Textures in Ozone.
class NativePixmapEGLBinding : public NativePixmapGLBinding {
 public:
  NativePixmapEGLBinding(const gfx::Size& size,
                         gfx::BufferFormat format,
                         gfx::BufferPlane plane);
  ~NativePixmapEGLBinding() override;

  static bool IsBufferFormatSupported(gfx::BufferFormat format);

  // Create an EGLImage from a given NativePixmap and plane and bind
  // |texture_id| to |target| followed by binding the image to |target|. The
  // color space is for the external sampler: When we sample the YUV buffer as
  // RGB, we need to tell it the encoding (BT.601, BT.709, or BT.2020) and range
  // (limited or null), and |color_space| conveys this.
  static std::unique_ptr<NativePixmapGLBinding> Create(
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat plane_format,
      gfx::BufferPlane plane,
      gfx::Size plane_size,
      const gfx::ColorSpace& color_space,
      GLenum target,
      GLuint texture_id);

 private:
  // Create an EGLImage from a given NativePixmap and bind |texture_id| to
  // |target| followed by binding the image to |target|. This EGLImage can be
  // converted to a GL texture.
  bool InitializeFromNativePixmap(scoped_refptr<gfx::NativePixmap> pixmap,
                                  const gfx::ColorSpace& color_space,
                                  GLenum target,
                                  GLuint texture_id);

  gl::ScopedEGLImage egl_image_;
  const gfx::Size size_;
  THREAD_CHECKER(thread_checker_);
  gfx::BufferFormat format_;
  scoped_refptr<gfx::NativePixmap> pixmap_;
  gfx::BufferPlane plane_;
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_NATIVE_PIXMAP_EGL_BINDING_H_
