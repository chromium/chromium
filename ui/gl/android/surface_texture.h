// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANDROID_SURFACE_TEXTURE_H_
#define UI_GL_ANDROID_SURFACE_TEXTURE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "ui/gl/gl_export.h"

namespace gl {

class ScopedANativeWindow;

// This class serves as a bridge for native code to call java functions inside
// android SurfaceTexture class.
class GL_EXPORT SurfaceTexture
    : public base::RefCountedThreadSafe<SurfaceTexture> {
 public:
  static scoped_refptr<SurfaceTexture> Create(int texture_id);

  SurfaceTexture(const SurfaceTexture&) = delete;
  SurfaceTexture& operator=(const SurfaceTexture&) = delete;

  // Creates a native render surface for this surface texture.
  ScopedANativeWindow CreateSurface();

  // Release the SurfaceTexture back buffers.  The SurfaceTexture is no longer
  // usable after calling this but the front buffer is still valid. Note that
  // this is not called 'Release', like the Android API, because scoped_refptr
  // calls that quite a bit.
  void ReleaseBackBuffers();

  const base::android::JavaRef<jobject>& j_surface_texture() const {
    return j_surface_texture_;
  }

 protected:
  explicit SurfaceTexture(
      const base::android::ScopedJavaLocalRef<jobject>& j_surface_texture);

 private:
  friend class base::RefCountedThreadSafe<SurfaceTexture>;
  virtual ~SurfaceTexture();

  // Java SurfaceTexture instance.
  base::android::ScopedJavaGlobalRef<jobject> j_surface_texture_;
};

}  // namespace gl

#endif  // UI_GL_ANDROID_SURFACE_TEXTURE_H_
