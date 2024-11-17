// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/android/surface_texture.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/debug/crash_logging.h"
#include "ui/gl/android/scoped_a_native_window.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture_listener.h"
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

void SurfaceTexture::SetFrameAvailableCallback(
    base::RepeatingClosure callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ChromeSurfaceTexture_setNativeListener(
      env, j_surface_texture_,
      reinterpret_cast<intptr_t>(
          new SurfaceTextureListener(std::move(callback), false)));
}

void SurfaceTexture::SetFrameAvailableCallbackOnAnyThread(
    base::RepeatingClosure callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ChromeSurfaceTexture_setNativeListener(
      env, j_surface_texture_,
      reinterpret_cast<intptr_t>(
          new SurfaceTextureListener(std::move(callback), true)));
}

void SurfaceTexture::UpdateTexImage() {
  static auto* kCrashKey = base::debug::AllocateCrashKeyString(
      "inside_surface_texture_update_tex_image",
      base::debug::CrashKeySize::Size256);
  base::debug::ScopedCrashKeyString scoped_crash_key(kCrashKey, "1");
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ChromeSurfaceTexture_updateTexImage(env, j_surface_texture_);

  // Notify ANGLE that the External texture binding has changed
  if (gl::g_current_gl_driver->ext.b_GL_ANGLE_texture_external_update)
    glInvalidateTextureANGLE(GL_TEXTURE_EXTERNAL_OES);
}

void SurfaceTexture::GetTransformMatrix(base::span<float, 16> mtx) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jfloatArray> jmatrix(
      env, env->NewFloatArray(16));
  Java_ChromeSurfaceTexture_getTransformMatrix(env, j_surface_texture_,
                                               jmatrix);

  jfloat* elements = env->GetFloatArrayElements(jmatrix.obj(), nullptr);
  for (int i = 0; i < 16; ++i) {
    // SAFETY: required from Android API.
    mtx[i] = static_cast<float>(UNSAFE_BUFFERS(elements[i]));
  }
  env->ReleaseFloatArrayElements(jmatrix.obj(), elements, JNI_ABORT);
}

void SurfaceTexture::AttachToGLContext() {
  // ANGLE emulates texture IDs so query the native ID of the texture.
  int texture_id = 0;
  if (gl::g_current_gl_driver->ext.b_GL_ANGLE_texture_external_update) {
    glGetTexParameteriv(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_NATIVE_ID_ANGLE,
                        &texture_id);
  } else {
    glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &texture_id);
  }
  DCHECK(texture_id);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ChromeSurfaceTexture_attachToGLContext(env, j_surface_texture_,
                                              texture_id);

  // Notify ANGLE that the External texture binding has changed
  if (gl::g_current_gl_driver->ext.b_GL_ANGLE_texture_external_update) {
    glInvalidateTextureANGLE(GL_TEXTURE_EXTERNAL_OES);
  }
}

void SurfaceTexture::DetachFromGLContext() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ChromeSurfaceTexture_detachFromGLContext(env, j_surface_texture_);
}

ScopedANativeWindow SurfaceTexture::CreateSurface() {
  ScopedJavaSurface surface(this);
  return ScopedANativeWindow(surface);
}

void SurfaceTexture::ReleaseBackBuffers() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ChromeSurfaceTexture_destroy(env, j_surface_texture_);
}

void SurfaceTexture::SetDefaultBufferSize(int width, int height) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ChromeSurfaceTexture_setDefaultBufferSize(env, j_surface_texture_, width,
                                                 height);
}

}  // namespace gl
