// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_memory.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_version_info.h"

using gfx::BufferFormat;

namespace gl {
namespace {

bool ValidInternalFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_RED:
    case GL_RG:
    case GL_RGB:
    case GL_RGBA:
    case GL_RGB10_A2_EXT:
    case GL_BGRA_EXT:
      return true;
    default:
      return false;
  }
}

GLenum TextureFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return GL_RED;
    case gfx::BufferFormat::R_16:
      return GL_R16_EXT;
    case gfx::BufferFormat::RG_88:
      return GL_RG;
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBA_F16:
      return GL_RGBA;
    case gfx::BufferFormat::BGRA_8888:
      return GL_BGRA_EXT;
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRX_8888:
      return GL_RGB;
    case gfx::BufferFormat::BGRX_1010102:
    case gfx::BufferFormat::RGBX_1010102:
      // Technically speaking we should use an opaque format, but neither
      // OpenGLES nor OpenGL supports the hypothetical GL_RGB10_EXT.
      return GL_RGB10_A2_EXT;
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::UYVY_422:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

GLenum DataFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBX_1010102:
      return GL_RGBA;
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRX_1010102:
      return GL_BGRA_EXT;
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::RGBA_F16:
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88:
      return TextureFormat(format);
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::UYVY_422:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

GLenum DataType(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGR_565:
      return GL_UNSIGNED_SHORT_5_6_5_REV;
    case gfx::BufferFormat::RGBA_4444:
      return GL_UNSIGNED_SHORT_4_4_4_4;
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::RG_88:
      return GL_UNSIGNED_BYTE;
    case gfx::BufferFormat::R_16:
      return GL_UNSIGNED_SHORT;
    case gfx::BufferFormat::RGBA_F16:
      return GL_HALF_FLOAT_OES;
    case gfx::BufferFormat::BGRX_1010102:
    case gfx::BufferFormat::RGBX_1010102:
      return GL_UNSIGNED_INT_2_10_10_10_REV;
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::UYVY_422:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

GLint DataRowLength(size_t stride, gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
      return base::checked_cast<GLint>(stride) / 2;
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRX_1010102:
    case gfx::BufferFormat::RGBX_1010102:
    case gfx::BufferFormat::BGRA_8888:
      return base::checked_cast<GLint>(stride) / 4;
    case gfx::BufferFormat::RGBA_F16:
      return base::checked_cast<GLint>(stride) / 8;
    case gfx::BufferFormat::R_8:
      return base::checked_cast<GLint>(stride);
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::UYVY_422:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

template <typename F>
std::unique_ptr<uint8_t[]> GLES2RGBData(const gfx::Size& size,
                                        size_t stride,
                                        const uint8_t* data,
                                        F const& data_to_rgb,
                                        GLenum* data_format,
                                        GLenum* data_type,
                                        GLint* data_row_length) {
  TRACE_EVENT2("gpu", "GLES2RGBData", "width", size.width(), "height",
               size.height());

  // Four-byte row alignment as specified by glPixelStorei with argument
  // GL_UNPACK_ALIGNMENT set to 4.
  size_t gles2_rgb_data_stride = (size.width() * 3 + 3) & ~3;
  std::unique_ptr<uint8_t[]> gles2_rgb_data(
      new uint8_t[gles2_rgb_data_stride * size.height()]);

  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      data_to_rgb(&data[y * stride + x * 4],
                  &gles2_rgb_data[y * gles2_rgb_data_stride + x * 3]);
    }
  }

  *data_format = GL_RGB;
  *data_type = GL_UNSIGNED_BYTE;
  *data_row_length = size.width();
  return gles2_rgb_data;
}

