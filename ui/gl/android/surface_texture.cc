// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/android/surface_texture.h"

#include "base/android/jni_android.h"
#include "ui/gl/android/scoped_a_native_window.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/gl_bindings.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/gl/gl_jni_headers/ChromeSurfaceTexture_jni.h"

#ifndef GL_ANGLE_texture_storage_external
#define GL_ANGLE_texture_storage_external 1
#define GL_TEXTURE_NATIVE_ID_ANGLE 0x3481
#endif /* GL_ANGLE_texture_storage_external */

namespace gl {

scoped_refptr<SurfaceTexture> SurfaceTexture::Create(int texture_id) {
  int native_id = texture_id;

  // ANGLE emulates texture IDs so query the native ID of the texture.
  if (texture_id != 0 &&
      gl::g_current_gl_driver->ext.b_GL_ANGLE_texture_external_update) {
    GLint prev_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &prev_texture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_id);
    glGetTexParameteriv(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_NATIVE_ID_ANGLE,
                        &native_id);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, prev_texture);
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  return new SurfaceTexture(
      Java_ChromeSurfaceTexture_Constructor(env, native_id));
}

SurfaceTexture::SurfaceTexture(
    const base::android::ScopedJavaLocalRef<jobject>& j_surface_texture) {
  j_surface_texture_.Reset(j_surface_texture);
}

SurfaceTexture::~SurfaceTexture() {
  ReleaseBackBuffers();
}

ScopedANativeWindow SurfaceTexture::CreateSurface() {
  ScopedJavaSurface surface(this);
  return ScopedANativeWindow(surface);
}

void SurfaceTexture::ReleaseBackBuffers() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ChromeSurfaceTexture_destroy(env, j_surface_texture_);
}

}  // namespace gl

DEFINE_JNI(ChromeSurfaceTexture)
