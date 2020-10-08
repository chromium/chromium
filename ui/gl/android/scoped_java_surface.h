// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANDROID_SCOPED_JAVA_SURFACE_H_
#define UI_GL_ANDROID_SCOPED_JAVA_SURFACE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "ui/gl/gl_export.h"

namespace gl {

class SurfaceTexture;

// A helper class for holding a scoped reference to a Java Surface instance.
// When going out of scope, Surface.release() is called on the Java object to
// make sure server-side references (esp. wrt graphics memory) are released.
class GL_EXPORT ScopedJavaSurface {
 public:
  ScopedJavaSurface();

  // Wraps an existing Java Surface object in a ScopedJavaSurface.
  explicit ScopedJavaSurface(const base::android::JavaRef<jobject>& surface);

  // Creates a Java Surface from a SurfaceTexture and wraps it in a
  // ScopedJavaSurface.
  explicit ScopedJavaSurface(const SurfaceTexture* surface_texture);

  // Move constructor. Take the surface from another ScopedJavaSurface object,
  // the latter no longer owns the surface afterwards.
  ScopedJavaSurface(ScopedJavaSurface&& rvalue);
  ScopedJavaSurface& operator=(ScopedJavaSurface&& rhs);

  // Creates a ScopedJavaSurface that is owned externally, i.e.,
  // someone else is responsible to call Surface.release().
  static ScopedJavaSurface AcquireExternalSurface(
      const base::android::JavaRef<jobject>& surface);

  ~ScopedJavaSurface();

  // Checks whether the surface is an empty one.
  bool IsEmpty() const;

  // Checks whether this object references a valid surface.
  bool IsValid() const;

  // Checks whether the surface is hardware protected so that no readback is
  // possible.
  bool is_protected() const { return is_protected_; }

  const base::android::JavaRef<jobject>& j_surface() const {
    return j_surface_;
  }

 private:
  // Performs destructive move from |other| to this.
  void MoveFrom(ScopedJavaSurface& other);
  void ReleaseSurfaceIfNeeded();

  bool auto_release_ = true;
  bool is_protected_ = false;

  base::android::ScopedJavaGlobalRef<jobject> j_surface_;

  DISALLOW_COPY_AND_ASSIGN(ScopedJavaSurface);
};

}  // namespace gl

#endif  // UI_GL_ANDROID_SCOPED_JAVA_SURFACE_H_