std::unique_ptr<uint8_t[]> GLES2RGB565Data(const gfx::Size& size,
                                           size_t stride,
                                           const uint8_t* data,
                                           GLenum* data_format,
                                           GLenum* data_type,
                                           GLint* data_row_length) {
  TRACE_EVENT2("gpu", "GLES2RGB565Data", "width", size.width(), "height",
               size.height());

  // Four-byte row alignment as specified by glPixelStorei with argument
  // GL_UNPACK_ALIGNMENT set to 4.
  size_t gles2_rgb_data_stride = (size.width() * 2 + 3) & ~3;
  std::unique_ptr<uint8_t[]> gles2_rgb_data(
      new uint8_t[gles2_rgb_data_stride * size.height()]);

  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      const uint16_t* src =
          reinterpret_cast<const uint16_t*>(&data[y * stride + x * 2]);
      uint16_t* dst = reinterpret_cast<uint16_t*>(
          &gles2_rgb_data[y * gles2_rgb_data_stride + x * 2]);
      *dst = (((*src & 0x1f) >> 0) << 11) | (((*src & 0x7e0) >> 5) << 5) |
             (((*src & 0xf800) >> 11) << 5);
    }
  }

  *data_format = GL_RGB;
  *data_type = GL_UNSIGNED_SHORT_5_6_5;
  *data_row_length = size.width();
  return gles2_rgb_data;
}

