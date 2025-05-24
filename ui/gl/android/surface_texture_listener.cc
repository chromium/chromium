// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/android/surface_texture_listener.h"

#include <utility>

#include "base/location.h"
#include "base/task/single_thread_task_runner.h"

namespace gl {

SurfaceTextureListener::SurfaceTextureListener(base::RepeatingClosure callback,
                                               bool use_any_thread)
    : callback_(std::move(callback)),
      browser_loop_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      use_any_thread_(use_any_thread) {}

SurfaceTextureListener::~SurfaceTextureListener() {
}

void SurfaceTextureListener::Destroy(JNIEnv* env) {
  if (!browser_loop_->DeleteSoon(FROM_HERE, this)) {
    delete this;
  }
}

void SurfaceTextureListener::FrameAvailable(JNIEnv* env) {
  if (!use_any_thread_ && !browser_loop_->BelongsToCurrentThread()) {
    browser_loop_->PostTask(FROM_HERE, callback_);
  } else {
    callback_.Run();
  }
}

}  // namespace gl
