// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/android/scoped_java_surface.h"

#include "base/logging.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/surface_jni_headers/Surface_jni.h"

using base::android::ScopedJavaLocalRef;

namespace gl {

ScopedJavaSurface::ScopedJavaSurface() {
}

ScopedJavaSurface::ScopedJavaSurface(
    const base::android::JavaRef<jobject>& surface) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env->IsInstanceOf(surface.obj(), android_view_Surface_clazz(env)));
  j_surface_.Reset(surface);
}

ScopedJavaSurface::ScopedJavaSurface(const SurfaceTexture* surface_texture) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> tmp(JNI_Surface::Java_Surface_ConstructorAVS_AGST(
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

void ScopedJavaSurface::ReleaseSurfaceIfNeeded() {
  if (auto_release_ && !j_surface_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    JNI_Surface::Java_Surface_releaseV(env, j_surface_);
  }
}

void ScopedJavaSurface::MoveFrom(ScopedJavaSurface& other) {
  ReleaseSurfaceIfNeeded();
  JNIEnv* env = base::android::AttachCurrentThread();
  j_surface_.Reset(env, other.j_surface_.Release());
  auto_release_ = other.auto_release_;
  is_protected_ = other.is_protected_;
}

bool ScopedJavaSurface::IsEmpty() const {
  return j_surface_.is_null();
}

bool ScopedJavaSurface::IsValid() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return !IsEmpty() && JNI_Surface::Java_Surface_isValidZ(env, j_surface_);
}

// static
ScopedJavaSurface ScopedJavaSurface::AcquireExternalSurface(jobject surface) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> surface_ref;
  surface_ref.Reset(env, surface);
  ScopedJavaSurface scoped_surface(surface_ref);
  scoped_surface.auto_release_ = false;
  scoped_surface.is_protected_ = true;
  return scoped_surface;
}

}  // namespace gl
