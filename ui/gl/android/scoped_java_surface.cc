// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/android/scoped_java_surface.h"

#include <utility>

#include "base/check.h"
#include "ui/gl/android/surface_texture.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/gl/surface_jni_headers/Surface_jni.h"

using base::android::ScopedJavaLocalRef;

namespace gl {

ScopedJavaSurface::ScopedJavaSurface() = default;
ScopedJavaSurface::ScopedJavaSurface(std::nullptr_t) {}

ScopedJavaSurface::ScopedJavaSurface(
    const base::android::JavaRef<jobject>& surface,
    bool auto_release)
    : auto_release_(auto_release), j_surface_(surface) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  DCHECK(env->IsInstanceOf(surface.obj(), android_view_Surface_clazz(env)));
}

ScopedJavaSurface::ScopedJavaSurface(const SurfaceTexture* surface_texture) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> tmp(
      JNI_Surface::Java_Surface_Constructor__android_graphics_SurfaceTexture(
          env, surface_texture->j_surface_texture()));
  DCHECK(!tmp.is_null());
  j_surface_.Reset(tmp);
}

ScopedJavaSurface::ScopedJavaSurface(ScopedJavaSurface&& rvalue) {
  MoveFrom(rvalue);
}

ScopedJavaSurface& ScopedJavaSurface::operator=(ScopedJavaSurface&& rhs) {
  MoveFrom(rhs);
  return *this;
}

ScopedJavaSurface::~ScopedJavaSurface() {
  ReleaseSurfaceIfNeeded();
}

ScopedJavaSurface ScopedJavaSurface::CopyRetainOwnership() const {
  return ScopedJavaSurface(j_surface_, /*auto_release=*/false);
}

void ScopedJavaSurface::ReleaseSurfaceIfNeeded() {
  if (auto_release_ && !j_surface_.is_null()) {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    JNI_Surface::Java_Surface_release(env, j_surface_);
  }
}

void ScopedJavaSurface::MoveFrom(ScopedJavaSurface& other) {
  if (this == &other) {
    return;
  }
  ReleaseSurfaceIfNeeded();
  j_surface_ = std::move(other.j_surface_);
  auto_release_ = other.auto_release_;
}

bool ScopedJavaSurface::IsEmpty() const {
  return j_surface_.is_null();
}

bool ScopedJavaSurface::IsValid() const {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return !IsEmpty() && JNI_Surface::Java_Surface_isValid(env, j_surface_);
}

}  // namespace gl
