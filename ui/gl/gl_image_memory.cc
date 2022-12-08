// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_memory.h"

#include <stdint.h>

#include "base/barrier_closure.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

using gfx::BufferFormat;

namespace gl {
namespace {

GLint DataRowLength(size_t stride, gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
      return base::checked_cast<GLint>(stride) / 2;
    case gfx::BufferFormat::RG_1616:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRA_1010102:
    case gfx::BufferFormat::RGBA_1010102:
    case gfx::BufferFormat::BGRA_8888:
      return base::checked_cast<GLint>(stride) / 4;
    case gfx::BufferFormat::RGBA_F16:
      return base::checked_cast<GLint>(stride) / 8;
    case gfx::BufferFormat::R_8:
      return base::checked_cast<GLint>(stride);
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
    case gfx::BufferFormat::P010:
      NOTREACHED() << gfx::BufferFormatToString(format);
      return 0;
  }

  NOTREACHED();
  return 0;
}

template <typename F>
std::vector<uint8_t> GLES2RGBData(const gfx::Size& size,
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
  std::vector<uint8_t> gles2_rgb_data(gles2_rgb_data_stride * size.height());

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

std::vector<uint8_t> GLES2RGB565Data(const gfx::Size& size,
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
  std::vector<uint8_t> gles2_rgb_data(gles2_rgb_data_stride * size.height());

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

absl::optional<std::vector<uint8_t>> GLES2Data(const gfx::Size& size,
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
      return absl::make_optional(GLES2RGBData(
          size, stride, data,
          [](const uint8_t* src, uint8_t* dst) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
          },
          data_format, data_type, data_row_length));
    case gfx::BufferFormat::BGR_565:
      return absl::make_optional(GLES2RGB565Data(
          size, stride, data, data_format, data_type, data_row_length));
    case gfx::BufferFormat::BGRX_8888:
      return absl::make_optional(GLES2RGBData(
          size, stride, data,
          [](const uint8_t* src, uint8_t* dst) {
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
          },
          data_format, data_type, data_row_length));
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRA_1010102:
    case gfx::BufferFormat::RGBA_1010102:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::RGBA_F16:
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::RG_1616: {
      size_t gles2_data_stride =
          RowSizeForBufferFormat(size.width(), format, 0);
      if (stride == gles2_data_stride ||
          g_current_gl_driver->ext.b_GL_EXT_unpack_subimage)
        return absl::nullopt;  // No data conversion needed

      std::vector<uint8_t> gles2_data(gles2_data_stride * size.height());
      for (int y = 0; y < size.height(); ++y) {
        memcpy(&gles2_data[y * gles2_data_stride], &data[y * stride],
               gles2_data_stride);
      }
      *data_row_length = size.width();
      return absl::make_optional(gles2_data);
    }
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
    case gfx::BufferFormat::P010:
      NOTREACHED() << gfx::BufferFormatToString(format);
      return absl::nullopt;
  }

  NOTREACHED();
  return absl::nullopt;
}

void MemcpyTask(const void* src,
                void* dst,
                size_t bytes,
                size_t task_index,
                size_t n_tasks,
                base::RepeatingClosure* done) {
  auto checked_bytes = base::CheckedNumeric<size_t>(bytes);
  size_t start = (checked_bytes * task_index / n_tasks).ValueOrDie();
  size_t end = (checked_bytes * (task_index + 1) / n_tasks).ValueOrDie();
  DCHECK_LE(start, bytes);
  DCHECK_LE(end, bytes);
  memcpy(static_cast<char*>(dst) + start, static_cast<const char*>(src) + start,
         end - start);
  done->Run();
}

bool SupportsPBO(GLContext* context) {
  const GLVersionInfo* version = context->GetVersionInfo();
  return version->IsAtLeastGL(2, 1) || version->IsAtLeastGLES(3, 0) ||
         context->HasExtension("GL_ARB_pixel_buffer_object") ||
         context->HasExtension("GL_EXT_pixel_buffer_object") ||
         context->HasExtension("GL_NV_pixel_buffer_object");
}

bool SupportsMapBuffer(GLContext* context) {
  return context->GetVersionInfo()->IsAtLeastGL(2, 0) ||
         context->HasExtension("GL_OES_mapbuffer");
}

bool SupportsMapBufferRange(GLContext* context) {
  return context->GetVersionInfo()->IsAtLeastGLES(3, 0) ||
         context->HasExtension("GL_EXT_map_buffer_range");
}

}  // namespace

