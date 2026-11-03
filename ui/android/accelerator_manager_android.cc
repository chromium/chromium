// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/accelerator_manager_android.h"

#include <android/input.h>

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/android/window_android.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/event.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/AcceleratorManager_jni.h"

namespace ui {

using base::android::AttachCurrentThread;

AcceleratorManagerAndroid* AcceleratorManagerAndroid::FromWindow(
    WindowAndroid* window) {
  if (!window) {
    return nullptr;
  }
  JNIEnv* env = AttachCurrentThread();
  return Java_AcceleratorManager_getNativePointerFromWindow(
      env, window->GetJavaObject());
}

// static
std::optional<Accelerator> AcceleratorManagerAndroid::AcceleratorFromKeyEvent(
    const base::android::JavaRef<jobject>& j_key_event) {
  KeyEventAndroid key_event_android(j_key_event);
  // ui::KeyEvent will CHECK fail if the type is not one of these.
  if (key_event_android.Action() != AKEY_EVENT_ACTION_DOWN &&
      key_event_android.Action() != AKEY_EVENT_ACTION_UP) {
    return std::nullopt;
  }
  ui::PlatformEvent native_event(key_event_android);
  ui::KeyEvent key_event(native_event);
  return ui::Accelerator(key_event);
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

bool AcceleratorManagerAndroid::HasPriorityHandler(
    const Accelerator& accelerator) const {
  return accelerator_manager_.HasPriorityHandler(accelerator);
}

bool AcceleratorManagerAndroid::ProcessAccelerator(
    const Accelerator& accelerator) {
  return accelerator_manager_.Process(accelerator);
}

bool AcceleratorManagerAndroid::ProcessKeyEvent(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_key_event) {
  std::optional<Accelerator> accelerator = AcceleratorFromKeyEvent(j_key_event);
  if (!accelerator.has_value()) {
    return false;
  }
  return ProcessAccelerator(*accelerator);
}

static jni_zero::JniUniquePtr<AcceleratorManagerAndroid>
JNI_AcceleratorManager_Init(const base::android::JavaRef<jobject>& obj) {
  return jni_zero::MakeUnique<AcceleratorManagerAndroid>(
      base::android::ScopedJavaGlobalRef<jobject>(obj));
}

}  // namespace ui

DEFINE_JNI(AcceleratorManager)
