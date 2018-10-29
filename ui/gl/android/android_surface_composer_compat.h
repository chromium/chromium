// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANDROID_ANDROID_SURFACE_COMPOSER_COMPAT_H_
#define UI_GL_ANDROID_ANDROID_SURFACE_COMPOSER_COMPAT_H_

#include <memory>

#include <android/hardware_buffer.h>
#include <android/native_window.h>

#include "base/files/scoped_file.h"
#include "ui/gl/gl_export.h"

extern "C" {
typedef struct ASurface ASurface;
typedef struct ASurfaceComposer ASurfaceComposer;
typedef struct ASurfaceTransaction ASurfaceTransaction;
}

namespace gl {

class GL_EXPORT SurfaceComposer {
 public:
  enum class SurfaceContentType : int32_t {
    kNone = 0,
    kAHardwareBuffer = 1,
  };

  class GL_EXPORT Surface {
   public:
    Surface();
    Surface(SurfaceComposer* composer,
            SurfaceContentType content_type,
            const char* name,
            Surface* parent = nullptr);
    ~Surface();

    Surface(Surface&& other);
    Surface& operator=(Surface&& other);

    ASurface* surface() const { return surface_; }

   private:
    ASurface* surface_ = nullptr;
  };

  class GL_EXPORT Transaction {
   public:
    Transaction();
    ~Transaction();

    void SetVisibility(const Surface& surface, bool show);
    void SetPosition(const Surface& surface, float x, float y);
    void SetZOrder(const Surface& surface, int32_t z);
    void SetBuffer(const Surface& surface,
                   AHardwareBuffer* buffer,
                   base::ScopedFD fence_fd);
    void SetSize(const Surface& surface, uint32_t width, uint32_t height);
    void SetCropRect(const Surface& surface,
                     int32_t left,
                     int32_t top,
                     int32_t right,
                     int32_t bottom);
    void SetOpaque(const Surface& surface, bool opaque);
    void Apply();

   private:
    ASurfaceTransaction* transaction_;
  };

  static bool IsSupported();

  static std::unique_ptr<SurfaceComposer> Create(ANativeWindow* window);
  ~SurfaceComposer();

 private:
  explicit SurfaceComposer(ASurfaceComposer* composer);

  ASurfaceComposer* composer_;
};

};  // namespace gl

#endif  // UI_GL_ANDROID_ANDROID_SURFACE_COMPOSER_COMPAT_H_
