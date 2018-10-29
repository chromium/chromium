// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_H_
#define UI_GL_GL_IMAGE_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gl/gl_export.h"

#if defined(OS_ANDROID)
#include <android/hardware_buffer.h>
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/files/scoped_file.h"
#endif

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}

namespace gfx {
class GpuFence;
}

namespace gl {

// Encapsulates an image that can be bound and/or copied to a texture, hiding
// platform specific management.
class GL_EXPORT GLImage : public base::RefCounted<GLImage> {
 public:
  GLImage() {}

  // Get the size of the image.
  virtual gfx::Size GetSize() = 0;

  // Get the internal format of the image.
  virtual unsigned GetInternalFormat() = 0;

  // Bind image to texture currently bound to |target|. Returns true on success.
  // It is valid for an implementation to always return false.
  virtual bool BindTexImage(unsigned target) = 0;

  // Bind image to texture currently bound to |target|, forcing the texture's
  // internal format to the specified one. This is a feature not available on
  // all platforms. Returns true on success.  It is valid for an implementation
  // to always return false.
  virtual bool BindTexImageWithInternalformat(unsigned target,
                                              unsigned internalformat);

  // Release image from texture currently bound to |target|.
  virtual void ReleaseTexImage(unsigned target) = 0;

  // Define texture currently bound to |target| by copying image into it.
  // Returns true on success. It is valid for an implementation to always
  // return false.
  virtual bool CopyTexImage(unsigned target) = 0;

  // Copy |rect| of image to |offset| in texture currently bound to |target|.
  // Returns true on success. It is valid for an implementation to always
  // return false.
  virtual bool CopyTexSubImage(unsigned target,
                               const gfx::Point& offset,
                               const gfx::Rect& rect) = 0;

  // Schedule image as an overlay plane to be shown at swap time for |widget|.
  virtual bool ScheduleOverlayPlane(
      gfx::AcceleratedWidget widget,
      int z_order,
      gfx::OverlayTransform transform,
      const gfx::Rect& bounds_rect,
      const gfx::RectF& crop_rect,
      bool enable_blend,
      std::unique_ptr<gfx::GpuFence> gpu_fence) = 0;

  // Set the color space when image is used as an overlay.
  virtual void SetColorSpace(const gfx::ColorSpace& color_space) = 0;

  // Flush any preceding rendering for the image.
  virtual void Flush() = 0;

  // Dumps information about the memory backing the GLImage to a dump named
  // |dump_name|.
  virtual void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                            uint64_t process_tracing_id,
                            const std::string& dump_name) = 0;

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

#if defined(OS_ANDROID)
  class GL_EXPORT ScopedHardwareBuffer {
   public:
    ScopedHardwareBuffer(base::android::ScopedHardwareBufferHandle handle,
                         base::ScopedFD fence_fd);
    virtual ~ScopedHardwareBuffer();

    AHardwareBuffer* buffer() const { return handle_.get(); }
    base::ScopedFD TakeFence();

   private:
    base::android::ScopedHardwareBufferHandle handle_;
    base::ScopedFD fence_fd_;
  };

  // Provides the buffer backing this image, if it is backed by an
  // AHardwareBuffer. The ScopedHardwareBuffer returned may include a fence
  // which will be signaled when all pending work for the buffer has been
  // finished and it can be safely read from.
  // The buffer is guaranteed to be valid until the lifetime of the object
  // returned.
  virtual std::unique_ptr<ScopedHardwareBuffer> GetAHardwareBuffer();
#endif

  // An identifier for subclasses. Necessary for safe downcasting.
  enum class Type { NONE, MEMORY, IOSURFACE, DXGI_IMAGE };
  virtual Type GetType() const;

 protected:
  virtual ~GLImage() {}

 private:
  friend class base::RefCounted<GLImage>;

  DISALLOW_COPY_AND_ASSIGN(GLImage);
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_H_
