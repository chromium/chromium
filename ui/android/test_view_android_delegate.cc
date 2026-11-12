// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/test_view_android_delegate.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "ui/android/view_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_javatest_jni_headers/TestViewAndroidDelegate_jni.h"

namespace ui {

TestViewAndroidDelegate::TestViewAndroidDelegate() {}
TestViewAndroidDelegate::~TestViewAndroidDelegate() {}

void TestViewAndroidDelegate::SetupTestDelegate(ui::ViewAndroid* view_android) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto test_delegate = Java_TestViewAndroidDelegate_create(env);
  view_android->SetDelegate(test_delegate);
  j_delegate_.Reset(env, test_delegate);
}

void TestViewAndroidDelegate::InsetViewportBottom(int bottom) {
  CHECK(j_delegate_);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TestViewAndroidDelegate_insetViewportBottom(env, j_delegate_, bottom);
}

int TestViewAndroidDelegate::GetLastCursorType() const {
  CHECK(j_delegate_);
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_TestViewAndroidDelegate_getLastCursorType(env, j_delegate_);
}

gfx::Size TestViewAndroidDelegate::GetLastCustomCursorSize() const {
  CHECK(j_delegate_);
  JNIEnv* env = base::android::AttachCurrentThread();
  return gfx::Size(
      Java_TestViewAndroidDelegate_getLastCustomCursorWidth(env, j_delegate_),
      Java_TestViewAndroidDelegate_getLastCustomCursorHeight(env, j_delegate_));
}

gfx::Point TestViewAndroidDelegate::GetLastCustomCursorHotspot() const {
  CHECK(j_delegate_);
  JNIEnv* env = base::android::AttachCurrentThread();
  return gfx::Point(Java_TestViewAndroidDelegate_getLastCustomCursorHotspotX(
                        env, j_delegate_),
                    Java_TestViewAndroidDelegate_getLastCustomCursorHotspotY(
                        env, j_delegate_));
}

void TestViewAndroidDelegate::SetContainerLocation(
    const gfx::Point& window_location,
    const gfx::Point& screen_location) {
  CHECK(j_delegate_);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TestViewAndroidDelegate_setContainerLocation(
      env, j_delegate_, window_location.x(), window_location.y(),
      screen_location.x(), screen_location.y());
}

void TestViewAndroidDelegate::ClearContainerView() {
  CHECK(j_delegate_);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TestViewAndroidDelegate_clearContainerView(env, j_delegate_);
}

}  // namespace ui

DEFINE_JNI(TestViewAndroidDelegate)
