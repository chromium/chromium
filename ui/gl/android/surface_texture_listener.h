// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANDROID_SURFACE_TEXTURE_LISTENER_H_
#define UI_GL_ANDROID_SURFACE_TEXTURE_LISTENER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner_helpers.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gl {

// Listener class for all the callbacks from android SurfaceTexture.
class SurfaceTextureListener {
 public:
  SurfaceTextureListener() = delete;
  SurfaceTextureListener(const SurfaceTextureListener&) = delete;
  SurfaceTextureListener& operator=(const SurfaceTextureListener&) = delete;

  // Destroy this listener.
  void Destroy(JNIEnv* env);

  // A new frame is available to consume.
  void FrameAvailable(JNIEnv* env);

 private:
  friend class base::DeleteHelper<SurfaceTextureListener>;

  // Native code should not hold any reference to this object, and instead pass
  // it up to Java for being referenced by a SurfaceTexture instance.
  // If use_any_thread is true, then the FrameAvailable callback will happen
  // on whatever thread calls us.  Otherwise, we will call it back on the same
  // thread that was used to construct us.
  SurfaceTextureListener(base::RepeatingClosure callback, bool use_any_thread);
  ~SurfaceTextureListener();

  friend class SurfaceTexture;

  base::RepeatingClosure callback_;

  scoped_refptr<base::SingleThreadTaskRunner> browser_loop_;

  bool use_any_thread_;
};

}  // namespace gl

#endif  // UI_GL_ANDROID_SURFACE_TEXTURE_LISTENER_H_
