// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/fake_modal_dialog_manager_bridge.h"

#include "base/android/jni_android.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_javatest_jni_headers/FakeModalDialogManager_jni.h"

namespace ui {

// static.
std::unique_ptr<FakeModalDialogManagerBridge>
FakeModalDialogManagerBridge::CreateForTab(WindowAndroid* window,
                                           bool use_empty_java_presenter) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto fake_manager = base::WrapUnique(new FakeModalDialogManagerBridge(
      Java_FakeModalDialogManager_createForTab(env, use_empty_java_presenter),
      window));
  window->SetModalDialogManagerForTesting(fake_manager->j_fake_manager_);
  return fake_manager;
}

FakeModalDialogManagerBridge::~FakeModalDialogManagerBridge() {
  window_->SetModalDialogManagerForTesting(
      base::android::ScopedJavaLocalRef<jobject>());
}

void FakeModalDialogManagerBridge::ClickPositiveButton() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FakeModalDialogManager_clickPositiveButton(env, j_fake_manager_);
}

bool FakeModalDialogManagerBridge::IsSuspend(
    ModalDialogManagerBridge::ModalDialogType dialog_type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return static_cast<bool>(Java_FakeModalDialogManager_isSuspended(
      env, j_fake_manager_, static_cast<int>(dialog_type)));
}

// private.
FakeModalDialogManagerBridge::FakeModalDialogManagerBridge(
    base::android::ScopedJavaLocalRef<jobject> j_fake_manager,
    WindowAndroid* window)
    : j_fake_manager_(j_fake_manager), window_(window) {}

}  // namespace ui
