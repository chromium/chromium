// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_H_
#define UI_GL_GL_IMAGE_H_

#include <stdint.h>

#include <string>

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

#if BUILDFLAG(IS_ANDROID)
#include <android/hardware_buffer.h>
#include <memory>
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/files/scoped_file.h"
#endif

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}  // namespace trace_event

namespace android {
class ScopedHardwareBufferFenceSync;
}  // namespace android
}  // namespace base

namespace gl {

// Encapsulates an image that can be bound and/or copied to a texture, hiding
// platform specific management.
class GL_EXPORT GLImage : public base::RefCounted<GLImage> {
 public:
  GLImage() = default;

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
  const gfx::ColorSpace& color_space() const { return color_space_; }

  // Flush any preceding rendering for the image.
  virtual void Flush();

  // Dumps information about the memory backing the GLImage to a dump named
  // |dump_name|.
  virtual void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                            uint64_t process_tracing_id,
                            const std::string& dump_name);

  // If this returns true, then the command buffer client has requested a
  // CHROMIUM image with internalformat GL_RGB, but the platform only supports
  // GL_RGBA. The client is responsible for implementing appropriate
  // workarounds. The only support that the command buffer provides is format
  // validation during calls to copyTexImage2D and copySubTexImage2D.
  //
  // This is a workaround that is not intended to become a permanent part of the
  // GLImage API. Theoretically, when Apple fixes their drivers, this can be
  // removed. https://crbug.com/581777#c36
  virtual bool EmulatingRGB() const;

  // Return true if the macOS WindowServer is currently using the underlying
  // storage for the image.
  virtual bool IsInUseByWindowServer() const;

  // If called, then IsInUseByWindowServer will always return false.
  virtual void DisableInUseByWindowServer();

#if BUILDFLAG(IS_ANDROID)
  // Provides the buffer backing this image, if it is backed by an
  // AHardwareBuffer. The ScopedHardwareBuffer returned may include a fence
  // which will be signaled when all pending work for the buffer has been
  // finished and it can be safely read from.
  // The buffer is guaranteed to be valid until the lifetime of the object
  // returned.
  virtual std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer();
#endif

  // An identifier for subclasses. Necessary for safe downcasting.
  enum class Type {
    NONE,
    MEMORY,
    IOSURFACE,
    DXGI_IMAGE,
    D3D,
    DCOMP_SURFACE,
  };
  virtual Type GetType() const;

  // Returns the NativePixmap backing the GLImage. If not backed by a
  // NativePixmap, returns null.
  virtual scoped_refptr<gfx::NativePixmap> GetNativePixmap();

  virtual void* GetEGLImage() const;

 protected:
  virtual ~GLImage() = default;

  gfx::ColorSpace color_space_;

 private:
  friend class base::RefCounted<GLImage>;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_H_
