// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/drag_event_android.h"

#include <memory>

#include "base/android/jni_android.h"

using base::android::ScopedJavaLocalRef;
using base::android::AttachCurrentThread;

namespace ui {

DragEventAndroid::DragEventAndroid(
    JNIEnv* env,
    int action,
    const gfx::PointF& location,
    const gfx::PointF& screen_location,
    const std::vector<std::u16string>& mime_types,
    jstring content,
    jobjectArray filenames,
    jstring text,
    jstring html,
    jstring url)
    : action_(action),
      location_(location),
      screen_location_(screen_location),
      mime_types_(mime_types) {
  content_.Reset(env, content);
  filenames_.Reset(env, filenames);
  text_.Reset(env, text);
  html_.Reset(env, html);
  url_.Reset(env, url);
}

DragEventAndroid::~DragEventAndroid() {}

ScopedJavaLocalRef<jstring> DragEventAndroid::GetJavaContent() const {
  return ScopedJavaLocalRef<jstring>(content_);
}

ScopedJavaLocalRef<jobjectArray> DragEventAndroid::GetJavaFilenames() const {
  return ScopedJavaLocalRef<jobjectArray>(filenames_);
}

ScopedJavaLocalRef<jstring> DragEventAndroid::GetJavaText() const {
  return ScopedJavaLocalRef<jstring>(text_);
}

ScopedJavaLocalRef<jstring> DragEventAndroid::GetJavaHtml() const {
  return ScopedJavaLocalRef<jstring>(html_);
}

ScopedJavaLocalRef<jstring> DragEventAndroid::GetJavaUrl() const {
  return ScopedJavaLocalRef<jstring>(url_);
}

std::unique_ptr<DragEventAndroid> DragEventAndroid::CreateFor(
    const gfx::PointF& new_location) const {
  gfx::PointF new_screen_location =
      new_location + (screen_location() - location());
  JNIEnv* env = AttachCurrentThread();
  return std::make_unique<DragEventAndroid>(
      env, action_, new_location, new_screen_location, *mime_types_,
      content_.obj(), filenames_.obj(), text_.obj(), html_.obj(), url_.obj());
}

}  // namespace ui
