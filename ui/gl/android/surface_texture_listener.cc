// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/android/surface_texture_listener.h"

#include <utility>

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/gl/gl_jni_headers/SurfaceTextureListener_jni.h"

using base::android::JavaParamRef;

namespace gl {

SurfaceTextureListener::SurfaceTextureListener(base::RepeatingClosure callback,
                                               bool use_any_thread)
    : callback_(std::move(callback)),
      browser_loop_(base::ThreadTaskRunnerHandle::Get()),
      use_any_thread_(use_any_thread) {}

SurfaceTextureListener::~SurfaceTextureListener() {
}

void SurfaceTextureListener::Destroy(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  if (!browser_loop_->DeleteSoon(FROM_HERE, this)) {
    delete this;
  }
}

void SurfaceTextureListener::FrameAvailable(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  if (!use_any_thread_ && !browser_loop_->BelongsToCurrentThread()) {
    browser_loop_->PostTask(FROM_HERE, callback_);
  } else {
    callback_.Run();
  }
}

}  // namespace gl
