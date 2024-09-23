// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANDROID_SURFACE_TEXTURE_H_
#define UI_GL_ANDROID_SURFACE_TEXTURE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
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

  // Set the listener callback, which will be invoked on the same thread that
  // is being called from here for registration.
  // Note: Since callbacks come in from Java objects that might outlive objects
  // being referenced from the callback, the only robust way here is to create
  // the callback from a weak pointer to your object.
  void SetFrameAvailableCallback(base::RepeatingClosure callback);

  // Set the listener callback, but allow it to be invoked on any thread.  The
  // same caveats apply as SetFrameAvailableCallback, plus whatever other issues
  // show up due to multithreading (e.g., don't bind the Closure to a method
  // via a weak ref).
  void SetFrameAvailableCallbackOnAnyThread(base::RepeatingClosure callback);

  // Update the texture image to the most recent frame from the image stream.
  void UpdateTexImage();

  // Retrieve the 4x4 texture coordinate transform matrix associated with the
  // texture image set by the most recent call to updateTexImage.
  void GetTransformMatrix(base::span<float, 16> mtx);

  // Attach the SurfaceTexture to the texture currently bound to
  // GL_TEXTURE_EXTERNAL_OES.
  void AttachToGLContext();

  // Detaches the SurfaceTexture from the context that owns its current GL
  // texture. Must be called with that context current on the calling thread.
  void DetachFromGLContext();

  // Creates a native render surface for this surface texture.
  ScopedANativeWindow CreateSurface();

  // Release the SurfaceTexture back buffers.  The SurfaceTexture is no longer
  // usable after calling this but the front buffer is still valid. Note that
  // this is not called 'Release', like the Android API, because scoped_refptr
  // calls that quite a bit.
  void ReleaseBackBuffers();

  // Set the default buffer size for the surface texture.
  void SetDefaultBufferSize(int width, int height);

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
