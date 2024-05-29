// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/android/scoped_java_surface_control.h"

#include <utility>

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/gl/gl_jni_headers/ScopedJavaSurfaceControl_jni.h"

namespace gl {

ScopedJavaSurfaceControl::ScopedJavaSurfaceControl() = default;

ScopedJavaSurfaceControl::ScopedJavaSurfaceControl(
    const base::android::JavaRef<jobject>& j_surface_control,
    bool release_on_destroy)
    : j_surface_control_(j_surface_control),
      release_on_destroy_(release_on_destroy) {}

ScopedJavaSurfaceControl::~ScopedJavaSurfaceControl() {
  DestroyIfNeeded();
}

ScopedJavaSurfaceControl::ScopedJavaSurfaceControl(
    ScopedJavaSurfaceControl&& other)
    : j_surface_control_(std::move(other.j_surface_control_)),
      release_on_destroy_(other.release_on_destroy_) {
  other.release_on_destroy_ = false;
}

ScopedJavaSurfaceControl& ScopedJavaSurfaceControl::operator=(
    ScopedJavaSurfaceControl&& other) {
  if (this != &other) {
    DestroyIfNeeded();
    j_surface_control_ = std::move(other.j_surface_control_);
    release_on_destroy_ = other.release_on_destroy_;
    other.release_on_destroy_ = false;
  }
  return *this;
}

ScopedJavaSurfaceControl ScopedJavaSurfaceControl::CopyRetainOwnership() const {
  return ScopedJavaSurfaceControl(j_surface_control_, false);
}

base::android::ScopedJavaGlobalRef<jobject>
ScopedJavaSurfaceControl::TakeJavaSurfaceControl(bool& release_on_destroy) {
  release_on_destroy = release_on_destroy_;
  release_on_destroy_ = false;
  return std::move(j_surface_control_);
}

scoped_refptr<gfx::SurfaceControl::Surface>
ScopedJavaSurfaceControl::MakeSurface() {
  if (!j_surface_control_)
    return nullptr;
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::MakeRefCounted<gfx::SurfaceControl::Surface>(env,
                                                            j_surface_control_);
}

void ScopedJavaSurfaceControl::DestroyIfNeeded() {
  if (release_on_destroy_ && j_surface_control_) {
    Java_ScopedJavaSurfaceControl_releaseSurfaceControl(
        base::android::AttachCurrentThread(), j_surface_control_);
  }
  j_surface_control_.Reset();
  release_on_destroy_ = false;
}

}  // namespace gl
