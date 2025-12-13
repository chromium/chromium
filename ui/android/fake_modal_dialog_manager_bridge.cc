// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/fake_modal_dialog_manager_bridge.h"

#include <vector>

#include "base/android/jni_android.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"

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

void FakeModalDialogManagerBridge::ClickNegativeButton() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FakeModalDialogManager_clickNegativeButton(env, j_fake_manager_);
}

void FakeModalDialogManagerBridge::ToggleCheckbox() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FakeModalDialogManager_toggleCheckbox(env, j_fake_manager_);
}

bool FakeModalDialogManagerBridge::IsCheckboxChecked() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return static_cast<bool>(
      Java_FakeModalDialogManager_isCheckboxChecked(env, j_fake_manager_));
}

int FakeModalDialogManagerBridge::GetButtonStyles() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_FakeModalDialogManager_getButtonStyles(env, j_fake_manager_);
}

std::vector<std::u16string>
FakeModalDialogManagerBridge::GetMessageParagraphs() {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto java_paragraphs =
      Java_FakeModalDialogManager_getMessageParagraphs(env, j_fake_manager_);
  auto paragraphs = std::vector<std::u16string>();
  base::android::AppendJavaStringArrayToStringVector(env, java_paragraphs,
                                                     &paragraphs);
  return paragraphs;
}

void FakeModalDialogManagerBridge::ClickLinkInMessageParagraphs(int index) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FakeModalDialogManager_clickLinkInMessageParagraphs(env, j_fake_manager_,
                                                           index);
}

std::vector<std::u16string> FakeModalDialogManagerBridge::GetMenuItemTexts() {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto java_texts =
      Java_FakeModalDialogManager_getMenuItemTexts(env, j_fake_manager_);
  auto texts = std::vector<std::u16string>();
  base::android::AppendJavaStringArrayToStringVector(env, java_texts, &texts);
  return texts;
}

void FakeModalDialogManagerBridge::ClickMenuItem(int index) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FakeModalDialogManager_clickMenuItem(env, j_fake_manager_, index);
}

std::vector<SkBitmap> FakeModalDialogManagerBridge::GetMenuItemIcons() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobjectArray> java_icons =
      Java_FakeModalDialogManager_getMenuItemIcons(env, j_fake_manager_);

  std::vector<SkBitmap> icons;
  if (java_icons) {
    size_t len = base::android::SafeGetArrayLength(env, java_icons);
    icons.reserve(len);
    for (size_t i = 0; i < len; ++i) {
      base::android::ScopedJavaLocalRef<jobject> java_bitmap =
          base::android::ScopedJavaLocalRef<jobject>::Adopt(
              env, env->GetObjectArrayElement(java_icons.obj(), i));
      if (java_bitmap) {
        icons.push_back(
            gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(java_bitmap)));
      } else {
        icons.emplace_back();
      }
    }
  }
  return icons;
}

SkBitmap FakeModalDialogManagerBridge::GetTitleIcon() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_bitmap =
      Java_FakeModalDialogManager_getTitleIcon(env, j_fake_manager_);
  if (java_bitmap.is_null()) {
    return SkBitmap();
  }
  return gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(java_bitmap));
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

DEFINE_JNI(FakeModalDialogManager)