GLImageMemory::GLImageMemory(const gfx::Size& size)
    : size_(size),
      memory_(nullptr),
      format_(gfx::BufferFormat::RGBA_8888),
      stride_(0) {}

GLImageMemory::~GLImageMemory() {
  if (buffer_ && original_context_ && original_surface_) {
    ui::ScopedMakeCurrent make_current(original_context_.get(),
                                       original_surface_.get());
    glDeleteBuffersARB(1, &buffer_);
  }
}

bool GLImageMemory::Initialize(const unsigned char* memory,
                               gfx::BufferFormat format,
                               size_t stride,
                               bool disable_pbo_upload) {
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

#if BUILDFLAG(IS_WIN)
  // CopyTexImage from PBO is slow on Windows.
  disable_pbo_upload = true;
#endif  // BUILDFLAG(IS_WIN)

  if (disable_pbo_upload)
    return true;

  GLContext* context = GLContext::GetCurrent();
  DCHECK(context);
  if (SupportsPBO(context) &&
      (SupportsMapBuffer(context) || SupportsMapBufferRange(context))) {
    constexpr size_t kTaskBytes = 1024 * 1024;
    buffer_bytes_ = stride * size_.height();
    memcpy_tasks_ = std::min<size_t>(buffer_bytes_ / kTaskBytes,
                                     base::SysInfo::NumberOfProcessors());
    if (memcpy_tasks_ > 1) {
      glGenBuffersARB(1, &buffer_);
      ScopedBufferBinder binder(GL_PIXEL_UNPACK_BUFFER, buffer_);
      glBufferData(GL_PIXEL_UNPACK_BUFFER, buffer_bytes_, nullptr,
                   GL_DYNAMIC_DRAW);
      original_context_ = context->AsWeakPtr();
      GLSurface* surface = GLSurface::GetCurrent();
      DCHECK(surface);
      original_surface_ = surface->AsWeakPtr();
    }
  }

  return true;
}

gfx::Size GLImageMemory::GetSize() {
  return size_;
}

unsigned GLImageMemory::GetInternalFormat() {
  return gl::BufferFormatToGLInternalFormat(format_);
}

unsigned GLImageMemory::GetDataFormat() {
  switch (format_) {
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBA_1010102:
      return GL_RGBA;
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRA_1010102:
      return GL_BGRA_EXT;
    default:
      break;
  }
  return GLImage::GetDataFormat();
}

unsigned GLImageMemory::GetDataType() {
  switch (format_) {
    case gfx::BufferFormat::BGR_565:
      return GL_UNSIGNED_SHORT_5_6_5_REV;
    default:
      break;
  }
  return gl::BufferFormatToGLDataType(format_);
}

GLImage::BindOrCopy GLImageMemory::ShouldBindOrCopy() {
  return COPY;
}

bool GLImageMemory::BindTexImage(unsigned target) {
  NOTREACHED();
  return false;
}

