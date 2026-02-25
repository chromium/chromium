// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/accelerator_manager_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "ui/android/window_android.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/event.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/AcceleratorManager_jni.h"

namespace ui {

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

AcceleratorManagerAndroid* AcceleratorManagerAndroid::FromWindow(
    WindowAndroid& window) {
  JNIEnv* env = AttachCurrentThread();
  return reinterpret_cast<AcceleratorManagerAndroid*>(
      Java_AcceleratorManager_getNativePointerFromWindow(
          env, window.GetJavaObject()));
}

AcceleratorManagerAndroid::AcceleratorManagerAndroid(
    base::android::ScopedJavaGlobalRef<jobject> obj)
    : java_ref_(std::move(obj)) {}

AcceleratorManagerAndroid::~AcceleratorManagerAndroid() = default;

void AcceleratorManagerAndroid::RegisterAccelerator(
    const Accelerator& accelerator,
    AcceleratorManager::HandlerPriority priority,
    AcceleratorTarget* target) {
  bool first = accelerator_manager_.IsEmpty();
  accelerator_manager_.RegisterAccelerator(accelerator, priority, target);
  if (first) {
    auto* env = AttachCurrentThread();
    Java_AcceleratorManager_acceleratorsAreRegistered(env, java_ref_, true);
  }
}

void AcceleratorManagerAndroid::UnregisterAccelerator(
    const Accelerator& accelerator,
    AcceleratorTarget* target) {
  accelerator_manager_.Unregister(accelerator, target);
  if (accelerator_manager_.IsEmpty()) {
    auto* env = AttachCurrentThread();
    Java_AcceleratorManager_acceleratorsAreRegistered(env, java_ref_, false);
  }
}

void AcceleratorManagerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

bool AcceleratorManagerAndroid::ProcessKeyEvent(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_key_event) {
  KeyEventAndroid key_event_android(j_key_event);
  ui::PlatformEvent native_event(key_event_android);
  ui::KeyEvent key_event(native_event);
  ui::Accelerator accelerator(key_event);
  return accelerator_manager_.Process(accelerator);
}

int64_t JNI_AcceleratorManager_Init(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj) {
  return reinterpret_cast<int64_t>(new AcceleratorManagerAndroid(
      base::android::ScopedJavaGlobalRef<jobject>(env, obj)));
}

}  // namespace ui

DEFINE_JNI(AcceleratorManager)