std::unique_ptr<uint8_t[]> GLES2Data(const gfx::Size& size,
                                     gfx::BufferFormat format,
                                     size_t stride,
                                     const uint8_t* data,
                                     GLenum* data_format,
                                     GLenum* data_type,
                                     GLint* data_row_length) {
  TRACE_EVENT2("gpu", "GLES2Data", "width", size.width(), "height",
               size.height());

  switch (format) {
    case gfx::BufferFormat::RGBX_8888:
      return GLES2RGBData(size, stride, data,
                          [](const uint8_t* src, uint8_t* dst) {
                            dst[0] = src[0];
                            dst[1] = src[1];
                            dst[2] = src[2];
                          },
                          data_format, data_type, data_row_length);
    case gfx::BufferFormat::BGR_565:
      return GLES2RGB565Data(size, stride, data, data_format, data_type,
                             data_row_length);
    case gfx::BufferFormat::BGRX_8888:
      return GLES2RGBData(size, stride, data,
                          [](const uint8_t* src, uint8_t* dst) {
                            dst[0] = src[2];
                            dst[1] = src[1];
                            dst[2] = src[0];
                          },
                          data_format, data_type, data_row_length);
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRX_1010102:
    case gfx::BufferFormat::RGBX_1010102:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::RGBA_F16:
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88: {
      size_t gles2_data_stride =
          RowSizeForBufferFormat(size.width(), format, 0);
      if (stride == gles2_data_stride ||
          g_current_gl_driver->ext.b_GL_EXT_unpack_subimage)
        return nullptr;  // No data conversion needed

      std::unique_ptr<uint8_t[]> gles2_data(
          new uint8_t[gles2_data_stride * size.height()]);
      for (int y = 0; y < size.height(); ++y) {
        memcpy(&gles2_data[y * gles2_data_stride], &data[y * stride],
               gles2_data_stride);
      }
      *data_row_length = size.width();
      return gles2_data;
    }
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::UYVY_422:
      NOTREACHED() << gfx::BufferFormatToString(format);
      return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace

GLImageMemory::GLImageMemory(const gfx::Size& size, unsigned internalformat)
    : size_(size),
      internalformat_(internalformat),
      memory_(nullptr),
      format_(gfx::BufferFormat::RGBA_8888),
      stride_(0) {}

GLImageMemory::~GLImageMemory() {}

// static
GLImageMemory* GLImageMemory::FromGLImage(GLImage* image) {
  if (!image || image->GetType() != Type::MEMORY)
    return nullptr;
  return static_cast<GLImageMemory*>(image);
}

bool GLImageMemory::Initialize(const unsigned char* memory,
                               gfx::BufferFormat format,
                               size_t stride) {
  if (!ValidInternalFormat(internalformat_)) {
    LOG(ERROR) << "Invalid internalformat: "
               << GLEnums::GetStringEnum(internalformat_);
    return false;
  }

  if (!ValidFormat(format)) {
    LOG(ERROR) << "Invalid format: " << gfx::BufferFormatToString(format);
    return false;
  }

  if (stride < RowSizeForBufferFormat(size_.width(), format, 0) || stride & 3) {
    LOG(ERROR) << "Invalid stride: " << stride;
    return false;
  }

  DCHECK(memory);
  DCHECK(!memory_);
  memory_ = memory;
  format_ = format;
  stride_ = stride;
  return true;
}

gfx::Size GLImageMemory::GetSize() {
  return size_;
}

unsigned GLImageMemory::GetInternalFormat() {
  return internalformat_;
}

bool GLImageMemory::BindTexImage(unsigned target) {
  return false;
}

bool GLImageMemory::CopyTexImage(unsigned target) {
  TRACE_EVENT2("gpu", "GLImageMemory::CopyTexImage", "width", size_.width(),
               "height", size_.height());

  // GL_TEXTURE_EXTERNAL_OES is not a supported target.
  if (target == GL_TEXTURE_EXTERNAL_OES)
    return false;

  GLenum data_format = DataFormat(format_);
  GLenum data_type = DataType(format_);
  GLint data_row_length = DataRowLength(stride_, format_);
  std::unique_ptr<uint8_t[]> gles2_data;

  if (GLContext::GetCurrent()->GetVersionInfo()->is_es) {
    gles2_data = GLES2Data(size_, format_, stride_, memory_, &data_format,
                           &data_type, &data_row_length);
  }

  if (data_row_length != size_.width())
    glPixelStorei(GL_UNPACK_ROW_LENGTH, data_row_length);

  glTexImage2D(target, 0, TextureFormat(format_), size_.width(), size_.height(),
               0, data_format, data_type,
               gles2_data ? gles2_data.get() : memory_);

  if (data_row_length != size_.width())
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  return true;
}

bool GLImageMemory::CopyTexSubImage(unsigned target,
                                    const gfx::Point& offset,
                                    const gfx::Rect& rect) {
  TRACE_EVENT2("gpu", "GLImageMemory::CopyTexSubImage", "width", rect.width(),
               "height", rect.height());

  // GL_TEXTURE_EXTERNAL_OES is not a supported target.
  if (target == GL_TEXTURE_EXTERNAL_OES)
    return false;

  // Sub width is not supported.
  if (rect.width() != size_.width())
    return false;

  const uint8_t* data = memory_ + rect.y() * stride_;
  GLenum data_format = DataFormat(format_);
  GLenum data_type = DataType(format_);
  GLint data_row_length = DataRowLength(stride_, format_);
  std::unique_ptr<uint8_t[]> gles2_data;

  if (GLContext::GetCurrent()->GetVersionInfo()->is_es) {
    gles2_data = GLES2Data(rect.size(), format_, stride_, data, &data_format,
                           &data_type, &data_row_length);
  }

  if (data_row_length != rect.width())
    glPixelStorei(GL_UNPACK_ROW_LENGTH, data_row_length);

  glTexSubImage2D(target, 0, offset.x(), offset.y(), rect.width(),
                  rect.height(), data_format, data_type,
                  gles2_data ? gles2_data.get() : data);

  if (data_row_length != rect.width())
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  return true;
}

bool GLImageMemory::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int z_order,
    gfx::OverlayTransform transform,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  return false;
}

GLImageMemory::Type GLImageMemory::GetType() const {
  return Type::MEMORY;
}

// static
unsigned GLImageMemory::GetInternalFormatForTesting(gfx::BufferFormat format) {
  DCHECK(ValidFormat(format));
  return TextureFormat(format);
}

// static
bool GLImageMemory::ValidFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRX_1010102:
    case gfx::BufferFormat::RGBX_1010102:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::RGBA_F16:
      return true;
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::UYVY_422:
      return false;
  }

  NOTREACHED();
  return false;
}

}  // namespace gl
