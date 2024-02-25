// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANDROID_SCOPED_JAVA_SURFACE_CONTROL_H_
#define UI_GL_ANDROID_SCOPED_JAVA_SURFACE_CONTROL_H_

#include <jni.h>

#include <cstddef>
#include <optional>

#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gl/gl_export.h"

namespace gl {

// Move-only class that wraps a Java SurfaceControl and optionally call
// SurfaceControl.release on destruction.
class GL_EXPORT ScopedJavaSurfaceControl {
 public:
  ScopedJavaSurfaceControl();
  ScopedJavaSurfaceControl(
      const base::android::JavaRef<jobject>& j_surface_control,
      bool release_on_destroy);
  ~ScopedJavaSurfaceControl();

  ScopedJavaSurfaceControl(ScopedJavaSurfaceControl&& other);
  ScopedJavaSurfaceControl& operator=(ScopedJavaSurfaceControl&& other);

  // Move only type.
  ScopedJavaSurfaceControl(const ScopedJavaSurfaceControl&) = delete;
  ScopedJavaSurfaceControl& operator=(const ScopedJavaSurfaceControl&) = delete;

  explicit operator bool() const { return !!j_surface_control_; }

  // Make a copy that does not retain ownership. Client is responsible for not
  // using the copy after this is destroyed.
  ScopedJavaSurfaceControl CopyRetainOwnership() const;

  // If `release_on_destroy` arg is true, the caller is responsible for calling
  // SurfaceControl.release.
  base::android::ScopedJavaGlobalRef<jobject> TakeJavaSurfaceControl(
      bool& release_on_destroy);

  scoped_refptr<gfx::SurfaceControl::Surface> MakeSurface();

 private:
  void DestroyIfNeeded();

  base::android::ScopedJavaGlobalRef<jobject> j_surface_control_;
  bool release_on_destroy_ = false;
};

}  // namespace gl

#endif  // UI_GL_ANDROID_SCOPED_JAVA_SURFACE_CONTROL_H_
