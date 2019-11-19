// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation.h"

#include "base/android/jni_android.h"
#include "ui/gfx/gfx_jni_headers/Animation_jni.h"

using base::android::AttachCurrentThread;

namespace gfx {

// static
void Animation::UpdatePrefersReducedMotion() {
  // prefers_reduced_motion_ should only be modified on the UI thread.
  // TODO(crbug.com/927163): DCHECK this assertion once tests are well-behaved.

  JNIEnv* env = AttachCurrentThread();
  prefers_reduced_motion_ = Java_Animation_prefersReducedMotion(env);
}

}  // namespace gfx