bool GLImageMemory::CopyTexImage(unsigned target) {
  TRACE_EVENT2("gpu", "GLImageMemory::CopyTexImage", "width", size_.width(),
               "height", size_.height());

  if (target == GL_TEXTURE_EXTERNAL_OES)
    return false;

  GLenum data_format = GetDataFormat();
  GLenum data_type = GetDataType();
  GLint data_row_length = DataRowLength(stride_, format_);
  absl::optional<std::vector<uint8_t>> gles2_data;

  GLContext* context = GLContext::GetCurrent();
  DCHECK(context);
  if (context->GetVersionInfo()->is_es) {
    gles2_data = GLES2Data(size_, format_, stride_, memory_, &data_format,
                           &data_type, &data_row_length);
  }

  ScopedPixelStore scoped_unpack_row_length(
      GL_UNPACK_ROW_LENGTH,
      data_row_length == size_.width() ? 0 : data_row_length);
  ScopedPixelStore scoped_unpack_skip_pixels(GL_UNPACK_SKIP_PIXELS, 0);
  ScopedPixelStore scoped_unpack_skip_rows(GL_UNPACK_SKIP_ROWS, 0);
  ScopedPixelStore scoped_unpack_alignment(GL_UNPACK_ALIGNMENT, 4);

  const void* src;
  size_t size;
  if (gles2_data) {
    src = gles2_data->data();
    size = gles2_data->size();
  } else {
    src = memory_;
    size = buffer_bytes_;
  }

  bool uploaded = false;
  if (buffer_ && original_context_.get() == context) {
    glTexImage2D(target, 0, GetInternalFormat(), size_.width(), size_.height(),
                 0, data_format, data_type, nullptr);

    ScopedBufferBinder binder(GL_PIXEL_UNPACK_BUFFER, buffer_);

    void* dst = nullptr;
    if (SupportsMapBuffer(context)) {
      dst = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    } else {
      DCHECK(SupportsMapBufferRange(context));
      dst = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, size, GL_MAP_WRITE_BIT);
    }

    if (dst) {
      base::WaitableEvent event;
      base::RepeatingClosure barrier = base::BarrierClosure(
          memcpy_tasks_, base::BindOnce(&base::WaitableEvent::Signal,
                                        base::Unretained(&event)));
      for (int i = 1; i < memcpy_tasks_; ++i) {
        base::ThreadPool::PostTask(
            FROM_HERE, base::BindOnce(&MemcpyTask, src, dst, size, i,
                                      memcpy_tasks_, &barrier));
      }
      MemcpyTask(src, dst, size, 0, memcpy_tasks_, &barrier);
      event.Wait();

      glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

      glTexSubImage2D(target, 0, 0, 0, size_.width(), size_.height(),
                      data_format, data_type, 0);
      uploaded = true;
    } else {
      glDeleteBuffersARB(1, &buffer_);
      buffer_ = 0;
    }
  }

  if (!uploaded) {
    glTexImage2D(target, 0, GetInternalFormat(), size_.width(), size_.height(),
                 0, data_format, data_type, src);
  }

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
  GLenum data_format = GetDataFormat();
  GLenum data_type = GetDataType();
  GLint data_row_length = DataRowLength(stride_, format_);
  absl::optional<std::vector<uint8_t>> gles2_data;

  if (GLContext::GetCurrent()->GetVersionInfo()->is_es) {
    gles2_data = GLES2Data(rect.size(), format_, stride_, data, &data_format,
                           &data_type, &data_row_length);
  }

  ScopedPixelStore scoped_unpack_row_length(
      GL_UNPACK_ROW_LENGTH,
      data_row_length == rect.width() ? 0 : data_row_length);
  ScopedPixelStore scoped_unpack_skip_pixels(GL_UNPACK_SKIP_PIXELS, 0);
  ScopedPixelStore scoped_unpack_skip_rows(GL_UNPACK_SKIP_ROWS, 0);
  ScopedPixelStore scoped_unpack_alignment(GL_UNPACK_ALIGNMENT, 4);

  glTexSubImage2D(target, 0, offset.x(), offset.y(), rect.width(),
                  rect.height(), data_format, data_type,
                  gles2_data ? gles2_data->data() : data);

  return true;
}

GLImageMemory::Type GLImageMemory::GetType() const {
  return Type::MEMORY;
}

// static
bool GLImageMemory::ValidFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::RG_1616:
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRA_1010102:
    case gfx::BufferFormat::RGBA_1010102:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::RGBA_F16:
      return true;
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
    case gfx::BufferFormat::P010:
      return false;
  }

  NOTREACHED();
  return false;
}

GLImageMemoryForTesting::GLImageMemoryForTesting(const gfx::Size& size)
    : GLImageMemory(size) {}

GLImageMemoryForTesting::~GLImageMemoryForTesting() = default;

}  // namespace gl
