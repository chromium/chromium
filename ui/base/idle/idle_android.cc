// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "ui/base/idle/idle_internal.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/base/ui_base_jni_headers/IdleDetector_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaLocalRef;
using jni_zero::AttachCurrentThread;

namespace ui {

namespace {

class AndroidIdleMonitor {
 public:
  AndroidIdleMonitor() {
    JNIEnv* env = AttachCurrentThread();
    j_idle_manager_.Reset(Java_IdleDetector_create(env));
  }

  ~AndroidIdleMonitor() {}

  static AndroidIdleMonitor* GetInstance() {
    // This class is constructed a single time whenever the
    // first web page using the User Idle Detection API
    // starts monitoring the idle state.
    //
    // Upon construction, a java object is instantiated and
    // the Android broadcast receivers are registered to listen
    // to the OS events.
    //
    // The singleton only gets destroyed when the browser exists,
    // so the broadcast receivers never get unregistered. The
    // events are rare and we respond very quickly to them.
    //
    // In addition to that, Android kills chrome regularly, so
    // in practice the lifetime is reasonably scoped.
    //
    // This approach is consistent with the implementation on
    // Macs.
    return base::Singleton<AndroidIdleMonitor>::get();
  }

  int CalculateIdleTime() {
    JNIEnv* env = AttachCurrentThread();
    jlong result = Java_IdleDetector_getIdleTime(env, j_idle_manager_);
    return result;
  }

  bool CheckIdleStateIsLocked() {
    JNIEnv* env = AttachCurrentThread();
    jboolean result = Java_IdleDetector_isScreenLocked(env, j_idle_manager_);
    return result;
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_idle_manager_;
};

}  // namespace

int CalculateIdleTime() {
  return AndroidIdleMonitor::GetInstance()->CalculateIdleTime();
}

bool CheckIdleStateIsLocked() {
  if (IdleStateForTesting().has_value())
    return IdleStateForTesting().value() == IDLE_STATE_LOCKED;

  return AndroidIdleMonitor::GetInstance()->CheckIdleStateIsLocked();
}

IdleState CalculateIdleState(int idle_threshold) {
  // TODO(crbug.com/40591477): implementation pending.
  NOTIMPLEMENTED();
  return IdleState::IDLE_STATE_UNKNOWN;
}

}  // namespace ui
