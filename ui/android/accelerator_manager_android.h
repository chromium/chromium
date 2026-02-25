// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_ACCELERATOR_MANAGER_ANDROID_H_
#define UI_ANDROID_ACCELERATOR_MANAGER_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "ui/android/ui_android_export.h"
#include "ui/base/accelerators/accelerator_manager.h"

namespace ui {

class WindowAndroid;

// Native bridge for AcceleratorManager.java.
class UI_ANDROID_EXPORT AcceleratorManagerAndroid {
 public:
  // Returns the AcceleratorManagerAndroid for the given WindowAndroid. Returns
  // nullptr if it doesn't exist.
  static AcceleratorManagerAndroid* FromWindow(WindowAndroid& window);

  explicit AcceleratorManagerAndroid(
      base::android::ScopedJavaGlobalRef<jobject> obj);

  AcceleratorManagerAndroid(const AcceleratorManagerAndroid&) = delete;
  AcceleratorManagerAndroid& operator=(const AcceleratorManagerAndroid&) =
      delete;

  ~AcceleratorManagerAndroid();

  void RegisterAccelerator(const Accelerator& accelerator,
                           AcceleratorManager::HandlerPriority priority,
                           AcceleratorTarget* target);

  void UnregisterAccelerator(const Accelerator& accelerator,
                             AcceleratorTarget* target);

  // Functions to be called by JNI only.

  bool ProcessKeyEvent(JNIEnv* env,
                       const base::android::JavaRef<jobject>& j_key_event);

  void Destroy(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  AcceleratorManager accelerator_manager_;
};

}  // namespace ui

#endif  // UI_ANDROID_ACCELERATOR_MANAGER_ANDROID_H_
