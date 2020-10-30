// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>

#include <memory>

#include "base/logging.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_glx.h"
#include "ui/gl/gl_surface_glx.h"
#include "ui/gl/gl_visual_picker_glx.h"
#include "ui/gl/glx_util.h"

namespace gl {
namespace {

int TextureFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRA_1010102:
      return GLX_TEXTURE_FORMAT_RGB_EXT;
    case gfx::BufferFormat::BGRA_8888:
      return GLX_TEXTURE_FORMAT_RGBA_EXT;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

GLImageGLX::GLImageGLX(const gfx::Size& size, gfx::BufferFormat format)
    : glx_pixmap_(0), size_(size), format_(format) {}

GLImageGLX::~GLImageGLX() {
  if (glx_pixmap_) {
    glXDestroyGLXPixmap(
        x11::Connection::Get()->GetXlibDisplay(x11::XlibDisplayType::kFlushing),
        glx_pixmap_);
  }
}

bool GLImageGLX::Initialize(x11::Pixmap pixmap) {
  auto fbconfig_id =
      GLVisualPickerGLX::GetInstance()->GetFbConfigForFormat(format_);

  auto* connection = x11::Connection::Get();
  GLXFBConfig config = GetGlxFbConfigForXProtoFbConfig(connection, fbconfig_id);
  if (!config)
    return false;

  int pixmap_attribs[] = {GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
                          GLX_TEXTURE_FORMAT_EXT, TextureFormat(format_), 0};
  glx_pixmap_ = glXCreatePixmap(
      x11::Connection::Get()->GetXlibDisplay(x11::XlibDisplayType::kSyncing),
      config, static_cast<::Pixmap>(pixmap), pixmap_attribs);
  if (!glx_pixmap_) {
    DVLOG(0) << "glXCreatePixmap failed.";
    return false;
  }

  return true;
}

gfx::Size GLImageGLX::GetSize() {
  return size_;
}

unsigned GLImageGLX::GetInternalFormat() {
  return gl::BufferFormatToGLInternalFormat(format_);
}

unsigned GLImageGLX::GetDataType() {
  return GL_UNSIGNED_BYTE;
}

GLImageGLX::BindOrCopy GLImageGLX::ShouldBindOrCopy() {
  return BIND;
}

bool GLImageGLX::BindTexImage(unsigned target) {
  if (!glx_pixmap_)
    return false;

  // Requires TEXTURE_2D target.
  if (target != GL_TEXTURE_2D)
    return false;

  glXBindTexImageEXT(
      x11::Connection::Get()->GetXlibDisplay(x11::XlibDisplayType::kFlushing),
      glx_pixmap_, GLX_FRONT_LEFT_EXT, nullptr);
  return true;
}

void GLImageGLX::ReleaseTexImage(unsigned target) {
  DCHECK_NE(0u, glx_pixmap_);
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), target);

  glXReleaseTexImageEXT(
      x11::Connection::Get()->GetXlibDisplay(x11::XlibDisplayType::kFlushing),
      glx_pixmap_, GLX_FRONT_LEFT_EXT);
}

bool GLImageGLX::CopyTexImage(unsigned target) {
  NOTREACHED();
  return false;
}

bool GLImageGLX::CopyTexSubImage(unsigned target,
                                 const gfx::Point& offset,
                                 const gfx::Rect& rect) {
  return false;
}

bool GLImageGLX::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int z_order,
    gfx::OverlayTransform transform,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  return false;
}

void GLImageGLX::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                              uint64_t process_tracing_id,
                              const std::string& dump_name) {
  // TODO(ericrk): Implement GLImage OnMemoryDump. crbug.com/514914
}

}  // namespace gl
